/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
 * Copyright © 2025, Gustav Oechler <gustavoechler@gmail.com>
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Fooyin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Fooyin.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "audioengine.h"

#include "audioanalysisbus.h"
#include "audiopipeline.h"
#include "audioutils.h"
#include "core/engine/pipeline/audiostream.h"
#include "decodingcontroller.h"
#include "dsp/dspregistry.h"
#include "enginehelpers.h"
#include "internalcoresettings.h"
#include "nexttrackpreparer.h"
#include "output/fadecontroller.h"
#include "output/outputcontroller.h"
#include "output/postprocessor/replaygainprocessor.h"
#include "playbackintentreducer.h"
#include "playbacktransitioncoordinator.h"
#include "seekplanner.h"
#include "trackloadplanner.h"

#include <core/coresettings.h>
#include <core/engine/audioloader.h>
#include <core/playlist/playlist.h>
#include <utils/enum.h>
#include <utils/settings/settingsmanager.h>

#include <QCoreApplication>
#include <QEvent>
#include <QLoggingCategory>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <array>
#include <atomic>
#include <limits>
#include <string_view>
#include <utility>

Q_LOGGING_CATEGORY(ENGINE, "fy.engine")

namespace {
constexpr auto BasePrefillDecodeFrames      = 4096;
constexpr auto MaxPrefillDecodeFrames       = 262144;
constexpr auto PrefillChunkDurationMs       = 10;
constexpr auto PositionSyncTimerCoalesceKey = 0;
constexpr auto GaplessPrepareLeadMs         = 300;
constexpr auto MaxCrossfadePrefillMs        = 1000;
constexpr auto GaplessHandoffPrefillMs      = 250;
constexpr auto GaplessBoundaryWatchdogMinMs = 250;
constexpr auto GaplessBoundaryWatchdogMaxMs = 2000;
constexpr auto DecodeHighWatermarkMaxRatio  = 0.99;
constexpr auto DecodeWatermarkMinHeadroomMs = 20;
constexpr auto DecodeWatermarkMinGapMs      = 30;
constexpr auto PrefillSafetyMarginMs        = 100;
constexpr auto AudiblePauseDrainPollMs      = 10;
constexpr auto AudiblePauseDrainThresholdMs = 5;
constexpr auto AudiblePauseWatchdogMinMs    = 250;
constexpr auto AudiblePauseWatchdogMaxMs    = 1500;
constexpr auto AudiblePauseWatchdogMarginMs = 100;
constexpr auto MinLiveBitrateKbps           = 8;
constexpr auto LiveBitrateWindowMs          = 1000;

bool outputReinitRequired(const Fooyin::AudioFormat& currentOutput, const Fooyin::AudioFormat& desiredOutput)
{
    if(!currentOutput.isValid() || !desiredOutput.isValid()) {
        return false;
    }

    const auto normalisedCurrent = Fooyin::normaliseChannelLayout(currentOutput);
    const auto normalisedDesired = Fooyin::normaliseChannelLayout(desiredOutput);
    return !Fooyin::Audio::outputFormatsMatch(normalisedCurrent, normalisedDesired);
}

std::pair<double, double> sanitiseWatermarkRatios(double lowRatio, double highRatio)
{
    lowRatio  = std::clamp(lowRatio, 0.05, DecodeHighWatermarkMaxRatio);
    highRatio = std::clamp(highRatio, 0.05, DecodeHighWatermarkMaxRatio);

    if(lowRatio > highRatio) {
        std::swap(lowRatio, highRatio);
    }

    return {lowRatio, highRatio};
}

std::pair<int, int> decodeWatermarksForBufferMs(int bufferLengthMs, double lowRatio, double highRatio)
{
    const int clampedBufferMs = std::max(200, bufferLengthMs);

    const auto [safeLowRatio, safeHighRatio] = sanitiseWatermarkRatios(lowRatio, highRatio);
    const int requestedLowMs                 = std::clamp(
        static_cast<int>(std::lround(static_cast<double>(clampedBufferMs) * safeLowRatio)), 1, clampedBufferMs);
    const int requestedHighMs
        = std::clamp(static_cast<int>(std::lround(static_cast<double>(clampedBufferMs) * safeHighRatio)),
                     requestedLowMs, clampedBufferMs);

    const int minLowMs = std::clamp(100, 1, clampedBufferMs);
    const int maxHighByRatioMs
        = std::max(1, static_cast<int>(std::floor(static_cast<double>(clampedBufferMs) * DecodeHighWatermarkMaxRatio)));
    const int maxHighByHeadroomMs = std::max(1, clampedBufferMs - DecodeWatermarkMinHeadroomMs);
    const int maxHighMs           = std::max(1, std::min({clampedBufferMs, maxHighByRatioMs, maxHighByHeadroomMs}));
    const int highMs              = std::clamp(requestedHighMs, minLowMs, maxHighMs);

    const int lowFloorMs        = std::min(minLowMs, highMs);
    const int effectiveMinGapMs = std::min(DecodeWatermarkMinGapMs, highMs - lowFloorMs);
    const int lowCeilForGapMs   = highMs - effectiveMinGapMs;
    const int lowMs             = std::clamp(requestedLowMs, lowFloorMs, lowCeilForGapMs);

    return {lowMs, highMs};
}

int startupPrefillFromOutputQueueMs(const Fooyin::AudioPipeline::OutputQueueSnapshot& snapshot,
                                    const Fooyin::AudioFormat& outputFormat)
{
    if(!snapshot.valid || !outputFormat.isValid() || outputFormat.sampleRate() <= 0) {
        return 0;
    }

    const int outputSampleRate = outputFormat.sampleRate();
    const auto framesToMs      = [outputSampleRate](int frames) {
        const int safeFrames = std::max(0, frames);
        if(safeFrames == 0) {
            return 0;
        }

        const auto scaledMs = std::llround((static_cast<long double>(safeFrames) * 1000.0L)
                                           / static_cast<long double>(outputSampleRate));
        return static_cast<int>(std::clamp<long double>(scaledMs, 0.0L, std::numeric_limits<int>::max()));
    };

    const double delaySeconds = std::isfinite(snapshot.state.delay) ? std::max(0.0, snapshot.state.delay) : 0.0;
    const int freeFrames      = std::max(0, snapshot.state.freeFrames);
    const int queuedFrames    = std::max(0, snapshot.state.queuedFrames);
    const int bufferFrames    = std::max(0, snapshot.bufferFrames);
    const int delayFrames     = std::max(0, static_cast<int>(std::llround(delaySeconds * outputSampleRate)));

    const int queueDemandFrames = std::max({freeFrames, bufferFrames, 0, queuedFrames + std::max(0, delayFrames)});
    const int delayDemandMs     = std::max(0, static_cast<int>(std::llround(delaySeconds * 1000.0)));

    return std::max(framesToMs(queueDemandFrames), delayDemandMs);
}

int gaplessBoundaryWatchdogDelayMs(int playbackBufferLengthMs)
{
    const int safeBufferMs = std::max(GaplessBoundaryWatchdogMinMs, playbackBufferLengthMs);
    return std::clamp(safeBufferMs / 2, GaplessBoundaryWatchdogMinMs, GaplessBoundaryWatchdogMaxMs);
}

Fooyin::AudioDecoder::PlaybackHints decoderPlaybackHintsForPlayMode(Fooyin::Playlist::PlayModes playMode)
{
    Fooyin::AudioDecoder::PlaybackHints hints{Fooyin::AudioDecoder::NoHints};

    hints.setFlag(Fooyin::AudioDecoder::PlaybackHint::RepeatTrackEnabled,
                  (playMode & Fooyin::Playlist::PlayMode::RepeatTrack) != 0);

    return hints;
}

size_t prefillFramesPerChunk(int sampleRate)
{
    if(sampleRate <= 0) {
        return BasePrefillDecodeFrames;
    }

    const uint64_t desiredFrames
        = (static_cast<uint64_t>(sampleRate) * static_cast<uint64_t>(PrefillChunkDurationMs)) / 1000ULL;

    return std::clamp<uint64_t>(desiredFrames, BasePrefillDecodeFrames, MaxPrefillDecodeFrames);
}

uint64_t scaledDelayMs(uint64_t delay, double scale)
{
    const double delayToTrackScale = std::clamp(scale, 0.05, 8.0);

    if(delay == 0) {
        return 0;
    }

    const auto scaledDelay = std::llround(static_cast<long double>(delay) * delayToTrackScale);
    return static_cast<uint64_t>(
        std::clamp<long double>(scaledDelay, 0.0, static_cast<long double>(std::numeric_limits<uint64_t>::max())));
}

bool audiblePauseDrainComplete(const Fooyin::AudioPipeline::OutputQueueSnapshot& snapshot,
                               const Fooyin::AudioFormat& outputFormat)
{
    if(!snapshot.valid) {
        return true;
    }

    const double delaySeconds = std::isfinite(snapshot.state.delay) ? std::max(0.0, snapshot.state.delay) : 0.0;
    const int queuedFrames    = std::max(0, snapshot.state.queuedFrames);
    const int outputRate      = outputFormat.sampleRate();

    if(queuedFrames == 0) {
        return true;
    }

    if(outputRate <= 0) {
        return queuedFrames == 0 && delaySeconds <= (static_cast<double>(AudiblePauseDrainThresholdMs) / 1000.0);
    }

    const int delayFrames = std::max(0, static_cast<int>(std::llround(delaySeconds * outputRate)));
    const int thresholdFrames
        = std::max(1, static_cast<int>((static_cast<int64_t>(outputRate) * AudiblePauseDrainThresholdMs) / 1000));

    return std::max(queuedFrames, delayFrames) <= thresholdFrames;
}

int audiblePauseWatchdogMs(int playbackBufferLengthMs, const Fooyin::AudioPipeline::OutputQueueSnapshot& snapshot,
                           const Fooyin::AudioFormat& outputFormat)
{
    int initialDelayMs{0};
    if(snapshot.valid) {
        if(std::isfinite(snapshot.state.delay) && snapshot.state.delay > 0.0) {
            initialDelayMs = std::max(initialDelayMs, static_cast<int>(std::llround(snapshot.state.delay * 1000.0)));
        }

        if(outputFormat.isValid() && outputFormat.sampleRate() > 0) {
            initialDelayMs = std::max(static_cast<uint64_t>(initialDelayMs),
                                      outputFormat.durationForFrames(std::max(0, snapshot.state.queuedFrames)));
        }
    }

    const int requestedMs = std::max({0, playbackBufferLengthMs, initialDelayMs}) + AudiblePauseWatchdogMarginMs;
    return std::clamp(requestedMs, AudiblePauseWatchdogMinMs, AudiblePauseWatchdogMaxMs);
}

bool samePlaybackItem(const Fooyin::Engine::PlaybackItem& lhs, const Fooyin::Engine::PlaybackItem& rhs)
{
    if(lhs.itemId != 0 && rhs.itemId != 0) {
        return lhs.itemId == rhs.itemId;
    }
    return Fooyin::sameTrackIdentity(lhs.track, rhs.track);
}

bool gaplessFormatsMatch(const Fooyin::AudioFormat& currentMixFormat, const Fooyin::AudioFormat& nextMixFormat)
{
    return currentMixFormat.isValid() && nextMixFormat.isValid()
        && Fooyin::Audio::inputFormatsMatch(currentMixFormat, nextMixFormat);
}

bool isFormatMismatchReadinessReason(const char* readinessReason)
{
    return readinessReason != nullptr && std::string_view{readinessReason} == "format-mismatch";
}

QEvent::Type engineTaskEventType()
{
    static const auto eventType = static_cast<QEvent::Type>(QEvent::registerEventType());
    return eventType;
}

QEvent::Type pipelineWakeEventType()
{
    static const auto eventType = static_cast<QEvent::Type>(QEvent::registerEventType());
    return eventType;
}
} // namespace

namespace Fooyin {
using EngineTaskType = EngineTaskQueue::TaskType;

AudioEngine::AudioEngine(std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings, DspRegistry* dspRegistry,
                         QObject* parent)
    : QObject{parent}
    , m_settings{settings}
    , m_dspRegistry{dspRegistry}
    , m_audioLoader{std::move(audioLoader)}
    , m_engineTaskQueue{this, engineTaskEventType()}
    , m_playbackState{Engine::PlaybackState::Stopped}
    , m_trackStatus{Engine::TrackStatus::NoTrack}
    , m_phase{Playback::Phase::Stopped}
    , m_trackGeneration{0}
    , m_streamToTrackOriginMs{0}
    , m_nextTransitionId{1}
    , m_decoder{this}
    , m_levelFrameMailbox{8}
    , m_levelFrameDispatchQueued{false}
    , m_pipelineWakeTaskQueued{false}
    , m_outputReconnectQueued{false}
    , m_fadeController{&m_pipeline}
    , m_outputController{this, &m_pipeline}
    , m_volume{m_settings->value<Settings::Core::OutputVolume>()}
    , m_playbackBufferLengthMs{m_settings->value<Settings::Core::BufferLength>()}
    , m_decodeLowWatermarkRatio{m_settings->value<Settings::Core::Internal::DecodeLowWatermarkRatio>()}
    , m_decodeHighWatermarkRatio{m_settings->value<Settings::Core::Internal::DecodeHighWatermarkRatio>()}
    , m_fadingEnabled{m_settings->value<Settings::Core::Internal::EngineFading>()}
    , m_crossfadeEnabled{m_settings->value<Settings::Core::Internal::EngineCrossfading>()}
    , m_gaplessEnabled{m_settings->value<Settings::Core::GaplessPlayback>()}
    , m_crossfadeSwitchPolicy{static_cast<Engine::CrossfadeSwitchPolicy>(
          m_settings->value<Settings::Core::Internal::CrossfadeSwitchPolicy>())}
    , m_audioClock{this}
    , m_lastReportedBitrate{0}
    , m_vbrUpdateIntervalMs{std::max(0, m_settings->value<Settings::Core::Internal::VBRUpdateInterval>())}
    , m_vbrUpdateTimerId{0}
    , m_lastAppliedSeekRequestId{0}
    , m_positionContextTrackGeneration{0}
    , m_positionContextTimelineEpoch{0}
    , m_positionContextSeekRequestId{0}
    , m_autoCrossfadeTailFadeActive{false}
    , m_autoCrossfadeTailFadeStreamId{InvalidStreamId}
    , m_autoCrossfadeTailFadeGeneration{0}
    , m_autoBoundaryFadeActive{false}
    , m_autoBoundaryFadeGeneration{0}
{
    setupSettings();

    m_outputController.setOutputStateHandler([this](AudioOutput::State state) {
        if(state != AudioOutput::State::Disconnected || m_engineTaskQueue.isShuttingDown()) {
            return;
        }

        if(m_outputReconnectQueued.exchange(true, std::memory_order_relaxed)) {
            return;
        }

        QMetaObject::invokeMethod(
            this,
            [this, state]() {
                m_outputReconnectQueued.store(false, std::memory_order_relaxed);

                if(m_engineTaskQueue.isShuttingDown()) {
                    return;
                }

                handleOutputStateChange(state);
            },
            Qt::QueuedConnection);
    });

    QObject::connect(
        &m_audioClock, &AudioClock::positionChanged, this, [this](uint64_t positionMs, uint64_t generation) {
            if(generation == m_positionContextTrackGeneration) {
                emit positionChanged(positionMs);
                emit positionChangedWithContext(positionMs, m_positionContextTrackGeneration,
                                                m_positionContextTimelineEpoch, m_positionContextSeekRequestId);
            }
        });
    QObject::connect(&m_audioClock, &AudioClock::requestSyncPosition, this, [this]() {
        if(!m_engineTaskQueue.isBarrierActive() && !m_engineTaskQueue.isDraining()) {
            updatePosition();
            return;
        }
        m_engineTaskQueue.enqueue(
            EngineTaskType::Timer, [engine = this]() { engine->updatePosition(); }, PositionSyncTimerCoalesceKey);
    });

    m_pipeline.setDspRegistry(m_dspRegistry);
    m_pipeline.addTrackProcessorFactory(
        [settings = std::shared_ptr<const ReplayGainProcessor::SharedSettings>(m_replayGainSharedSettings)]() {
            return std::make_unique<ReplayGainProcessor>(settings);
        });

    m_analysisBus = std::make_unique<AudioAnalysisBus>();
    m_analysisBus->setLevelReadyHandler(this, &AudioEngine::onLevelFrameReady);

    m_pipeline.start();
    m_pipeline.setFaderCurve(m_fadingValues.pause.curve);
    m_pipeline.setSignalWakeTarget(this, pipelineWakeEventType());
    m_pipeline.setAnalysisBus(m_analysisBus.get());

    m_nextTrackPrepareWorker.start([this](uint64_t jobToken, uint64_t requestId, const Engine::PlaybackItem& item,
                                          NextTrackPreparationState preparedState) {
        m_engineTaskQueue.enqueue(EngineTaskType::Worker, [this, jobToken, requestId, item,
                                                           preparedState = std::move(preparedState)]() mutable {
            applyPreparedNextTrackResult(jobToken, requestId, item, std::move(preparedState));
        });
    });
}

void AudioEngine::beginShutdown()
{
    if(m_vbrUpdateTimerId != 0) {
        killTimer(m_vbrUpdateTimerId);
        m_vbrUpdateTimerId = 0;
    }
    m_pipelineWakeTaskQueued.store(false, std::memory_order_relaxed);
    m_engineTaskQueue.beginShutdown();
}

AudioEngine::~AudioEngine()
{
    beginShutdown();

    if(m_analysisBus) {
        m_analysisBus->setSubscriptions(Engine::AnalysisDataTypes{});
        m_analysisBus->setLevelReadyHandler({});
    }

    m_pipeline.setAnalysisBus(nullptr);
    m_pipeline.setSignalWakeTarget(nullptr);
    m_analysisBus.reset();
    m_nextTrackPrepareWorker.stop();
    m_engineTaskQueue.clear();

    if(QThread::currentThread() == thread()) {
        stopImmediate();
        m_engineTaskQueue.drain();
    }
    else {
        qCWarning(ENGINE) << "AudioEngine destroyed off-thread; performing non-blocking shutdown";
    }

    m_decoder.stopDecoding();
    m_pipeline.stop();
}

bool AudioEngine::ensureCrossfadePrepared(const Engine::PlaybackItem& item, bool isManualChange)
{
    if(!m_preparedNext || !samePlaybackItem(item, m_preparedNext->item)) {
        if(isManualChange) {
            // Manual crossfades need prepared state immediately
            cancelPendingPrepareJobs();
            if(!prepareNextTrackImmediate(item)) {
                return false;
            }
        }
        else {
            prepareNextTrack(item);
        }
    }

    return true;
}

bool AudioEngine::handleManualChangeFade(const Engine::PlaybackItem& item)
{
    const bool manualCrossfadeConfigured = m_crossfadeEnabled && m_crossfadingValues.manualChange.isConfigured();

    if(manualCrossfadeConfigured) {
        return startTrackCrossfade(item, true);
    }

    const uint64_t transitionId = nextTransitionId();
    if(m_fadeController.handleManualChangeFade(item.track, m_crossfadeEnabled, m_crossfadingValues,
                                               hasPlaybackState(Engine::PlaybackState::Playing), m_volume,
                                               transitionId)) {
        m_pendingManualTrackItemId = item.itemId;
        setPhase(Playback::Phase::FadingToManualChange, PhaseChangeReason::ManualChangeFadeQueued);
        return true;
    }

    return false;
}

bool AudioEngine::startTrackCrossfade(const Engine::PlaybackItem& item, bool isManualChange)
{
    const Track& track = item.track;
    if(!track.isValid()) {
        return false;
    }

    if(!isManualChange && m_fadeController.hasPendingFade()) {
        return false;
    }

    if(!isManualChange && isMultiTrackFileTransition(m_currentTrack, track)) {
        return false;
    }

    if(!ensureCrossfadePrepared(item, isManualChange)) {
        return false;
    }

    const auto eligibility = evaluateAutoTransitionEligibility(track, isManualChange);
    if(!eligibility.has_value()) {
        return false;
    }

    const bool crossfadeMix       = eligibility->crossfadeMix;
    const bool gaplessHandoff     = eligibility->gaplessHandoff;
    const bool autoGaplessHandoff = !isManualChange && gaplessHandoff && !crossfadeMix;

    const uint64_t activeGeneration = m_trackGeneration;
    const auto activeStream         = m_decoder.activeStream();
    const StreamId activeStreamId   = (activeStream ? activeStream->id() : InvalidStreamId);
    const bool hasEarlyAutoTailFade = crossfadeMix && !isManualChange && m_autoCrossfadeTailFadeActive
                                   && m_autoCrossfadeTailFadeGeneration == activeGeneration
                                   && m_autoCrossfadeTailFadeStreamId == activeStreamId
                                   && activeStreamId != InvalidStreamId;

    int fadeOutDurationMs      = eligibility->fadeOutDurationMs;
    const int fadeInDurationMs = eligibility->fadeInDurationMs;

    if(crossfadeMix && fadeOutDurationMs > 0) {
        prefillActiveStream(std::min(fadeOutDurationMs, m_playbackBufferLengthMs));

        if(const auto currentStream = m_decoder.activeStream()) {
            fadeOutDurationMs = std::min(fadeOutDurationMs, static_cast<int>(currentStream->bufferedDurationMs()));
        }
        else {
            return false;
        }
    }

    const int overlapDurationMs
        = crossfadeMix ? std::min(std::max(0, fadeOutDurationMs), std::max(0, fadeInDurationMs)) : 0;
    const bool skipFadeOutStart = hasEarlyAutoTailFade && overlapDurationMs <= 0;

    clearAutoCrossfadeTailFadeState();
    clearAutoBoundaryFadeState(true);

    setCurrentTrackContext(item);
    updateTrackStatus(Engine::TrackStatus::Loading);
    m_streamToTrackOriginMs = track.offset();
    clearTrackEndLatch();
    m_transitions.clearTrackEnding();

    const bool wasDecoding = m_decoder.isDecodeTimerActive();
    if(wasDecoding) {
        m_decoder.stopDecodeTimer();
    }

    if(!initDecoder(item, true)) {
        m_pipeline.cleanupOrphanImmediate();
        updateTrackStatus(Engine::TrackStatus::Invalid);
        return true;
    }

    syncDecoderTrackMetadata();
    m_format = m_decoder.format();

    const auto newStream = m_decoder.setupCrossfadeStream(m_playbackBufferLengthMs, Engine::FadeCurve::Sine);
    if(!newStream) {
        updateTrackStatus(Engine::TrackStatus::Invalid);
        return true;
    }

    const int basePrefillMs = std::min(m_playbackBufferLengthMs / 4, MaxCrossfadePrefillMs);
    const int requestedPrefillMs
        = gaplessHandoff ? std::min(basePrefillMs, GaplessHandoffPrefillMs)
                         : ((fadeInDurationMs > 0) ? std::min(fadeInDurationMs, basePrefillMs) : basePrefillMs);
    const int prefillTargetMs = crossfadeMix ? adjustedCrossfadePrefillMs(requestedPrefillMs) : requestedPrefillMs;
    prefillActiveStream(prefillTargetMs);

    AudioPipeline::TransitionRequest request;
    request.type   = crossfadeMix ? AudioPipeline::TransitionType::Crossfade : AudioPipeline::TransitionType::Gapless;
    request.stream = newStream;
    request.fadeOutMs        = fadeOutDurationMs;
    request.fadeInMs         = fadeInDurationMs;
    request.skipFadeOutStart = skipFadeOutStart;

    const auto result = m_pipeline.executeTransition(request);
    if(!result.success || result.streamId == InvalidStreamId) {
        updateTrackStatus(Engine::TrackStatus::Invalid);
        return true;
    }

    if(autoGaplessHandoff) {
        m_positionCoordinator.setGaplessHold(result.streamId);
    }
    else {
        m_positionCoordinator.clearGaplessHold();
    }

    const bool crossfadeInProgress = crossfadeMix && m_pipeline.hasOrphanStream();
    if(crossfadeInProgress) {
        setPhase(Playback::Phase::TrackCrossfading, PhaseChangeReason::AutoTransitionCrossfadeActive);
    }
    else if(hasPlaybackState(Engine::PlaybackState::Playing)) {
        setPhase(Playback::Phase::Playing, PhaseChangeReason::AutoTransitionPlayingNoOrphan);
    }

    if(wasDecoding || hasPlaybackState(Engine::PlaybackState::Playing)) {
        m_decoder.ensureDecodeTimerRunning();
    }

    updateTrackStatus(Engine::TrackStatus::Loaded);
    updateTrackStatus(Engine::TrackStatus::Buffered);
    if(isManualChange) {
        updatePositionContext(m_pipeline.currentStatus().timelineEpoch);
        publishPosition(0, 0, 1.0, AudioClock::UpdateMode::Discontinuity, true);
    }

    const auto committedMode = crossfadeMix ? Engine::TransitionMode::Crossfade
                                            : (m_transitions.autoTransitionMode() == AutoTransitionMode::BoundaryFade
                                                   ? Engine::TransitionMode::BoundaryFade
                                                   : Engine::TransitionMode::Gapless);
    finaliseTrackCommit(committedMode);

    return true;
}

void AudioEngine::performSeek(uint64_t positionMs, uint64_t requestId)
{
    if(!m_decoder.isValid()) {
        return;
    }

    m_positionCoordinator.clearGaplessHold();

    clearAutoCrossfadeTailFadeState();
    clearAutoBoundaryFadeState(true);
    clearAutoAdvanceState();
    invalidatePreparedAutoTransitionsForSeek();

    if(m_analysisBus) {
        m_analysisBus->flush();
    }

    m_transitions.cancelPendingSeek();

    auto currentStream = m_decoder.activeStream();
    const SeekPlanContext context{
        .decoderValid           = m_decoder.isValid(),
        .isPlaying              = hasPlaybackState(Engine::PlaybackState::Playing),
        .crossfadeEnabled       = m_crossfadeEnabled,
        .trackDurationMs        = m_currentTrack.duration(),
        .autoCrossfadeOutMs     = m_crossfadingValues.autoChange.effectiveOutMs(),
        .seekFadeOutMs          = m_crossfadingValues.seek.effectiveOutMs(),
        .seekFadeInMs           = m_crossfadingValues.seek.effectiveInMs(),
        .hasCurrentStream       = static_cast<bool>(currentStream),
        .requestedPositionMs    = positionMs,
        .bufferedDurationMs     = currentStream ? currentStream->bufferedDurationMs() : 0,
        .decoderHighWatermarkMs = m_decoder.highWatermarkMs(),
        .bufferLengthMs         = m_playbackBufferLengthMs,
    };

    SeekPlan plan = planSeek(context);
    if(plan.strategy == SeekStrategy::NoOp) {
        return;
    }

    if(plan.strategy == SeekStrategy::SimpleSeek) {
        performSimpleSeek(positionMs, requestId);
        return;
    }

    if(!currentStream) {
        performSimpleSeek(positionMs, requestId);
        return;
    }

    if(plan.reserveTargetMs > 0) {
        m_decoder.requestDecodeReserveMs(plan.reserveTargetMs);
        if(currentStream->bufferedDurationMs() < static_cast<uint64_t>(plan.reserveTargetMs)) {
            prefillActiveStream(plan.reserveTargetMs);
            if(currentStream->bufferedDurationMs() < static_cast<uint64_t>(plan.reserveTargetMs)) {
                qCWarning(ENGINE) << "Seek reserve prefill shortfall:" << "buffered="
                                  << currentStream->bufferedDurationMs() << "ms reserveTarget=" << plan.reserveTargetMs
                                  << "ms";
            }
        }

        SeekPlanContext postPrefillContext    = context;
        postPrefillContext.bufferedDurationMs = currentStream ? currentStream->bufferedDurationMs() : uint64_t{0};
        plan                                  = planSeek(postPrefillContext);
        if(plan.strategy != SeekStrategy::CrossfadeSeek) {
            performSimpleSeek(positionMs, requestId);
            return;
        }
    }

    startSeekCrossfade(positionMs, plan.fadeOutDurationMs, plan.fadeInDurationMs, requestId);
}

void AudioEngine::checkPendingSeek()
{
    if(!m_transitions.hasPendingSeek()) {
        return;
    }

    auto currentStream = m_decoder.activeStream();
    if(!currentStream) {
        m_transitions.cancelPendingSeek();
        return;
    }

    if(const auto params = m_transitions.pendingSeekIfBuffered(currentStream->bufferedDurationMs())) {
        m_transitions.cancelPendingSeek();
        startSeekCrossfade(params->seekPositionMs, params->fadeOutDurationMs, params->fadeInDurationMs, 0);
    }
}

void AudioEngine::startSeekCrossfade(uint64_t positionMs, int fadeOutDurationMs, int fadeInDurationMs,
                                     uint64_t requestId)
{
    m_transitions.setSeekInProgress(true);
    clearPendingAnalysisData();

    const bool wasDecoding = m_decoder.isDecodeTimerActive();
    if(wasDecoding) {
        m_decoder.stopDecodeTimer();
    }

    const uint64_t seekPosMs = absoluteTrackPositionMs(positionMs, m_currentTrack.offset());
    const auto newStream
        = m_decoder.prepareSeekStream(seekPosMs, m_playbackBufferLengthMs, Engine::FadeCurve::Sine, m_currentTrack);
    if(!newStream) {
        m_transitions.setSeekInProgress(false);
        stopImmediate();
        return;
    }

    const int requestedPrefillMs = std::max(fadeInDurationMs, m_playbackBufferLengthMs / 4);
    const int prefillTargetMs    = adjustedCrossfadePrefillMs(requestedPrefillMs);
    prefillActiveStream(prefillTargetMs);
    if(fadeInDurationMs > 0
       && std::cmp_less(newStream->bufferedDurationMs(), static_cast<uint64_t>(fadeInDurationMs))) {
        qCWarning(ENGINE) << "Seek crossfade prefill below fade-in window:" << "buffered="
                          << newStream->bufferedDurationMs() << "ms fadeIn=" << fadeInDurationMs
                          << "ms target=" << prefillTargetMs << "ms";
    }

    if(newStream->bufferedDurationMs() == 0 && newStream->endOfInput()) {
        qCWarning(ENGINE) << "Seek crossfade failed: no audio after prefill, stopping playback";
        m_transitions.setSeekInProgress(false);
        stopImmediate();
        return;
    }

    AudioPipeline::TransitionRequest request;
    request.type      = AudioPipeline::TransitionType::SeekCrossfade;
    request.stream    = newStream;
    request.fadeOutMs = fadeOutDurationMs;
    request.fadeInMs  = fadeInDurationMs;

    const auto result = m_pipeline.executeTransition(request);
    if(!result.success || result.streamId == InvalidStreamId) {
        m_transitions.setSeekInProgress(false);
        stopImmediate();
        return;
    }

    if(m_pipeline.hasOrphanStream()) {
        setPhase(Playback::Phase::SeekCrossfading, PhaseChangeReason::SeekCrossfadeActive);
    }
    else {
        setPhase(Playback::Phase::Seeking, PhaseChangeReason::SeekSimpleActive);
    }

    m_transitions.clearTrackEnding();

    if(wasDecoding || hasPlaybackState(Engine::PlaybackState::Playing)) {
        m_decoder.ensureDecodeTimerRunning();
    }

    m_transitions.setSeekInProgress(false);
    const uint64_t pipelineDelayMs  = m_pipeline.playbackDelayMs();
    const double delayToSourceScale = m_pipeline.playbackDelayToTrackScale();

    if(requestId != 0) {
        m_lastAppliedSeekRequestId = requestId;
    }

    updatePositionContext(m_pipeline.currentStatus().timelineEpoch);
    publishPosition(positionMs, pipelineDelayMs, delayToSourceScale, AudioClock::UpdateMode::Discontinuity, true);

    if(requestId != 0) {
        emit seekPositionApplied(positionMs, requestId);
    }

    if(!m_pipeline.hasOrphanStream() && hasPlaybackState(Engine::PlaybackState::Playing)) {
        setPhase(Playback::Phase::Playing, PhaseChangeReason::SeekRestorePlaying);
    }
}

void AudioEngine::performSimpleSeek(uint64_t positionMs, uint64_t requestId, int prefillTargetMs)
{
    m_decoder.clearDecodeReserve();

    const bool wasPlaying  = hasPlaybackState(Engine::PlaybackState::Playing);
    const bool wasDecoding = m_decoder.isDecodeTimerActive();
    setPhase(Playback::Phase::Seeking, PhaseChangeReason::SeekSimpleActive);

    const auto restorePhaseFromTransport = [this]() {
        if(hasPlaybackState(Engine::PlaybackState::Playing)) {
            setPhase(Playback::Phase::Playing, PhaseChangeReason::SeekRestorePlaying);
            return;
        }
        if(hasPlaybackState(Engine::PlaybackState::Paused)) {
            setPhase(Playback::Phase::Paused, PhaseChangeReason::SeekRestorePaused);
            return;
        }
        if(hasPlaybackState(Engine::PlaybackState::Stopped)) {
            setPhase(Playback::Phase::Stopped, PhaseChangeReason::SeekRestoreStopped);
        }
    };

    m_transitions.setSeekInProgress(true);
    m_transitions.clearTrackEnding();
    clearPendingAnalysisData();

    if(wasDecoding) {
        m_decoder.stopDecodeTimer();
    }

    if(isCrossfading(m_phase) || m_pipeline.hasOrphanStream()) {
        m_fadeController.invalidateActiveFade();
        m_pipeline.cleanupOrphanImmediate();
    }

    auto stream = m_decoder.activeStream();
    if(!stream) {
        m_transitions.setSeekInProgress(false);
        restorePhaseFromTransport();
        return;
    }

    if(wasPlaying) {
        m_pipeline.pause();
    }

    const uint64_t seekPosMs = absoluteTrackPositionMs(positionMs, m_currentTrack.offset());
    m_pipeline.resetStreamForSeek(stream->id(), seekPosMs);

    m_decoder.seek(seekPosMs);
    m_decoder.syncStreamPosition();

    if(!m_decoder.isDecoding()) {
        m_decoder.start();
    }

    const int seekPrefillMs = prefillTargetMs > 0 ? prefillTargetMs : (m_playbackBufferLengthMs / 4);
    prefillActiveStream(seekPrefillMs);

    if(stream->bufferedDurationMs() == 0 && stream->endOfInput()) {
        qCWarning(ENGINE) << "Simple seek failed: no audio after prefill, stopping playback";
        m_transitions.setSeekInProgress(false);
        stopImmediate();
        return;
    }

    if(wasPlaying) {
        m_pipeline.sendStreamCommand(stream->id(), AudioStream::Command::Play);
        m_pipeline.play();
    }

    if(wasDecoding || wasPlaying) {
        m_decoder.ensureDecodeTimerRunning();
    }

    m_transitions.setSeekInProgress(false);
    const uint64_t pipelineDelayMs  = m_pipeline.playbackDelayMs();
    const double delayToSourceScale = m_pipeline.playbackDelayToTrackScale();
    if(requestId != 0) {
        m_lastAppliedSeekRequestId = requestId;
    }
    updatePositionContext(m_pipeline.currentStatus().timelineEpoch);
    publishPosition(positionMs, pipelineDelayMs, delayToSourceScale, AudioClock::UpdateMode::Discontinuity, true);
    if(requestId != 0) {
        emit seekPositionApplied(positionMs, requestId);
    }
    restorePhaseFromTransport();
}

void AudioEngine::prefillActiveStream(int targetMs)
{
    int sampleRate = m_format.sampleRate();
    if(const auto stream = m_decoder.activeStream()) {
        sampleRate = stream->sampleRate();
    }

    const int chunksDecoded = m_decoder.prefillActiveStreamMs(targetMs, 0, prefillFramesPerChunk(sampleRate));
    if(chunksDecoded > 0) {
        syncDecoderBitrate();
    }
}

int AudioEngine::adjustedCrossfadePrefillMs(int requestedMs) const
{
    if(requestedMs <= 0) {
        requestedMs = m_playbackBufferLengthMs / 4;
    }

    return std::clamp(requestedMs, 1, std::max(1, m_playbackBufferLengthMs));
}

void AudioEngine::cancelAllFades()
{
    m_fadeController.cancelAll(m_fadingValues.pause.curve);
}

void AudioEngine::cancelFadesForReinit()
{
    m_fadeController.cancelForReinit(m_fadingValues.pause.curve);
}

void AudioEngine::reconfigureActiveStreamBuffering(uint64_t positionMs)
{
    if(!m_currentTrack.isValid()) {
        return;
    }

    const auto barrier = m_engineTaskQueue.barrierScope();

    if(m_pipeline.hasOrphanStream()) {
        qCWarning(ENGINE) << "Reconfiguring playback buffering during active transition; forcing orphan cleanup";
        m_pipeline.cleanupOrphanImmediate();
    }

    if(isCrossfading(m_phase)) {
        qCWarning(ENGINE) << "Reconfiguring playback buffering while crossfade state is active; forcing idle state";
    }

    clearPendingAudiblePause();
    clearAutoCrossfadeTailFadeState();
    clearAutoBoundaryFadeState(true);
    clearTrackEndAutoTransitions();
    clearPendingAnalysisData();
    clearTrackEndLatch();
    m_transitions.cancelPendingSeek();
    m_transitions.setSeekInProgress(false);
    m_transitions.clearTrackEnding();
    m_fadeController.invalidateActiveFade();

    m_decoder.stopDecoding();
    cleanupActiveStream();

    if(!initDecoder(currentPlaybackItem(), false)) {
        updateTrackStatus(Engine::TrackStatus::Invalid);
        return;
    }

    syncDecoderTrackMetadata();
    m_format = m_decoder.format();

    const AudioFormat oldInputFormat = m_pipeline.inputFormat();
    AudioFormat desiredOutput        = m_pipeline.predictOutputFormat(m_format);
    if(!desiredOutput.isValid()) {
        desiredOutput = m_format;
    }

    const AudioFormat currentOutput = m_pipeline.outputFormat();
    const bool inputFormatChanged   = oldInputFormat.isValid() && !Audio::inputFormatsMatch(oldInputFormat, m_format);
    const bool outputFormatChanged  = outputReinitRequired(currentOutput, desiredOutput);
    const bool hasOutput            = m_pipeline.hasOutput();

    if(!hasOutput || outputFormatChanged) {
        if(outputFormatChanged && hasOutput) {
            qCDebug(ENGINE) << "Playback buffering reconfiguration requires output reinit";
            m_outputController.uninitOutput();
        }
        if(!m_outputController.initOutput(m_format, m_volume)) {
            updateTrackStatus(Engine::TrackStatus::Invalid);
            return;
        }
    }
    else if(inputFormatChanged) {
        m_pipeline.applyInputFormat(m_format);
    }

    uint64_t clampedPositionMs{positionMs};
    if(m_currentTrack.duration() > 0) {
        clampedPositionMs = std::min<uint64_t>(clampedPositionMs, m_currentTrack.duration());
    }
    m_transitions.queueInitialSeek(clampedPositionMs, m_currentTrack.id(), 0);

    if(!setupNewTrackStream(m_currentTrack, true)) {
        updateTrackStatus(Engine::TrackStatus::Invalid);
    }
}

void AudioEngine::reinitOutputForCurrentFormat()
{
    if(!m_format.isValid()) {
        return;
    }

    const auto barrier = m_engineTaskQueue.barrierScope();

    m_fadeController.invalidateActiveFade();

    if(m_pipeline.hasOrphanStream()) {
        qCWarning(ENGINE) << "Reinitializing output during active transition; forcing orphan cleanup";
        m_pipeline.cleanupOrphanImmediate();
    }

    if(isCrossfading(m_phase)) {
        qCWarning(ENGINE) << "Reinitializing output while crossfade state is active; forcing idle state";
    }

    clearAutoCrossfadeTailFadeState();
    clearAutoBoundaryFadeState();
    m_transitions.cancelPendingSeek();
    m_transitions.setSeekInProgress(false);
    clearPendingAnalysisData();

    const auto prevState   = playbackState();
    const bool wasPlaying  = prevState == Engine::PlaybackState::Playing;
    const bool wasPaused   = prevState == Engine::PlaybackState::Paused;
    const bool wasDecoding = m_decoder.isDecodeTimerActive();

    if(wasDecoding) {
        m_decoder.stopDecodeTimer();
    }

    if(wasPlaying || wasPaused) {
        cancelFadesForReinit();
        m_outputController.pauseOutputImmediate(false);
    }

    m_outputController.uninitOutput();
    if(!m_outputController.initOutput(m_format, m_volume)) {
        updateTrackStatus(Engine::TrackStatus::Invalid);
        return;
    }

    if(wasPlaying) {
        if(auto stream = m_decoder.activeStream()) {
            m_pipeline.sendStreamCommand(stream->id(), AudioStream::Command::Play);
        }
        m_pipeline.play();
    }

    if(wasDecoding || wasPlaying) {
        m_decoder.ensureDecodeTimerRunning();
    }
}

void AudioEngine::applyFadeResult(const FadeController::FadeResult& result)
{
    if((result.pauseNow || result.stopImmediateNow) && result.transportTransitionId != 0
       && !m_transitions.isActiveTransportTransition(result.transportTransitionId)) {
        return;
    }

    if(result.setPauseCurve) {
        m_pipeline.setFaderCurve(m_fadingValues.pause.curve);
    }
    if(result.stopImmediateNow) {
        clearTransportTransition();
        stopImmediate();
    }
    else if(result.pauseNow) {
        beginAudiblePauseCompletion(result.transportTransitionId);
    }
    if(result.loadTrack) {
        loadTrack({.track = *result.loadTrack, .itemId = std::exchange(m_pendingManualTrackItemId, uint64_t{0})},
                  false);
    }
}

void AudioEngine::beginAudiblePauseCompletion(const uint64_t transportTransitionId)
{
    const auto outputSnapshot = m_pipeline.outputQueueSnapshot();
    const auto outputFormat   = m_pipeline.outputFormat();

    m_decoder.stopDecodeTimer();
    m_pipeline.beginPauseDrain();
    clearPendingAnalysisData();
    m_audioClock.stop();
    updatePlaybackState(Engine::PlaybackState::Paused);

    m_pendingAudiblePause.active       = true;
    m_pendingAudiblePause.transitionId = transportTransitionId;
    m_pendingAudiblePause.serial       = ++m_nextPendingAudiblePauseSerial;
    m_pendingAudiblePause.startedAt    = std::chrono::steady_clock::now();
    m_pendingAudiblePause.watchdogMs   = audiblePauseWatchdogMs(m_playbackBufferLengthMs, outputSnapshot, outputFormat);

    maybeCompletePendingAudiblePause(m_pendingAudiblePause.serial);
}

void AudioEngine::maybeCompletePendingAudiblePause(const uint64_t serial)
{
    if(!m_pendingAudiblePause.active || m_pendingAudiblePause.serial != serial) {
        return;
    }

    if(m_pendingAudiblePause.transitionId != 0
       && !m_transitions.isActiveTransportTransition(m_pendingAudiblePause.transitionId)) {
        clearPendingAudiblePause();
        return;
    }

    const auto outputSnapshot = m_pipeline.outputQueueSnapshot();
    const auto outputFormat   = m_pipeline.outputFormat();
    const bool drainComplete  = audiblePauseDrainComplete(outputSnapshot, outputFormat);
    const auto elapsedMs      = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()
                                                                                      - m_pendingAudiblePause.startedAt)
                                    .count();
    const bool watchdogExpired = m_pendingAudiblePause.watchdogMs > 0 && elapsedMs >= m_pendingAudiblePause.watchdogMs;

    if(!drainComplete && !watchdogExpired) {
        QTimer::singleShot(std::chrono::milliseconds{AudiblePauseDrainPollMs}, this,
                           [this, serial]() { maybeCompletePendingAudiblePause(serial); });
        return;
    }

    if(!drainComplete && watchdogExpired) {
        const int queuedFrames = outputSnapshot.valid ? std::max(0, outputSnapshot.state.queuedFrames) : -1;
        const int delayMs      = outputSnapshot.valid && std::isfinite(outputSnapshot.state.delay)
                                   ? std::max(0, static_cast<int>(std::llround(outputSnapshot.state.delay * 1000.0)))
                                   : -1;
        qCWarning(ENGINE) << "Audible pause drain watchdog expired:"
                          << "watchdogMs=" << m_pendingAudiblePause.watchdogMs << "elapsedMs=" << elapsedMs
                          << "queuedFrames=" << queuedFrames << "delayMs=" << delayMs;
    }

    clearPendingAudiblePause();
    finalisePausedState();
}

void AudioEngine::finalisePausedState()
{
    clearTransportTransition();
    m_pipeline.pause();
    m_fadeController.setState(FadeState::Idle);
    m_fadeController.setFadeOnNext(false);
    setPhase(Playback::Phase::Paused, PhaseChangeReason::PlaybackStatePaused);
}

void AudioEngine::clearPendingAudiblePause()
{
    m_pendingAudiblePause.active       = false;
    m_pendingAudiblePause.transitionId = 0;
    m_pendingAudiblePause.watchdogMs   = 0;
}

bool AudioEngine::cancelPendingAudiblePause()
{
    if(!m_pendingAudiblePause.active) {
        return false;
    }

    clearPendingAudiblePause();
    clearTransportTransition();

    m_fadeController.applyPlayFade(Engine::PlaybackState::Paused, m_fadingEnabled, m_fadingValues, m_volume);
    m_decoder.startDecoding();
    m_pipeline.play();

    updatePosition();
    m_audioClock.start();
    updatePlaybackState(Engine::PlaybackState::Playing);
    return true;
}

void AudioEngine::handlePipelineFadeEvent(const AudioPipeline::FadeEvent& event)
{
    const bool fadeOut = event.type == OutputFader::CompletionType::FadeOutComplete;
    applyFadeResult(m_fadeController.handlePipelineFadeEvent(fadeOut, event.token));
}

void AudioEngine::syncDecoderTrackMetadata()
{
    if(!m_decoder.refreshTrackMetadata()) {
        return;
    }

    const Track changedTrack = m_decoder.track();
    if(!changedTrack.isValid()) {
        return;
    }

    m_currentTrack = changedTrack;
    setStreamToTrackOriginForTrack(changedTrack);
    if(changedTrack.bitrate() >= MinLiveBitrateKbps) {
        publishBitrate(changedTrack.bitrate());
    }
    emit trackChanged(changedTrack);
}

void AudioEngine::publishBitrate(int bitrate)
{
    const int clampedBitrate = std::max(0, bitrate);
    if(m_lastReportedBitrate == clampedBitrate) {
        return;
    }

    m_lastReportedBitrate = clampedBitrate;
    emit bitrateChanged(clampedBitrate);
}

void AudioEngine::syncDecoderBitrate()
{
    if(!m_decoder.isValid()) {
        return;
    }

    if(m_vbrUpdateIntervalMs <= 0) {
        return;
    }

    int bitrate{0};
    if(const auto stream = m_decoder.activeStream()) {
        bitrate = stream->audibleWindowBitrate(LiveBitrateWindowMs);
    }

    if(bitrate >= MinLiveBitrateKbps) {
        const auto now = std::chrono::steady_clock::now();
        if(m_lastVbrUpdateAt != std::chrono::steady_clock::time_point{}
           && (now - m_lastVbrUpdateAt) < std::chrono::milliseconds{m_vbrUpdateIntervalMs}) {
            return;
        }

        m_lastVbrUpdateAt = now;
        publishBitrate(bitrate);
    }
}

AudioStreamPtr AudioEngine::currentTrackTimingStream() const
{
    const bool stagedPreparedGaplessDecoder
        = m_preparedGaplessTransition.active && m_preparedGaplessTransition.sourceGeneration == m_trackGeneration
       && m_preparedGaplessTransition.decoderAdopted && m_preparedGaplessTransition.preCommitTimingStream;
    if(stagedPreparedGaplessDecoder) {
        return m_preparedGaplessTransition.preCommitTimingStream;
    }

    return m_decoder.activeStream();
}

void AudioEngine::scheduleGaplessBoundaryStallDiagnostic(uint64_t generation, StreamId currentStreamId,
                                                         StreamId preparedStreamId)
{
    const auto delay = std::chrono::milliseconds{gaplessBoundaryWatchdogDelayMs(m_playbackBufferLengthMs)};
    QTimer::singleShot(delay, this, [this, generation, currentStreamId, preparedStreamId]() {
        if(generation != m_trackGeneration || !m_preparedGaplessTransition.active
           || m_preparedGaplessTransition.sourceGeneration != generation
           || m_preparedGaplessTransition.streamId != preparedStreamId
           || !m_autoAdvanceState.boundaryPendingUntilAudible) {
            return;
        }

        logGaplessBoundaryDiagnostic("boundary watchdog expired", currentStreamId, preparedStreamId);
    });
}

void AudioEngine::logGaplessBoundaryDiagnostic(const char* reason, StreamId triggerCurrentStreamId,
                                               StreamId triggerPreparedStreamId) const
{
    const auto pipelineStatus = m_pipeline.currentStatus();
    const auto outputSnapshot = m_pipeline.outputQueueSnapshot();
    const auto activeStream   = m_decoder.activeStream();
    const auto timingStream   = currentTrackTimingStream();

    const StreamId activeStreamId = activeStream ? activeStream->id() : InvalidStreamId;
    const StreamId timingStreamId = timingStream ? timingStream->id() : InvalidStreamId;
    const StreamId preparedStreamId
        = m_preparedGaplessTransition.active ? m_preparedGaplessTransition.streamId : InvalidStreamId;
    const StreamId audibleOutputStreamId = m_pipeline.audibleOutputStreamId();

    AudioStreamPtr preparedStream;
    if(preparedStreamId != InvalidStreamId) {
        if(activeStream && activeStream->id() == preparedStreamId) {
            preparedStream = activeStream;
        }
        else if(m_preparedNext && m_preparedNext->preparedStream
                && m_preparedNext->preparedStream->id() == preparedStreamId) {
            preparedStream = m_preparedNext->preparedStream;
        }
    }

    const int backendQueuedFrames = outputSnapshot.valid ? std::max(0, outputSnapshot.state.queuedFrames) : -1;
    const int backendFreeFrames   = outputSnapshot.valid ? std::max(0, outputSnapshot.state.freeFrames) : -1;
    const int backendBufferFrames = outputSnapshot.valid ? std::max(0, outputSnapshot.bufferFrames) : -1;
    const double backendDelayS    = outputSnapshot.valid ? outputSnapshot.state.delay : -1.0;

    qCWarning(ENGINE) << "Gapless handoff diagnostic:" << reason << "trackId=" << m_currentTrack.id()
                      << "generation=" << m_trackGeneration
                      << "mode=" << static_cast<int>(effectiveAutoTransitionMode())
                      << "boundarySeen=" << m_autoAdvanceState.boundaryAnchorSeen
                      << "boundaryPending=" << m_autoAdvanceState.boundaryPendingUntilAudible
                      << "preparedActive=" << m_preparedGaplessTransition.active
                      << "decoderAdopted=" << m_preparedGaplessTransition.decoderAdopted
                      << "triggerCurrentStreamId=" << triggerCurrentStreamId
                      << "triggerPreparedStreamId=" << triggerPreparedStreamId << "activeStreamId=" << activeStreamId
                      << "timingStreamId=" << timingStreamId << "preparedStreamId=" << preparedStreamId
                      << "audibleOutputStreamId=" << audibleOutputStreamId
                      << "renderedStreamId=" << pipelineStatus.renderedSegment.streamId
                      << "renderedFrames=" << pipelineStatus.renderedSegment.outputFrames
                      << "positionMs=" << pipelineStatus.position
                      << "positionMapped=" << pipelineStatus.positionIsMapped
                      << "backendQueuedFrames=" << backendQueuedFrames << "backendFreeFrames=" << backendFreeFrames
                      << "backendBufferFrames=" << backendBufferFrames << "backendDelayS=" << backendDelayS
                      << "currentBufferedMs=" << (activeStream ? activeStream->bufferedDurationMs() : 0)
                      << "currentEndOfInput=" << (activeStream ? activeStream->endOfInput() : false)
                      << "currentBufferEmpty=" << (activeStream ? activeStream->bufferEmpty() : true) << "currentState="
                      << static_cast<int>(activeStream ? activeStream->state() : AudioStream::State::Stopped)
                      << "preparedBufferedMs=" << (preparedStream ? preparedStream->bufferedDurationMs() : 0)
                      << "preparedState="
                      << static_cast<int>(preparedStream ? preparedStream->state() : AudioStream::State::Stopped);
}

void AudioEngine::publishPosition(uint64_t sourcePositionMs, uint64_t outputDelayMs, double delayToSourceScale,
                                  AudioClock::UpdateMode mode, bool emitNow)
{
    m_audioClock.applyPosition(sourcePositionMs, outputDelayMs, delayToSourceScale, m_trackGeneration, mode, emitNow);
    if(mode == AudioClock::UpdateMode::Discontinuity) {
        m_positionCoordinator.notePublishedDiscontinuity(sourcePositionMs);
    }
}

void AudioEngine::updatePositionContext(uint64_t timelineEpoch)
{
    const uint64_t trackGeneration = m_trackGeneration;
    const uint64_t seekRequestId   = m_lastAppliedSeekRequestId;

    if(trackGeneration == m_positionContextTrackGeneration && timelineEpoch == m_positionContextTimelineEpoch
       && seekRequestId == m_positionContextSeekRequestId) {
        return;
    }

    m_positionContextTrackGeneration = trackGeneration;
    m_positionContextTimelineEpoch   = timelineEpoch;
    m_positionContextSeekRequestId   = seekRequestId;

    emit positionContextChanged(trackGeneration, timelineEpoch, seekRequestId);
}

void AudioEngine::maybeBeginAutoCrossfadeTailFadeOut(const AudioStreamPtr& stream, uint64_t relativePosMs)
{
    if(!stream || !hasPlaybackState(Engine::PlaybackState::Playing)) {
        return;
    }

    if(m_transitions.autoTransitionMode() != AutoTransitionMode::Crossfade) {
        return;
    }

    const int fadeOutMs    = std::max(0, m_crossfadingValues.autoChange.effectiveOutMs());
    const int fadeInMs     = std::max(0, m_crossfadingValues.autoChange.effectiveInMs());
    const int overlapMs    = std::min(fadeOutMs, fadeInMs);
    const int preOverlapMs = std::max(0, fadeOutMs - overlapMs);

    if(preOverlapMs <= 0) {
        return;
    }

    if(m_fadeController.hasPendingFade() || m_pipeline.hasOrphanStream()) {
        return;
    }

    const StreamId streamId = stream->id();
    if(m_autoCrossfadeTailFadeActive && m_autoCrossfadeTailFadeGeneration == m_trackGeneration
       && m_autoCrossfadeTailFadeStreamId == streamId) {
        return;
    }

    if(m_currentTrack.duration() == 0 || relativePosMs >= m_currentTrack.duration()) {
        return;
    }

    const uint64_t remainingTrackMs = m_currentTrack.duration() - relativePosMs;
    const int remainingWindowMs
        = static_cast<int>(std::min<uint64_t>(remainingTrackMs, std::numeric_limits<int>::max()));

    const int tailFadeOutMs = std::min(fadeOutMs, remainingWindowMs);
    if(tailFadeOutMs <= 0) {
        return;
    }

    // Use a solo tail fade so the fade is audible before the next track starts
    // Overlap fade is started at switch time
    stream->setFadeCurve(Engine::FadeCurve::Linear);
    m_pipeline.sendStreamCommand(streamId, AudioStream::Command::StartFadeOut, tailFadeOutMs);

    m_autoCrossfadeTailFadeActive     = true;
    m_autoCrossfadeTailFadeStreamId   = streamId;
    m_autoCrossfadeTailFadeGeneration = m_trackGeneration;
}

void AudioEngine::maybeBeginAutoBoundaryFadeOut(uint64_t relativePosMs)
{
    if(!hasPlaybackState(Engine::PlaybackState::Playing)) {
        return;
    }

    if(effectiveAutoTransitionMode() != AutoTransitionMode::BoundaryFade) {
        return;
    }

    const auto& boundarySpec = m_fadingValues.boundary;
    if(!boundarySpec.enabled) {
        return;
    }

    const int fadeOutMs = std::max(0, boundarySpec.effectiveOutMs());
    if(fadeOutMs <= 0) {
        return;
    }

    if(m_fadeController.hasPendingFade()) {
        return;
    }

    if(m_autoBoundaryFadeActive && m_autoBoundaryFadeGeneration == m_trackGeneration) {
        return;
    }

    if(m_currentTrack.duration() == 0 || relativePosMs >= m_currentTrack.duration()) {
        return;
    }

    const uint64_t remainingTrackMs = m_currentTrack.duration() - relativePosMs;
    const int remainingWindowMs
        = static_cast<int>(std::min<uint64_t>(remainingTrackMs, std::numeric_limits<int>::max()));
    if(remainingWindowMs > fadeOutMs) {
        return;
    }

    const int tailFadeOutMs = std::min(fadeOutMs, remainingWindowMs);

    if(tailFadeOutMs <= 0) {
        return;
    }

    m_pipeline.setFaderCurve(boundarySpec.curve);
    m_pipeline.faderFadeOut(tailFadeOutMs, 1.0, 0);
    m_autoBoundaryFadeActive     = true;
    m_autoBoundaryFadeGeneration = m_trackGeneration;
}

void AudioEngine::applyAutoBoundaryFadeIn(const bool allowFadeInOnly)
{
    if(!allowFadeInOnly && !m_autoBoundaryFadeActive) {
        return;
    }

    const int fadeInMs = std::max(0, m_fadingValues.boundary.effectiveInMs());
    m_pipeline.setFaderCurve(m_fadingValues.boundary.curve);
    m_pipeline.faderFadeIn(fadeInMs, 1.0, 0);
    clearAutoBoundaryFadeState();
}

void AudioEngine::clearAutoCrossfadeTailFadeState()
{
    m_autoCrossfadeTailFadeActive     = false;
    m_autoCrossfadeTailFadeStreamId   = InvalidStreamId;
    m_autoCrossfadeTailFadeGeneration = 0;
}

void AudioEngine::clearAutoBoundaryFadeState(bool restoreOutput)
{
    if(restoreOutput && m_autoBoundaryFadeActive) {
        m_pipeline.faderFadeIn(0, 1.0, 0);
    }

    m_autoBoundaryFadeActive     = false;
    m_autoBoundaryFadeGeneration = 0;
}

void AudioEngine::clearAutoAdvanceState()
{
    m_autoAdvanceState = AutoAdvanceState{
        .generation = m_trackGeneration,
    };
    m_drainFillPrepareDiagnostic = {};
}

void AudioEngine::rememberAutoTransitionMode(const AutoTransitionMode mode)
{
    if(mode == AutoTransitionMode::None) {
        return;
    }

    m_autoAdvanceState.generation = m_trackGeneration;
    m_autoAdvanceState.mode       = mode;
}

AutoTransitionMode AudioEngine::effectiveAutoTransitionMode() const
{
    const auto liveMode = m_transitions.autoTransitionMode();
    if(liveMode != AutoTransitionMode::None) {
        return liveMode;
    }

    if(m_autoAdvanceState.generation != m_trackGeneration) {
        return AutoTransitionMode::None;
    }

    return m_autoAdvanceState.mode;
}

AutoTransitionMode AudioEngine::configuredTrackEndAutoTransitionMode() const
{
    return configuredTrackEndAutoTransitionMode(m_currentTrack);
}

AutoTransitionMode AudioEngine::configuredTrackEndAutoTransitionMode(const Track& track) const
{
    if(!m_trackEndAutoTransitionEnabled) {
        return AutoTransitionMode::None;
    }

    const bool crossfadeConfigured   = m_crossfadeEnabled && m_crossfadingValues.autoChange.isConfigured();
    const uint64_t trackDurationMs   = track.duration();
    const bool trackDurationKnown    = trackDurationMs > 0;
    const uint64_t crossfadeWindowMs = static_cast<uint64_t>(std::max(
        {0, m_crossfadingValues.autoChange.effectiveOutMs(), 0, m_crossfadingValues.autoChange.effectiveInMs()}));
    if(crossfadeConfigured && (!trackDurationKnown || trackDurationMs >= crossfadeWindowMs)) {
        return AutoTransitionMode::Crossfade;
    }

    const bool boundaryFadeConfigured   = m_fadingEnabled && m_fadingValues.boundary.isConfigured();
    const uint64_t boundaryFadeWindowMs = static_cast<uint64_t>(std::max(0, m_fadingValues.boundary.effectiveOutMs()));
    if(boundaryFadeConfigured && (!trackDurationKnown || trackDurationMs >= boundaryFadeWindowMs)) {
        return AutoTransitionMode::BoundaryFade;
    }

    if(m_gaplessEnabled) {
        return AutoTransitionMode::Gapless;
    }

    return AutoTransitionMode::None;
}

uint64_t AudioEngine::scaledPlaybackDelayMs() const
{
    return scaledDelayMs(m_pipeline.playbackDelayMs(), m_pipeline.playbackDelayToTrackScale());
}

uint64_t AudioEngine::scaledTransitionDelayMs() const
{
    return scaledDelayMs(m_pipeline.transitionPlaybackDelayMs(), m_pipeline.playbackDelayToTrackScale());
}

uint64_t AudioEngine::boundaryCrossfadeOverlapMs() const
{
    if(m_crossfadeSwitchPolicy != Engine::CrossfadeSwitchPolicy::Boundary
       || configuredTrackEndAutoTransitionMode() != AutoTransitionMode::Crossfade) {
        return 0;
    }

    const uint64_t fadeOutMs = static_cast<uint64_t>(std::max(0, m_crossfadingValues.autoChange.effectiveOutMs()));
    const uint64_t fadeInMs  = static_cast<uint64_t>(std::max(0, m_crossfadingValues.autoChange.effectiveInMs()));
    return std::min(fadeOutMs, fadeInMs);
}

uint64_t AudioEngine::transitionReserveMs() const
{
    const auto reserveMs             = static_cast<uint64_t>(std::max(1, m_playbackBufferLengthMs));
    const uint64_t boundaryOverlapMs = boundaryCrossfadeOverlapMs();
    if(boundaryOverlapMs == 0) {
        return reserveMs;
    }

    const uint64_t outputDelayMs = std::max(scaledPlaybackDelayMs(), scaledTransitionDelayMs());
    const auto requiredReserveMs
        = saturatingAdd(saturatingAdd(boundaryOverlapMs, outputDelayMs), PrefillSafetyMarginMs);
    return std::max(reserveMs, requiredReserveMs);
}

uint64_t AudioEngine::aggressivePreparedPrefillMs() const
{
    return std::max(preferredPreparedPrefillMs(), transitionReserveMs());
}

Engine::PlaybackItem AudioEngine::autoAdvanceTargetTrack() const
{
    const auto currentItem = currentPlaybackItem();

    if(m_upcomingTrackCandidate.isValid() && !samePlaybackItem(upcomingTrackCandidateItem(), currentItem)) {
        return upcomingTrackCandidateItem();
    }

    if(m_preparedCrossfadeTransition.active && m_preparedCrossfadeTransition.sourceGeneration == m_trackGeneration
       && m_preparedCrossfadeTransition.targetTrack.isValid()
       && !samePlaybackItem(preparedCrossfadeTargetItem(), currentItem)) {
        return preparedCrossfadeTargetItem();
    }

    if(m_preparedGaplessTransition.active && m_preparedGaplessTransition.sourceGeneration == m_trackGeneration
       && m_preparedGaplessTransition.targetTrack.isValid()
       && !samePlaybackItem(preparedGaplessTargetItem(), currentItem)) {
        return preparedGaplessTargetItem();
    }

    if(m_preparedNext && m_preparedNext->item.track.isValid() && !samePlaybackItem(m_preparedNext->item, currentItem)) {
        return m_preparedNext->item;
    }

    return {};
}

void AudioEngine::maybeLogDrainFillPrepareGate(const AudioStreamPtr& stream, const TrackEndingResult& result)
{
    if(!stream) {
        return;
    }

    if(!result.aboutToFinish && !result.readyToSwitch && !result.boundaryReached && !result.endReached) {
        return;
    }

    if(!m_upcomingTrackCandidate.isValid()) {
        logDrainFillPrepareDiagnostic(DrainFillPrepareDiagnosticReason::NoUpcomingCandidate, stream);
        return;
    }

    if(samePlaybackItem(upcomingTrackCandidateItem(), currentPlaybackItem())) {
        logDrainFillPrepareDiagnostic(DrainFillPrepareDiagnosticReason::CandidateMatchesCurrent, stream);
        return;
    }

    if(isMultiTrackFileTransition(m_currentTrack, m_upcomingTrackCandidate)) {
        logDrainFillPrepareDiagnostic(DrainFillPrepareDiagnosticReason::MultiTrackFileTransition, stream);
        return;
    }

    if(configuredTrackEndAutoTransitionMode() == AutoTransitionMode::None) {
        logDrainFillPrepareDiagnostic(DrainFillPrepareDiagnosticReason::AutoTransitionDisabled, stream);
        return;
    }

    if(!stream->endOfInput()) {
        logDrainFillPrepareDiagnostic(DrainFillPrepareDiagnosticReason::WaitingForEndOfInput, stream,
                                      aggressivePreparedPrefillMs());
    }
}

void AudioEngine::logDrainFillPrepareDiagnostic(DrainFillPrepareDiagnosticReason reason, const AudioStreamPtr& stream,
                                                uint64_t prefillTargetMs)
{
    const int candidateTrackId = m_upcomingTrackCandidate.id();
    if(m_drainFillPrepareDiagnostic.generation == m_trackGeneration && m_drainFillPrepareDiagnostic.reason == reason
       && m_drainFillPrepareDiagnostic.candidateTrackId == candidateTrackId) {
        return;
    }

    m_drainFillPrepareDiagnostic = {
        .generation       = m_trackGeneration,
        .reason           = reason,
        .candidateTrackId = candidateTrackId,
    };

    const auto preparedStream = (m_preparedNext ? m_preparedNext->preparedStream : AudioStreamPtr{});
    const auto relativePosMs
        = stream ? relativeTrackPositionMs(stream->positionMs(), m_streamToTrackOriginMs, m_currentTrack.offset()) : 0;

    qCDebug(ENGINE) << "Gapless drain-fill preparation state:"
                    << "reason=" << Utils::Enum::toString(reason) << "trackId=" << m_currentTrack.id()
                    << "generation=" << m_trackGeneration << "trackPosMs=" << relativePosMs
                    << "trackDurationMs=" << m_currentTrack.duration()
                    << "currentStreamId=" << (stream ? stream->id() : InvalidStreamId)
                    << "streamEndOfInput=" << (stream ? stream->endOfInput() : false)
                    << "streamBufferEmpty=" << (stream ? stream->bufferEmpty() : true)
                    << "streamBufferedMs=" << (stream ? stream->bufferedDurationMs() : 0)
                    << "candidateTrackId=" << candidateTrackId << "candidateItemId=" << m_upcomingTrackCandidateItemId
                    << "preparedTrackId=" << (m_preparedNext ? m_preparedNext->item.track.id() : 0)
                    << "preparedItemId=" << (m_preparedNext ? m_preparedNext->item.itemId : 0)
                    << "preparedStreamBufferedMs=" << (preparedStream ? preparedStream->bufferedDurationMs() : 0)
                    << "drainPrepareRequested=" << m_autoAdvanceState.drainPrepareRequested
                    << "configuredMode=" << Utils::Enum::toString(configuredTrackEndAutoTransitionMode())
                    << "effectiveMode=" << Utils::Enum::toString(effectiveAutoTransitionMode())
                    << "prefillTargetMs=" << prefillTargetMs;
}

void AudioEngine::maybePrepareUpcomingTrackForDrainFill(const AudioStreamPtr& stream)
{
    if(!stream || !stream->endOfInput()) {
        return;
    }

    if(!m_upcomingTrackCandidate.isValid()) {
        logDrainFillPrepareDiagnostic(DrainFillPrepareDiagnosticReason::NoUpcomingCandidate, stream);
        return;
    }

    if(samePlaybackItem(upcomingTrackCandidateItem(), currentPlaybackItem())) {
        logDrainFillPrepareDiagnostic(DrainFillPrepareDiagnosticReason::CandidateMatchesCurrent, stream);
        return;
    }

    if(isMultiTrackFileTransition(m_currentTrack, m_upcomingTrackCandidate)) {
        logDrainFillPrepareDiagnostic(DrainFillPrepareDiagnosticReason::MultiTrackFileTransition, stream);
        return;
    }

    if(m_preparedCrossfadeTransition.active && m_preparedCrossfadeTransition.sourceGeneration == m_trackGeneration
       && samePlaybackItem(preparedCrossfadeTargetItem(), upcomingTrackCandidateItem())) {
        logDrainFillPrepareDiagnostic(DrainFillPrepareDiagnosticReason::PreparedCrossfadeAlreadyActive, stream);
        return;
    }

    if(m_preparedGaplessTransition.active && m_preparedGaplessTransition.sourceGeneration == m_trackGeneration
       && samePlaybackItem(preparedGaplessTargetItem(), upcomingTrackCandidateItem())) {
        logDrainFillPrepareDiagnostic(DrainFillPrepareDiagnosticReason::PreparedGaplessAlreadyActive, stream);
        return;
    }

    if(configuredTrackEndAutoTransitionMode() == AutoTransitionMode::None) {
        logDrainFillPrepareDiagnostic(DrainFillPrepareDiagnosticReason::AutoTransitionDisabled, stream);
        return;
    }

    if(m_autoAdvanceState.generation != m_trackGeneration) {
        clearAutoAdvanceState();
    }

    if(m_autoAdvanceState.mode == AutoTransitionMode::None) {
        rememberAutoTransitionMode(configuredTrackEndAutoTransitionMode());
    }

    const uint64_t aggressivePrefillMs = aggressivePreparedPrefillMs();
    if(m_preparedNext && samePlaybackItem(m_preparedNext->item, upcomingTrackCandidateItem())
       && m_preparedNext->format.isValid() && m_preparedNext->preparedStream
       && m_preparedNext->preparedStream->bufferedDurationMs() >= aggressivePrefillMs) {
        logDrainFillPrepareDiagnostic(DrainFillPrepareDiagnosticReason::AlreadyBuffered, stream, aggressivePrefillMs);
        return;
    }

    if(m_autoAdvanceState.drainPrepareRequested) {
        logDrainFillPrepareDiagnostic(DrainFillPrepareDiagnosticReason::AlreadyRequested, stream, aggressivePrefillMs);
        return;
    }

    m_autoAdvanceState.generation            = m_trackGeneration;
    m_autoAdvanceState.drainPrepareRequested = true;

    clearPreparedNextTrack();
    enqueuePrepareNextTrack(upcomingTrackCandidateItem(), 0, aggressivePrefillMs);
    logDrainFillPrepareDiagnostic(DrainFillPrepareDiagnosticReason::Enqueued, stream, aggressivePrefillMs);
}

void AudioEngine::noteOverlapStartAnchor()
{
    m_autoAdvanceState.generation             = m_trackGeneration;
    m_autoAdvanceState.overlapStartAnchorSeen = true;
}

void AudioEngine::noteOverlapMidpointAnchor()
{
    m_autoAdvanceState.generation                = m_trackGeneration;
    m_autoAdvanceState.overlapMidpointAnchorSeen = true;
}

void AudioEngine::noteBoundaryAnchor()
{
    m_autoAdvanceState.generation         = m_trackGeneration;
    m_autoAdvanceState.boundaryAnchorSeen = true;
}

Engine::TrackCommitContext AudioEngine::makeTrackCommitContext(const Engine::TransitionMode mode,
                                                               const uint64_t audibleDelayMs) const
{
    return Engine::TrackCommitContext{
        .track          = m_currentTrack,
        .itemId         = m_currentTrackItemId,
        .generation     = m_trackGeneration,
        .mode           = mode,
        .audibleDelayMs = audibleDelayMs,
    };
}

void AudioEngine::clearPendingAudibleTrackCommit()
{
    m_pendingAudibleTrackCommit = {};
}

void AudioEngine::finaliseTrackCommitCleanup()
{
    if(samePlaybackItem(upcomingTrackCandidateItem(), currentPlaybackItem())) {
        m_upcomingTrackCandidate       = {};
        m_upcomingTrackCandidateItemId = 0;
    }

    clearAutoAdvanceState();
}

void AudioEngine::finaliseTrackCommit(const Engine::TransitionMode mode, const uint64_t audibleDelayMs)
{
    finaliseTrackCommitCleanup();
    emit trackCommitted(makeTrackCommitContext(mode, audibleDelayMs));
}

bool AudioEngine::finaliseTrackCommitWhenAudible(const Engine::TransitionMode mode, const StreamId streamId)
{
    if(streamId == InvalidStreamId) {
        finaliseTrackCommit(mode);
        return true;
    }

    finaliseTrackCommitCleanup();
    m_pendingAudibleTrackCommit = {
        .active   = true,
        .context  = makeTrackCommitContext(mode),
        .streamId = streamId,
    };

    qCDebug(ENGINE) << "Deferring track commit until prepared stream becomes audible:"
                    << "trackId=" << m_currentTrack.id() << "itemId=" << m_currentTrackItemId
                    << "generation=" << m_trackGeneration << "streamId=" << streamId;
    return false;
}

void AudioEngine::maybeEmitPendingAudibleTrackCommit(const StreamId audibleOutputStreamId)
{
    if(!m_pendingAudibleTrackCommit.active || audibleOutputStreamId == InvalidStreamId
       || audibleOutputStreamId != m_pendingAudibleTrackCommit.streamId) {
        return;
    }

    const auto context = m_pendingAudibleTrackCommit.context;
    clearPendingAudibleTrackCommit();

    qCDebug(ENGINE) << "Deferred track commit released after prepared stream became audible:"
                    << "trackId=" << context.track.id() << "itemId=" << context.itemId
                    << "generation=" << context.generation << "streamId=" << audibleOutputStreamId;
    emit trackCommitted(context);
}

void AudioEngine::tryAutoAdvanceCommit()
{
    const Engine::PlaybackItem target = autoAdvanceTargetTrack();
    if(!target.isValid() || m_autoAdvanceState.generation != m_trackGeneration) {
        return;
    }

    const bool waitingForDrainPrepare = m_autoAdvanceState.drainPrepareRequested;

    switch(effectiveAutoTransitionMode()) {
        case AutoTransitionMode::Crossfade: {
            const bool overlapStartAnchorSeen    = m_autoAdvanceState.overlapStartAnchorSeen;
            const bool overlapMidpointAnchorSeen = m_autoAdvanceState.overlapMidpointAnchorSeen;
            const bool boundarySeen              = m_autoAdvanceState.boundaryAnchorSeen;

            if(!boundarySeen && !overlapStartAnchorSeen && !overlapMidpointAnchorSeen) {
                return;
            }

            const bool armedPreparedCrossfade = armPreparedCrossfadeTransition(target, m_trackGeneration);
            const bool commitAnchorSeen
                = (m_crossfadeSwitchPolicy == Engine::CrossfadeSwitchPolicy::OverlapStart && overlapStartAnchorSeen)
               || (m_crossfadeSwitchPolicy == Engine::CrossfadeSwitchPolicy::OverlapMidpoint
                   && overlapMidpointAnchorSeen);

            if(commitAnchorSeen && armedPreparedCrossfade && commitPreparedCrossfadeTransition(target)) {
                return;
            }

            if(!boundarySeen) {
                return;
            }

            if(armedPreparedCrossfade && commitPreparedCrossfadeTransition(target)) {
                return;
            }

            if(waitingForDrainPrepare) {
                cancelPendingPrepareJobs();
                m_autoAdvanceState.drainPrepareRequested = false;
            }

            const bool preparedImmediately = prepareNextTrackImmediate(target, aggressivePreparedPrefillMs());
            const bool armedImmediately
                = preparedImmediately && armPreparedCrossfadeTransition(target, m_trackGeneration);

            if(armedImmediately && commitPreparedCrossfadeTransition(target)) {
                return;
            }

            loadTrack(target, false);
            return;
        }
        case AutoTransitionMode::Gapless:
        case AutoTransitionMode::BoundaryFade: {
            const bool armedPreparedGapless = armPreparedGaplessTransition(target, m_trackGeneration);

            if(!m_autoAdvanceState.boundaryAnchorSeen) {
                return;
            }

            if(armedPreparedGapless && commitPreparedGaplessTransition(target)) {
                return;
            }

            if(waitingForDrainPrepare) {
                cancelPendingPrepareJobs();
                m_autoAdvanceState.drainPrepareRequested = false;
            }

            if(prepareNextTrackImmediate(target, aggressivePreparedPrefillMs())
               && armPreparedGaplessTransition(target, m_trackGeneration) && commitPreparedGaplessTransition(target)) {
                return;
            }

            loadTrack(target, false);
            return;
        }
        case AutoTransitionMode::None:
            return;
    }
}

void AudioEngine::handleTrackEndingSignals(const AudioStreamPtr& stream, uint64_t trackEndingPosMs,
                                           uint64_t publishedAudiblePosMs, uint64_t boundaryAudiblePosMs,
                                           bool preparedCrossfadeArmed, bool boundaryFallbackReached,
                                           bool preparedGaplessRendered)
{
    const uint64_t initialGeneration = m_trackGeneration;
    const auto transitionModeBefore  = m_transitions.autoTransitionMode();
    const auto result                = checkTrackEnding(stream, trackEndingPosMs, boundaryAudiblePosMs);
    const auto transitionMode        = m_transitions.autoTransitionMode();

    rememberAutoTransitionMode(transitionModeBefore);
    rememberAutoTransitionMode(transitionMode);
    maybeLogDrainFillPrepareGate(stream, result);
    maybePrepareUpcomingTrackForDrainFill(stream);

    const bool crossfadeUsesAudibleTimeline = transitionMode == AutoTransitionMode::Crossfade
                                           && m_crossfadeSwitchPolicy != Engine::CrossfadeSwitchPolicy::Boundary;

    if(result.aboutToFinish) {
        const uint64_t crossfadeAnchorPosMs = crossfadeUsesAudibleTimeline ? publishedAudiblePosMs : trackEndingPosMs;
        maybeBeginAutoCrossfadeTailFadeOut(stream, crossfadeAnchorPosMs);
        maybeBeginAutoBoundaryFadeOut(trackEndingPosMs);
        emit trackAboutToFinish(m_currentTrack, m_trackGeneration);
    }

    if(effectiveAutoTransitionMode() == AutoTransitionMode::BoundaryFade && !m_autoBoundaryFadeActive) {
        maybeBeginAutoBoundaryFadeOut(trackEndingPosMs);
    }

    if(result.readyToSwitch && transitionMode == AutoTransitionMode::Crossfade) {
        noteOverlapStartAnchor();
        tryAutoAdvanceCommit();
        emit trackReadyToSwitch(m_currentTrack, m_trackGeneration);
    }

    const bool preparedCrossfadeMatchesCurrentTrack
        = m_preparedCrossfadeTransition.active && m_preparedCrossfadeTransition.sourceGeneration == m_trackGeneration;

    if(crossfadeUsesAudibleTimeline && m_crossfadeSwitchPolicy == Engine::CrossfadeSwitchPolicy::OverlapMidpoint
       && m_autoAdvanceState.overlapStartAnchorSeen && !m_autoAdvanceState.overlapMidpointAnchorSeen
       && preparedCrossfadeMatchesCurrentTrack) {
        const uint64_t currentCrossfadePosMs{publishedAudiblePosMs};
        const uint64_t remainingToBoundaryMs = (m_currentTrack.duration() > currentCrossfadePosMs)
                                                 ? (m_currentTrack.duration() - currentCrossfadePosMs)
                                                 : 0;
        if(remainingToBoundaryMs <= m_preparedCrossfadeTransition.overlapMidpointLeadMs) {
            noteOverlapMidpointAnchor();
            tryAutoAdvanceCommit();
        }
    }

    if(effectiveAutoTransitionMode() == AutoTransitionMode::Crossfade && m_autoAdvanceState.overlapStartAnchorSeen
       && !m_autoAdvanceState.boundaryAnchorSeen && !m_preparedCrossfadeTransition.active) {
        tryAutoAdvanceCommit();
    }

    const auto effectiveMode = effectiveAutoTransitionMode();
    const bool gaplessBoundaryMode
        = effectiveMode == AutoTransitionMode::Gapless || effectiveMode == AutoTransitionMode::BoundaryFade;
    const bool preparedGaplessActive
        = m_preparedGaplessTransition.active && m_preparedGaplessTransition.sourceGeneration == m_trackGeneration;
    const StreamId audibleOutputStreamId = m_pipeline.audibleOutputStreamId();
    const bool preparedGaplessRenderedSafe
        = preparedGaplessRendered && audibleOutputStreamId == m_preparedGaplessTransition.streamId;
    const bool currentStreamFullyDrained   = stream && stream->endOfInput() && stream->bufferEmpty();
    const bool preparedGaplessStageReady   = currentStreamFullyDrained && preparedGaplessRendered;
    const bool preparedGaplessReleaseReady = preparedGaplessRenderedSafe;
    const bool deferPreparedGaplessCommit  = gaplessBoundaryMode && preparedGaplessActive && !preparedGaplessRendered;
    const bool deferRenderedGaplessCommit
        = gaplessBoundaryMode && preparedGaplessActive && preparedGaplessRendered && !preparedGaplessReleaseReady;
    const bool releasePreparedGaplessCommit = gaplessBoundaryMode && preparedGaplessActive
                                           && m_autoAdvanceState.boundaryAnchorSeen && preparedGaplessReleaseReady;

    if(preparedGaplessActive && preparedGaplessStageReady && !m_preparedGaplessTransition.decoderAdopted) {
        if(!stagePreparedGaplessDecoder(preparedGaplessTargetItem())) {
            return;
        }
    }

    if(releasePreparedGaplessCommit) {
        qCDebug(ENGINE) << "Prepared gapless commit released after prepared stream became audible:"
                        << "trackId=" << m_currentTrack.id() << "generation=" << m_trackGeneration
                        << "preparedStreamId=" << m_preparedGaplessTransition.streamId
                        << "audibleOutputStreamId=" << audibleOutputStreamId;
        tryAutoAdvanceCommit();
        if(m_trackGeneration != initialGeneration) {
            return;
        }
    }

    const bool pendingBoundaryRenderedGaplessReached
        = m_autoAdvanceState.boundaryPendingUntilAudible && preparedGaplessActive && preparedGaplessReleaseReady;
    const bool cueBoundaryMode        = m_currentTrack.hasCue();
    const bool audibleBoundaryReached = m_currentTrack.duration() == 0
                                     || boundaryAudiblePosMs >= m_currentTrack.duration()
                                     || pendingBoundaryRenderedGaplessReached;

    const bool deferBoundaryUntilAudible = preparedGaplessActive || cueBoundaryMode;
    const bool boundaryWasPending        = m_autoAdvanceState.boundaryPendingUntilAudible;
    if(result.boundaryReached && !audibleBoundaryReached && deferBoundaryUntilAudible) {
        m_autoAdvanceState.boundaryPendingUntilAudible = true;
        if(!boundaryWasPending && preparedGaplessActive) {
            const StreamId currentStreamId = stream ? stream->id() : InvalidStreamId;
            qCDebug(ENGINE) << "Gapless boundary waiting for prepared stream to become audible:"
                            << "trackId=" << m_currentTrack.id() << "generation=" << m_trackGeneration
                            << "currentStreamId=" << currentStreamId
                            << "preparedStreamId=" << m_preparedGaplessTransition.streamId
                            << "audibleOutputStreamId=" << audibleOutputStreamId
                            << "preparedRendered=" << preparedGaplessRendered
                            << "decoderAdopted=" << m_preparedGaplessTransition.decoderAdopted;
            scheduleGaplessBoundaryStallDiagnostic(m_trackGeneration, currentStreamId,
                                                   m_preparedGaplessTransition.streamId);
        }
    }

    const bool boundaryReachedNow = (result.boundaryReached && (audibleBoundaryReached || !deferBoundaryUntilAudible))
                                 || (m_autoAdvanceState.boundaryPendingUntilAudible && audibleBoundaryReached);

    const auto emitBoundarySignal = [&]() -> bool {
        m_autoAdvanceState.boundaryPendingUntilAudible = false;
        uint64_t boundaryRemainingOutputMs{0};

        if(stream) {
            const uint64_t playbackDelayMs = m_pipeline.playbackDelayMs();

            boundaryRemainingOutputMs = stream->bufferedDurationMs();
            if(boundaryRemainingOutputMs > std::numeric_limits<uint64_t>::max() - playbackDelayMs) {
                boundaryRemainingOutputMs = std::numeric_limits<uint64_t>::max();
            }
            else {
                boundaryRemainingOutputMs += playbackDelayMs;
            }
        }

        const Track boundaryTrack{m_currentTrack};
        const uint64_t boundaryGeneration{m_trackGeneration};
        const bool boundaryEngineOwnsTransition
            = preparedCrossfadeArmed || deferPreparedGaplessCommit || deferRenderedGaplessCommit;
        const bool cueLogicalBoundary = cueBoundaryMode && audibleBoundaryReached;

        if(boundaryEngineOwnsTransition || cueLogicalBoundary) {
            boundaryRemainingOutputMs = 0;
        }
        else if(m_currentTrack.duration() > boundaryAudiblePosMs) {
            boundaryRemainingOutputMs
                = std::max(boundaryRemainingOutputMs, m_currentTrack.duration() - boundaryAudiblePosMs);
        }

        m_preparedCrossfadeTransition.boundarySignalled = true;
        noteBoundaryAnchor();
        qCDebug(ENGINE) << "Track boundary emitted:" << "trackId=" << boundaryTrack.id()
                        << "generation=" << boundaryGeneration << "remainingOutputMs=" << boundaryRemainingOutputMs
                        << "engineOwnsTransition=" << boundaryEngineOwnsTransition
                        << "candidateTrackId=" << m_upcomingTrackCandidate.id()
                        << "effectiveMode=" << Utils::Enum::toString(effectiveAutoTransitionMode())
                        << "currentStreamId=" << (stream ? stream->id() : InvalidStreamId)
                        << "streamEndOfInput=" << (stream ? stream->endOfInput() : false)
                        << "streamBufferEmpty=" << (stream ? stream->bufferEmpty() : true)
                        << "streamBufferedMs=" << (stream ? stream->bufferedDurationMs() : 0)
                        << "cueBoundaryMode=" << cueBoundaryMode << "boundaryAudiblePosMs=" << boundaryAudiblePosMs;

        emit trackBoundaryReached(boundaryTrack, boundaryGeneration, boundaryRemainingOutputMs,
                                  boundaryEngineOwnsTransition);

        if(deferPreparedGaplessCommit || deferRenderedGaplessCommit) {
            return true;
        }

        tryAutoAdvanceCommit();
        return m_trackGeneration == boundaryGeneration && sameTrackIdentity(m_currentTrack, boundaryTrack);
    };

    if(boundaryReachedNow || (preparedCrossfadeArmed && boundaryFallbackReached)) {
        if(!emitBoundarySignal()) {
            return;
        }
    }
    if(result.endReached) {
        if(m_trackGeneration != initialGeneration) {
            return;
        }

        if(!m_autoAdvanceState.boundaryAnchorSeen && !m_autoAdvanceState.boundaryPendingUntilAudible) {
            qCWarning(ENGINE) << "Track reached end without prior boundary signal:"
                              << "trackId=" << m_currentTrack.id() << "generation=" << m_trackGeneration
                              << "candidateTrackId=" << m_upcomingTrackCandidate.id()
                              << "effectiveMode=" << Utils::Enum::toString(effectiveAutoTransitionMode())
                              << "configuredMode=" << Utils::Enum::toString(configuredTrackEndAutoTransitionMode())
                              << "resultBoundaryReached=" << result.boundaryReached
                              << "currentStreamId=" << (stream ? stream->id() : InvalidStreamId)
                              << "streamEndOfInput=" << (stream ? stream->endOfInput() : false)
                              << "streamBufferEmpty=" << (stream ? stream->bufferEmpty() : true)
                              << "streamBufferedMs=" << (stream ? stream->bufferedDurationMs() : 0)
                              << "trackEndingPosMs=" << trackEndingPosMs
                              << "boundaryAudiblePosMs=" << boundaryAudiblePosMs;
        }

        const bool deferTrackEndStatusForPendingGaplessBoundary
            = ((gaplessBoundaryMode && preparedGaplessActive) || cueBoundaryMode)
           && (!m_autoAdvanceState.boundaryAnchorSeen || m_autoAdvanceState.boundaryPendingUntilAudible);
        if(deferTrackEndStatusForPendingGaplessBoundary) {
            qCDebug(ENGINE) << "Deferring track-end status until pending boundary resolves:"
                            << "trackId=" << m_currentTrack.id() << "generation=" << m_trackGeneration
                            << "candidateTrackId=" << m_upcomingTrackCandidate.id()
                            << "preparedStreamId=" << m_preparedGaplessTransition.streamId
                            << "currentStreamId=" << (stream ? stream->id() : InvalidStreamId)
                            << "cueBoundaryMode=" << cueBoundaryMode
                            << "boundaryAnchorSeen=" << m_autoAdvanceState.boundaryAnchorSeen
                            << "boundaryPendingUntilAudible=" << m_autoAdvanceState.boundaryPendingUntilAudible
                            << "preparedGaplessRendered=" << preparedGaplessRendered;
            return;
        }

        // Same-file logical segment transitions can reach "track end" before
        // the underlying stream is actually drained. In that case, avoid
        // flushing DSP state.
        const bool shouldFlushDspOnEnd = stream->endOfInput() && stream->bufferEmpty();
        signalTrackEndOnce(shouldFlushDspOnEnd);
    }
}

void AudioEngine::updatePosition()
{
    auto stream = currentTrackTimingStream();
    if(!stream) {
        return;
    }

    const uint64_t pipelineDelayMs  = m_pipeline.playbackDelayMs();
    const double delayToSourceScale = m_pipeline.playbackDelayToTrackScale();
    const auto pipelineStatus       = m_pipeline.currentStatus();
    const bool preparedCrossfadeArmed
        = m_preparedCrossfadeTransition.active && m_preparedCrossfadeTransition.sourceGeneration == m_trackGeneration;

    PositionCoordinator::Input input;
    input.playbackState                       = playbackState();
    input.seekInProgress                      = m_transitions.isSeekInProgress();
    input.decoderTimerActive                  = m_decoder.isDecodeTimerActive();
    input.decoderLowWatermarkMs               = m_decoder.lowWatermarkMs();
    input.streamId                            = stream->id();
    input.streamState                         = stream->state();
    input.streamEndOfInput                    = stream->endOfInput();
    input.streamBufferedDurationMs            = stream->bufferedDurationMs();
    input.streamPositionMs                    = stream->positionMs();
    input.streamToTrackOriginMs               = m_streamToTrackOriginMs;
    input.trackOffsetMs                       = m_currentTrack.offset();
    input.pipelineDelayMs                     = pipelineDelayMs;
    input.delayToSourceScale                  = delayToSourceScale;
    input.pipelineStatus                      = pipelineStatus;
    input.preparedCrossfade.armed             = preparedCrossfadeArmed;
    input.preparedCrossfade.streamId          = m_preparedCrossfadeTransition.streamId;
    input.preparedCrossfade.bufferedAtArmMs   = m_preparedCrossfadeTransition.bufferedAtArmMs;
    input.preparedCrossfade.boundaryLeadMs    = m_preparedCrossfadeTransition.boundaryLeadMs;
    input.preparedCrossfade.boundarySignalled = m_preparedCrossfadeTransition.boundarySignalled;

    const auto output = m_positionCoordinator.evaluate(input);

    if(output.shouldEnsureDecodeTimer) {
        m_decoder.ensureDecodeTimerRunning();
    }

    if(!output.positionAvailable) {
        return;
    }

    updatePositionContext(pipelineStatus.timelineEpoch);

    const auto updateMode
        = output.discontinuity ? AudioClock::UpdateMode::Discontinuity : AudioClock::UpdateMode::Continuous;
    publishPosition(output.relativePosMs, pipelineDelayMs, delayToSourceScale, updateMode, output.emitNow);

    bool boundaryFallbackReached = output.boundaryFallbackReached;
    if(!boundaryFallbackReached && output.preparedCrossfadeArmed && !m_preparedCrossfadeTransition.boundarySignalled
       && m_preparedNext && m_preparedNext->preparedStream
       && m_preparedNext->preparedStream->id() == m_preparedCrossfadeTransition.streamId) {
        const uint64_t preparedBufferedMs = m_preparedNext->preparedStream->bufferedDurationMs();
        const uint64_t consumedMs         = (m_preparedCrossfadeTransition.bufferedAtArmMs > preparedBufferedMs)
                                              ? (m_preparedCrossfadeTransition.bufferedAtArmMs - preparedBufferedMs)
                                              : 0;
        boundaryFallbackReached           = consumedMs >= m_preparedCrossfadeTransition.boundaryLeadMs;
    }

    const bool preparedGaplessRendered = m_preparedGaplessTransition.active
                                      && m_preparedGaplessTransition.sourceGeneration == m_trackGeneration
                                      && pipelineStatus.renderedSegment.valid
                                      && pipelineStatus.renderedSegment.streamId == m_preparedGaplessTransition.streamId
                                      && pipelineStatus.renderedSegment.outputFrames > 0;

    if(output.evaluateTrackEnding) {
        uint64_t boundaryAudiblePosMs  = output.relativePosMs;
        const bool hasMappedAudiblePos = pipelineStatus.positionIsMapped || output.hasRenderedSegment;

        if(!hasMappedAudiblePos || m_currentTrack.hasCue()) {
            uint64_t timelineDelayMs{pipelineDelayMs};
            if(timelineDelayMs > 0) {
                timelineDelayMs = scaledDelayMs(timelineDelayMs, delayToSourceScale);
            }

            const uint64_t estimatedAudiblePosMs
                = output.trackEndingPosMs > timelineDelayMs ? (output.trackEndingPosMs - timelineDelayMs) : 0;
            boundaryAudiblePosMs = m_currentTrack.hasCue() ? estimatedAudiblePosMs
                                                           : std::max(estimatedAudiblePosMs, boundaryAudiblePosMs);
        }

        handleTrackEndingSignals(stream, output.trackEndingPosMs, output.relativePosMs, boundaryAudiblePosMs,
                                 output.preparedCrossfadeArmed, boundaryFallbackReached, preparedGaplessRendered);
    }
}

void AudioEngine::handleTimerTick(int timerId)
{
    if(timerId == m_vbrUpdateTimerId) {
        syncDecoderBitrate();
        return;
    }

    if(auto decodeResult = m_decoder.handleTimer(timerId, m_transitions.isSeekInProgress())) {
        syncDecoderTrackMetadata();
        syncDecoderBitrate();
        checkPendingSeek();
        cleanupOrphanedStream();

        if(decodeResult->stopDecodeTimer) {
            m_decoder.stopDecodeTimer();
        }
    }
}

void AudioEngine::clearPendingAnalysisData()
{
    if(m_analysisBus) {
        m_analysisBus->flush();
    }

    m_levelFrameMailbox.requestReset();
    m_levelFrameDispatchQueued.store(false, std::memory_order_relaxed);
}

void AudioEngine::onLevelFrameReady(const LevelFrame& frame)
{
    auto writer              = m_levelFrameMailbox.writer();
    const size_t framesWrote = writer.write(&frame, 1, RingBufferOverflowPolicy::OverwriteOldest);

    if(framesWrote == 0) {
        return;
    }

    if(!m_levelFrameDispatchQueued.exchange(true, std::memory_order_relaxed)) {
        QMetaObject::invokeMethod(this, [this]() { dispatchPendingLevelFrames(); }, Qt::QueuedConnection);
    }
}

void AudioEngine::handleOutputStateChange(const AudioOutput::State state)
{
    if(state != AudioOutput::State::Disconnected || !m_format.isValid() || !m_pipeline.hasOutput()) {
        return;
    }

    qCInfo(ENGINE) << "Audio output disconnected; attempting to reinit current output";
    reinitOutputForCurrentFormat();
}

void AudioEngine::dispatchPendingLevelFrames()
{
    auto reader = m_levelFrameMailbox.reader();

    LevelFrame latestFrame;
    bool hasFrame = false;

    while(reader.read(&latestFrame, 1) == 1) {
        hasFrame = true;
    }

    if(hasFrame) {
        emit levelReady(latestFrame);
    }

    m_levelFrameDispatchQueued.store(false, std::memory_order_relaxed);

    if(m_levelFrameMailbox.readAvailable() > 0
       && !m_levelFrameDispatchQueued.exchange(true, std::memory_order_relaxed)) {
        QMetaObject::invokeMethod(this, [this]() { dispatchPendingLevelFrames(); }, Qt::QueuedConnection);
    }
}

void AudioEngine::handlePipelineWakeSignals(const AudioPipeline::PendingSignals& pendingSignals)
{
    if(pendingSignals.fadeEventsPending) {
        constexpr size_t FadeDrainBatchSize = 16;
        std::array<AudioPipeline::FadeEvent, FadeDrainBatchSize> fadeEvents{};

        while(true) {
            const size_t drained = m_pipeline.drainFadeEvents(fadeEvents.data(), fadeEvents.size());

            for(size_t i{0}; i < drained; ++i) {
                handlePipelineFadeEvent(fadeEvents[i]);
            }

            if(drained < fadeEvents.size()) {
                break;
            }
        }
    }

    if(pendingSignals.needsData) {
        m_decoder.ensureDecodeTimerRunning();
    }
}

void AudioEngine::schedulePipelineWakeDrainTask()
{
    if(m_engineTaskQueue.isShuttingDown()) {
        return;
    }

    if(m_pipelineWakeTaskQueued.exchange(true, std::memory_order_relaxed)) {
        return;
    }

    const bool enqueued = m_engineTaskQueue.enqueue(EngineTaskType::PipelineWake, [engine = this]() {
        engine->m_pipelineWakeTaskQueued.store(false, std::memory_order_relaxed);
        if(engine->m_engineTaskQueue.isShuttingDown()) {
            return;
        }

        engine->handlePipelineWakeSignals(engine->m_pipeline.drainPendingSignals());
    });

    if(!enqueued) {
        m_pipelineWakeTaskQueued.store(false, std::memory_order_relaxed);
    }
}

void AudioEngine::setAnalysisDataSubscriptions(const Engine::AnalysisDataTypes subscriptions)
{
    if(!m_analysisBus) {
        return;
    }

    m_analysisBus->setSubscriptions(subscriptions);
}

uint64_t AudioEngine::beginTransportTransition()
{
    return m_transitions.beginTransportTransition(nextTransitionId());
}

void AudioEngine::clearTransportTransition()
{
    m_transitions.clearTransportTransition();
}

uint64_t AudioEngine::nextTransitionId()
{
    uint64_t id = m_nextTransitionId++;

    if(id == 0) {
        id = m_nextTransitionId++;
    }

    if(m_nextTransitionId == 0) {
        m_nextTransitionId = 1;
    }

    return id;
}

void AudioEngine::refreshVbrUpdateTimer()
{
    const bool shouldRun = m_vbrUpdateIntervalMs > 0 && playbackState() == Engine::PlaybackState::Playing
                        && m_decoder.isValid() && m_decoder.activeStream();

    if(!shouldRun) {
        if(m_vbrUpdateTimerId != 0) {
            killTimer(m_vbrUpdateTimerId);
            m_vbrUpdateTimerId = 0;
        }
        return;
    }

    if(m_vbrUpdateTimerId != 0) {
        killTimer(m_vbrUpdateTimerId);
    }

    m_vbrUpdateTimerId = startTimer(m_vbrUpdateIntervalMs, Qt::PreciseTimer);
}

void AudioEngine::setupSettings()
{
    const auto updateFadeDurations = [this]() {
        m_fadingValues = m_settings->value<Settings::Core::Internal::FadingValues>().value<Engine::FadingValues>();

        if(m_pipeline.isRunning()) {
            m_pipeline.setFaderCurve(m_fadingValues.pause.curve);
        }

        m_crossfadingValues
            = m_settings->value<Settings::Core::Internal::CrossfadingValues>().value<Engine::CrossfadingValues>();
    };

    const auto updateDecodeWatermarks = [this]() {
        const auto [safeLowRatio, safeHighRatio]
            = sanitiseWatermarkRatios(m_decodeLowWatermarkRatio, m_decodeHighWatermarkRatio);
        const auto [lowWatermarkMs, highWatermarkMs]
            = decodeWatermarksForBufferMs(m_playbackBufferLengthMs, safeLowRatio, safeHighRatio);

        const int clampedBufferMs = std::max(200, m_playbackBufferLengthMs);
        const int requestedLowMs  = std::clamp(
            static_cast<int>(std::lround(static_cast<double>(clampedBufferMs) * safeLowRatio)), 1, clampedBufferMs);
        const int requestedHighMs
            = std::clamp(static_cast<int>(std::lround(static_cast<double>(clampedBufferMs) * safeHighRatio)),
                         requestedLowMs, clampedBufferMs);

        if(lowWatermarkMs != requestedLowMs || highWatermarkMs != requestedHighMs) {
            qCWarning(ENGINE) << "Decode watermarks safety-clamped:" << "requestedLow=" << requestedLowMs
                              << "ms requestedHigh=" << requestedHighMs << "ms appliedLow=" << lowWatermarkMs
                              << "ms appliedHigh=" << highWatermarkMs << "ms bufferMs=" << clampedBufferMs;
        }
        m_decoder.setBufferWatermarksMs(lowWatermarkMs, highWatermarkMs);
    };

    updateFadeDurations();
    updateDecodeWatermarks();

    m_settings->subscribe<Settings::Core::Internal::FadingValues>(this, updateFadeDurations);
    m_settings->subscribe<Settings::Core::Internal::EngineFading>(this,
                                                                  [this](bool enabled) { m_fadingEnabled = enabled; });
    m_settings->subscribe<Settings::Core::Internal::EngineCrossfading>(
        this, [this](bool enabled) { m_crossfadeEnabled = enabled; });
    m_settings->subscribe<Settings::Core::GaplessPlayback>(this, [this](bool enabled) { m_gaplessEnabled = enabled; });
    m_settings->subscribe<Settings::Core::Internal::CrossfadeSwitchPolicy>(
        this, [this](int policy) { m_crossfadeSwitchPolicy = static_cast<Engine::CrossfadeSwitchPolicy>(policy); });
    m_settings->subscribe<Settings::Core::Internal::CrossfadingValues>(this, updateFadeDurations);
    m_settings->subscribe<Settings::Core::BufferLength>(this, [this, updateDecodeWatermarks](int bufferLengthMs) {
        const int clampedBufferLengthMs  = std::max(200, bufferLengthMs);
        const bool bufferLengthChanged   = m_playbackBufferLengthMs != clampedBufferLengthMs;
        const uint64_t currentPositionMs = position();

        m_playbackBufferLengthMs = clampedBufferLengthMs;
        updateDecodeWatermarks();

        if(bufferLengthChanged && m_currentTrack.isValid()) {
            reconfigureActiveStreamBuffering(currentPositionMs);
        }
    });
    m_settings->subscribe<Settings::Core::Internal::DecodeLowWatermarkRatio>(
        this, [this, updateDecodeWatermarks](double ratio) {
            m_decodeLowWatermarkRatio = ratio;
            updateDecodeWatermarks();
        });
    m_settings->subscribe<Settings::Core::Internal::DecodeHighWatermarkRatio>(
        this, [this, updateDecodeWatermarks](double ratio) {
            m_decodeHighWatermarkRatio = ratio;
            updateDecodeWatermarks();
        });
    m_settings->subscribe<Settings::Core::Internal::VBRUpdateInterval>(this, [this](int intervalMs) {
        m_vbrUpdateIntervalMs = std::max(0, intervalMs);
        m_lastVbrUpdateAt     = {};
        refreshVbrUpdateTimer();
        if(m_vbrUpdateIntervalMs <= 0) {
            publishBitrate(m_currentTrack.bitrate());
        }
    });

    m_replayGainSharedSettings           = ReplayGainProcessor::makeSharedSettings();
    const auto refreshReplayGainSettings = [this]() {
        ReplayGainProcessor::refreshSharedSettings(*m_settings, *m_replayGainSharedSettings);
    };

    const auto refreshDecoderPlaybackHints = [this](bool invalidatePreparedState) {
        const auto playMode = static_cast<Playlist::PlayModes>(m_settings->value<Settings::Core::PlayMode>());
        const auto hints    = decoderPlaybackHintsForPlayMode(playMode);

        if(hints == m_decoderPlaybackHints) {
            return;
        }

        m_decoderPlaybackHints = hints;
        m_decoder.setPlaybackHints(m_decoderPlaybackHints);

        if(invalidatePreparedState) {
            clearPreparedCrossfadeTransition();
            discardPreparedGaplessTransition(false);
            clearPreparedNextTrackAndCancelPendingJobs();
        }
    };

    refreshReplayGainSettings();
    refreshDecoderPlaybackHints(false);

    m_settings->subscribe<Settings::Core::PlayMode>(this, refreshReplayGainSettings);
    m_settings->subscribe<Settings::Core::PlayMode>(
        this, [refreshDecoderPlaybackHints]() { refreshDecoderPlaybackHints(true); });
    m_settings->subscribe<Settings::Core::RGMode>(this, refreshReplayGainSettings);
    m_settings->subscribe<Settings::Core::RGType>(this, refreshReplayGainSettings);
    m_settings->subscribe<Settings::Core::RGPreAmp>(this, refreshReplayGainSettings);
    m_settings->subscribe<Settings::Core::NonRGPreAmp>(this, refreshReplayGainSettings);

    m_pipeline.setOutputBitdepth(static_cast<SampleFormat>(m_settings->value<Settings::Core::OutputBitDepth>()));
    m_pipeline.setDither(m_settings->value<Settings::Core::OutputDither>());
    m_settings->subscribe<Settings::Core::OutputBitDepth>(this, [this](int bitdepth) {
        const AudioFormat inputFormat   = m_format;
        const AudioFormat currentOutput = m_pipeline.outputFormat();
        AudioFormat desiredOutput       = m_pipeline.setOutputBitdepth(static_cast<SampleFormat>(bitdepth));

        if(!desiredOutput.isValid() && inputFormat.isValid()) {
            desiredOutput = inputFormat;
        }

        const bool outputFormatChanged
            = inputFormat.isValid() && m_pipeline.hasOutput() && outputReinitRequired(currentOutput, desiredOutput);

        if(outputFormatChanged) {
            reinitOutputForCurrentFormat();
        }
    });
    m_settings->subscribe<Settings::Core::OutputDither>(this, [this](bool dither) { m_pipeline.setDither(dither); });
}

void AudioEngine::updatePlaybackState(Engine::PlaybackState state)
{
    const auto prevState = m_playbackState.exchange(state, std::memory_order_relaxed);
    if(prevState != state) {
        switch(state) {
            case Engine::PlaybackState::Playing:
                m_audioClock.setPlaying(m_audioClock.position());
                if(!isCrossfading(m_phase) && !isFadingTransport(m_phase) && m_phase != Playback::Phase::Seeking) {
                    setPhase(Playback::Phase::Playing, PhaseChangeReason::PlaybackStatePlaying);
                }
                break;
            case Engine::PlaybackState::Paused:
                m_audioClock.setPaused();
                setPhase(Playback::Phase::Paused, PhaseChangeReason::PlaybackStatePaused);
                break;
            case Engine::PlaybackState::Stopped:
                setPhase(Playback::Phase::Stopped, PhaseChangeReason::PlaybackStateStopped);
                m_audioClock.setStopped();
                break;
            case Engine::PlaybackState::Error:
                setPhase(Playback::Phase::Error, PhaseChangeReason::PlaybackStateError);
                m_audioClock.setStopped();
                break;
        }
        emit stateChanged(state);
    }

    refreshVbrUpdateTimer();
}

void AudioEngine::updateTrackStatus(Engine::TrackStatus status, bool flushDspOnEnd)
{
    const auto prevStatus = m_trackStatus.exchange(status, std::memory_order_relaxed);
    if(prevStatus != status) {
        emit trackStatusContextChanged(status, m_currentTrack, m_trackGeneration);
    }

    if(status == Engine::TrackStatus::Loading) {
        setPhase(Playback::Phase::Loading, PhaseChangeReason::TrackStatusLoading);
    }

    if(status == Engine::TrackStatus::End && flushDspOnEnd) {
        m_pipeline.flushDsp(DspNode::FlushMode::EndOfTrack);
    }
}

void AudioEngine::setPhase(const Playback::Phase phase, const PhaseChangeReason reason)
{
    if(m_phase == phase) {
        return;
    }

    const QString previousPhaseName = Utils::Enum::toString(m_phase);
    const QString nextPhaseName     = Utils::Enum::toString(phase);
    const QString reasonName        = Utils::Enum::toString(reason);

    qCDebug(ENGINE) << "Playback phase changed:"
                    << (previousPhaseName.isEmpty() ? QStringLiteral("Unknown") : previousPhaseName) << "->"
                    << (nextPhaseName.isEmpty() ? QStringLiteral("Unknown") : nextPhaseName)
                    << "reason=" << (reasonName.isEmpty() ? QStringLiteral("Unknown") : reasonName);
    m_phase = phase;
}

bool AudioEngine::hasPlaybackState(Engine::PlaybackState state) const
{
    return m_playbackState.load(std::memory_order_relaxed) == state;
}

bool AudioEngine::signalTrackEndOnce(bool flushDspOnEnd)
{
    if(!shouldEmitTrackEndOnce(m_lastEndedTrack, m_currentTrack)) {
        return false;
    }

    qCDebug(ENGINE) << "Emitting track-end status:" << "trackId=" << m_currentTrack.id()
                    << "generation=" << m_trackGeneration << "flushDspOnEnd=" << flushDspOnEnd;
    updateTrackStatus(Engine::TrackStatus::End, flushDspOnEnd);
    return true;
}

void AudioEngine::clearTrackEndLatch()
{
    m_lastEndedTrack = {};
}

AudioEngine::TrackEndingResult AudioEngine::checkTrackEnding(const AudioStreamPtr& stream, const uint64_t relativePosMs,
                                                             const uint64_t audiblePosMs)
{
    if(!stream) {
        return {};
    }

    const uint64_t bufferedMs        = stream->bufferedDurationMs();
    const uint64_t playbackDelayMs   = m_pipeline.playbackDelayMs();
    const uint64_t transitionDelayMs = m_pipeline.transitionPlaybackDelayMs();
    const double delayToTrackScale   = std::clamp(m_pipeline.playbackDelayToTrackScale(), 0.05, 8.0);

    uint64_t timelineDelayMs{playbackDelayMs};
    if(playbackDelayMs > 0) {
        const auto scaledDelay = std::llround(static_cast<long double>(playbackDelayMs) * delayToTrackScale);
        timelineDelayMs        = static_cast<uint64_t>(
            std::clamp<long double>(scaledDelay, 0.0L, static_cast<long double>(std::numeric_limits<uint64_t>::max())));
    }

    uint64_t drainDelayMs{transitionDelayMs};
    if(transitionDelayMs > 0) {
        const auto scaledDelay = std::llround(static_cast<long double>(transitionDelayMs) * delayToTrackScale);
        drainDelayMs           = static_cast<uint64_t>(
            std::clamp<long double>(scaledDelay, 0.0L, static_cast<long double>(std::numeric_limits<uint64_t>::max())));
    }

    uint64_t remainingOutputMs{bufferedMs};
    if(remainingOutputMs > std::numeric_limits<uint64_t>::max() - drainDelayMs) {
        remainingOutputMs = std::numeric_limits<uint64_t>::max();
    }
    else {
        remainingOutputMs += drainDelayMs;
    }

    PlaybackTransitionCoordinator::TrackEndingInput input;
    const auto configuredMode               = configuredTrackEndAutoTransitionMode();
    const bool crossfadeUsesAudibleBoundary = configuredMode == AutoTransitionMode::Crossfade
                                           && m_crossfadeSwitchPolicy != Engine::CrossfadeSwitchPolicy::Boundary;

    input.positionMs                     = crossfadeUsesAudibleBoundary ? audiblePosMs : relativePosMs;
    input.durationMs                     = m_currentTrack.duration();
    input.durationBoundaryEnabled        = m_currentTrack.hasCue();
    input.predictiveTimelineHintsEnabled = shouldEnableTimelineTransitionHints(m_currentTrack, m_decoderPlaybackHints);
    input.timelineDelayMs                = crossfadeUsesAudibleBoundary ? 0 : timelineDelayMs;
    input.remainingOutputMs              = remainingOutputMs;
    input.endOfInput                     = stream->endOfInput();
    input.bufferEmpty                    = stream->bufferEmpty();
    input.autoCrossfadeEnabled           = configuredMode == AutoTransitionMode::Crossfade;
    input.gaplessEnabled                 = configuredMode == AutoTransitionMode::Gapless;
    input.autoFadeOutMs          = input.autoCrossfadeEnabled ? m_crossfadingValues.autoChange.effectiveOutMs() : 0;
    input.autoFadeInMs           = input.autoCrossfadeEnabled ? m_crossfadingValues.autoChange.effectiveInMs() : 0;
    input.boundaryFadeEnabled    = configuredMode == AutoTransitionMode::BoundaryFade;
    input.boundaryFadeOutMs      = input.boundaryFadeEnabled ? m_fadingValues.boundary.effectiveOutMs() : 0;
    input.gaplessPrepareWindowMs = GaplessPrepareLeadMs;

    auto result = m_transitions.evaluateTrackEnding(input);
    return result;
}

uint64_t AudioEngine::preferredPreparedPrefillMs() const
{
    const uint64_t scaledDelayMs = scaledTransitionDelayMs();

    uint64_t overlapLeadMs{0};
    if(configuredTrackEndAutoTransitionMode() == AutoTransitionMode::Crossfade) {
        const uint64_t fadeOutMs = static_cast<uint64_t>(std::max(0, m_crossfadingValues.autoChange.effectiveOutMs()));
        const uint64_t fadeInMs  = static_cast<uint64_t>(std::max(0, m_crossfadingValues.autoChange.effectiveInMs()));
        overlapLeadMs            = std::min(fadeOutMs, fadeInMs);
    }

    const auto preferredPrefillMs = saturatingAdd(saturatingAdd(scaledDelayMs, overlapLeadMs), PrefillSafetyMarginMs);
    if(boundaryCrossfadeOverlapMs() == 0) {
        return preferredPrefillMs;
    }

    return std::max(preferredPrefillMs, transitionReserveMs());
}

Engine::PlaybackItem AudioEngine::currentPlaybackItem() const
{
    return {.track = m_currentTrack, .itemId = m_currentTrackItemId};
}

Engine::PlaybackItem AudioEngine::upcomingTrackCandidateItem() const
{
    return {.track = m_upcomingTrackCandidate, .itemId = m_upcomingTrackCandidateItemId};
}

Engine::PlaybackItem AudioEngine::preparedCrossfadeTargetItem() const
{
    return {.track = m_preparedCrossfadeTransition.targetTrack, .itemId = m_preparedCrossfadeTransition.targetItemId};
}

Engine::PlaybackItem AudioEngine::preparedGaplessTargetItem() const
{
    return {.track = m_preparedGaplessTransition.targetTrack, .itemId = m_preparedGaplessTransition.targetItemId};
}

std::optional<AudioEngine::AutoTransitionEligibility>
AudioEngine::evaluateAutoTransitionEligibility(const Track& track, bool isManualChange, bool requireTransitionReady,
                                               const char** rejectionReason) const
{
    const auto reject = [rejectionReason](const char* reason) -> std::optional<AutoTransitionEligibility> {
        if(rejectionReason) {
            *rejectionReason = reason;
        }
        return {};
    };

    if(rejectionReason) {
        *rejectionReason = "eligible";
    }

    if(!isManualChange && m_fadeController.hasPendingFade()) {
        return reject("pending-fade");
    }

    if(!track.isValid()) {
        return reject("invalid-track");
    }

    if(!isManualChange && isMultiTrackFileTransition(m_currentTrack, track)) {
        // Keep multi-track same-file transitions on the non-crossfade path
        return reject("same-file-segment");
    }

    const auto& transitionSpec = !isManualChange ? m_crossfadingValues.autoChange : m_crossfadingValues.manualChange;
    const int baseFadeOutMs    = transitionSpec.effectiveOutMs();
    const int baseFadeInMs     = transitionSpec.effectiveInMs();

    const auto configuredMode = !isManualChange ? configuredTrackEndAutoTransitionMode() : AutoTransitionMode::None;
    const bool crossfadeMix   = isManualChange ? (m_crossfadeEnabled && (baseFadeInMs > 0 || baseFadeOutMs > 0))
                                               : (configuredMode == AutoTransitionMode::Crossfade);
    const bool boundaryFadeEnabled = !isManualChange && configuredMode == AutoTransitionMode::BoundaryFade;
    const bool gaplessHandoff
        = !isManualChange && (configuredMode == AutoTransitionMode::Gapless || boundaryFadeEnabled);

    if(!crossfadeMix && !gaplessHandoff) {
        return reject("auto-transition-disabled");
    }

    if(!isManualChange && requireTransitionReady) {
        if(!m_transitions.isReadyForAutoTransition()) {
            return reject("transition-not-ready");
        }
        const auto readyMode = m_transitions.autoTransitionMode();
        if(crossfadeMix && readyMode != AutoTransitionMode::Crossfade) {
            return reject("transition-mode-not-crossfade");
        }
        if(!crossfadeMix && gaplessHandoff && boundaryFadeEnabled && readyMode != AutoTransitionMode::BoundaryFade) {
            return reject("transition-mode-not-boundary-fade");
        }
        if(!crossfadeMix && gaplessHandoff && !boundaryFadeEnabled && readyMode != AutoTransitionMode::Gapless) {
            return reject("transition-mode-not-gapless");
        }
    }

    if(!hasPlaybackState(Engine::PlaybackState::Playing)) {
        return reject("playback-not-playing");
    }

    if(!m_decoder.isValid()) {
        return reject("decoder-invalid");
    }

    if(m_transitions.isSeekInProgress()) {
        return reject("seek-in-progress");
    }

    if(!m_preparedNext || !samePlaybackItem(Engine::PlaybackItem{.track = track}, m_preparedNext->item)
       || !m_preparedNext->format.isValid()) {
        return reject("prepared-track-missing");
    }

    const auto inputFormat = m_pipeline.inputFormat();
    if(!inputFormat.isValid()) {
        return reject("pipeline-input-format-invalid");
    }

    const AudioFormat currentMixFormat = m_pipeline.predictPerTrackFormat(inputFormat);
    const AudioFormat nextMixFormat    = m_pipeline.predictPerTrackFormat(m_preparedNext->format);
    if(!gaplessFormatsMatch(currentMixFormat, nextMixFormat)) {
        return reject("format-mismatch");
    }

    auto currentStream = m_decoder.activeStream();
    if(!currentStream) {
        return reject("current-stream-missing");
    }

    int fadeOutDurationMs      = crossfadeMix ? std::max(0, baseFadeOutMs) : 0;
    const int fadeInDurationMs = crossfadeMix ? std::max(0, baseFadeInMs) : 0;

    if(crossfadeMix && !isManualChange && m_currentTrack.duration() > 0) {
        const uint64_t relativePosMs
            = relativeTrackPositionMs(currentStream->positionMs(), m_streamToTrackOriginMs, m_currentTrack.offset());
        const uint64_t remainingTrackMs
            = relativePosMs < m_currentTrack.duration() ? (m_currentTrack.duration() - relativePosMs) : 0;
        const int remainingWindowMs
            = static_cast<int>(std::min<uint64_t>(remainingTrackMs, std::numeric_limits<int>::max()));
        fadeOutDurationMs = std::min(fadeOutDurationMs, remainingWindowMs);
    }

    if(crossfadeMix && fadeOutDurationMs <= 0 && fadeInDurationMs <= 0) {
        return reject("zero-crossfade-window");
    }

    return AutoTransitionEligibility{
        .crossfadeMix      = crossfadeMix,
        .gaplessHandoff    = gaplessHandoff,
        .fadeOutDurationMs = fadeOutDurationMs,
        .fadeInDurationMs  = fadeInDurationMs,
    };
}

bool AudioEngine::isAutoTransitionEligible(const Track& track) const
{
    return evaluateAutoTransitionEligibility(track, false, false).has_value();
}

bool AudioEngine::shouldEnableTimelineTransitionHints(const Track& track,
                                                      const AudioDecoder::PlaybackHints playbackHints)
{
    return track.duration() > 0 && !playbackHints.testFlag(AudioDecoder::PlaybackHint::RepeatTrackEnabled);
}

void AudioEngine::setCurrentTrackContext(const Engine::PlaybackItem& item)
{
    m_currentTrack       = item.track;
    m_currentTrackItemId = item.itemId;
    ++m_trackGeneration;
    m_lastAppliedSeekRequestId = 0;
    m_lastVbrUpdateAt          = {};
    m_positionCoordinator.resetContinuity();
    clearAutoAdvanceState();
    clearPendingAudibleTrackCommit();

    updatePositionContext(m_pipeline.currentStatus().timelineEpoch);
    publishBitrate(item.track.bitrate());
    refreshVbrUpdateTimer();
}

void AudioEngine::setStreamToTrackOriginForTrack(const Track& track)
{
    m_streamToTrackOriginMs = track.offset();
}

bool AudioEngine::setStreamToTrackOriginForSegmentSwitch(const Track& track, uint64_t streamPosMs)
{
    if(track.offset() >= streamPosMs) {
        m_streamToTrackOriginMs = track.offset() - streamPosMs;
        return true;
    }

    qCWarning(ENGINE) << "Segment switch stream position exceeded target offset; aborting segment switch:"
                      << "trackOffsetMs=" << track.offset() << "streamPosMs=" << streamPosMs;
    return false;
}

bool AudioEngine::initDecoder(const Engine::PlaybackItem& item, bool allowPreparedStream)
{
    const Track& track = item.track;
    m_decoder.reset();

    if(!track.isValid()) {
        return false;
    }

    if(m_preparedNext && samePlaybackItem(item, m_preparedNext->item)) {
        auto& prepared               = *m_preparedNext;
        const bool hasPreparedStream = prepared.preparedStream != nullptr;
        const bool decoderAdvanced   = prepared.preparedDecodePositionMs > track.offset();

        if(!m_decoder.adoptPreparedDecoder(std::move(prepared.loadedDecoder), track)) {
            qCWarning(ENGINE) << "Failed to adopt prepared decoder";
            clearPreparedNextTrack();
            return false;
        }

        if(hasPreparedStream) {
            if(allowPreparedStream) {
                m_decoder.setActiveStream(prepared.preparedStream);
                m_decoder.setPreparedDecodePosition(prepared.preparedDecodePositionMs);
            }
            else {
                m_decoder.seek(m_decoder.startPosition());
            }
        }
        else if(decoderAdvanced) {
            m_decoder.seek(m_decoder.startPosition());
        }

        clearPreparedNextTrack();
        return true;
    }

    auto decoder = m_audioLoader->loadDecoderForTrack(track, AudioDecoder::UpdateTracks, m_decoderPlaybackHints);
    if(!decoder.decoder) {
        qCWarning(ENGINE) << "No decoder available for track:" << track.filepath();
        return false;
    }

    if(!m_decoder.init(std::move(decoder), track)) {
        qCWarning(ENGINE) << "Failed to initialse decoder context";
        return false;
    }

    return true;
}

bool AudioEngine::setupNewTrackStream(const Track& track, bool applyPendingSeek)
{
    const auto startupPrefillMs = [this]() {
        const int baseMs          = m_playbackBufferLengthMs / 4;
        const int decodeHighMs    = std::max(1, m_decoder.highWatermarkMs());
        const auto outputSnapshot = m_pipeline.outputQueueSnapshot();
        const int outputDemandMs  = startupPrefillFromOutputQueueMs(outputSnapshot, m_pipeline.outputFormat());
        const int targetMs        = std::max({baseMs, decodeHighMs, outputDemandMs});
        return std::clamp(targetMs, 1, std::max(1, m_playbackBufferLengthMs));
    }();

    const size_t bufferSamples
        = Audio::bufferSamplesFromMs(m_playbackBufferLengthMs, m_format.sampleRate(), m_format.channelCount());

    auto stream = m_decoder.createStream(bufferSamples, Engine::FadeCurve::Linear);
    if(!stream) {
        qCWarning(ENGINE) << "Failed to create track stream";
        return false;
    }

    stream->setTrack(track);
    m_decoder.setActiveStream(stream);

    if(m_decoder.startPosition() > 0) {
        m_decoder.seek(m_decoder.startPosition());
        m_decoder.syncStreamPosition();
    }

    m_decoder.start();

    if(applyPendingSeek) {
        if(!registerAndSwitchStream(stream, "Failed to register stream for pending seek")) {
            return false;
        }

        const auto pendingSeek = m_transitions.takePendingInitialSeekForTrack(track.id());
        if(!pendingSeek.has_value()) {
            prefillActiveStream(startupPrefillMs);
            return true;
        }

        performSimpleSeek(pendingSeek->positionMs, pendingSeek->requestId, startupPrefillMs);

        if(!m_decoder.isValid()) {
            qCWarning(ENGINE) << "Pending-seek stream setup left decoder invalid";
            return false;
        }
    }
    else {
        prefillActiveStream(startupPrefillMs);

        if(!registerAndSwitchStream(stream, "Failed to register stream")) {
            return false;
        }
    }

    return true;
}

bool AudioEngine::registerAndSwitchStream(const AudioStreamPtr& stream, const char* failureMessage)
{
    const StreamId streamId = m_pipeline.registerStream(stream);
    if(streamId == InvalidStreamId) {
        qCWarning(ENGINE) << failureMessage;
        return false;
    }

    m_pipeline.switchToStream(streamId);
    return true;
}

void AudioEngine::cleanupDecoderActiveStreamFromPipeline(bool removeFromMixer)
{
    if(auto stream = m_decoder.activeStream()) {
        const StreamId streamId = stream->id();

        if(removeFromMixer) {
            m_pipeline.removeStream(streamId);
        }

        m_pipeline.unregisterStream(streamId);
    }
}

void AudioEngine::clearPreparedNextTrack()
{
    if(m_preparedNext.has_value() && m_preparedNext->isValid() && m_preparedNext->loadedDecoder.decoder) {
        m_preparedNext->loadedDecoder.decoder->stop();
    }
    m_preparedNext.reset();

    clearPreparedCrossfadeTransition();
    clearPreparedGaplessTransition();
}

void AudioEngine::clearPreparedCrossfadeTransition()
{
    m_preparedCrossfadeTransition.active                = false;
    m_preparedCrossfadeTransition.targetTrack           = {};
    m_preparedCrossfadeTransition.targetItemId          = 0;
    m_preparedCrossfadeTransition.sourceGeneration      = 0;
    m_preparedCrossfadeTransition.streamId              = InvalidStreamId;
    m_preparedCrossfadeTransition.boundaryLeadMs        = 0;
    m_preparedCrossfadeTransition.overlapMidpointLeadMs = 0;
    m_preparedCrossfadeTransition.bufferedAtArmMs       = 0;
    m_preparedCrossfadeTransition.boundarySignalled     = false;
}

void AudioEngine::clearPreparedGaplessTransition()
{
    m_preparedGaplessTransition.active           = false;
    m_preparedGaplessTransition.targetTrack      = {};
    m_preparedGaplessTransition.targetItemId     = 0;
    m_preparedGaplessTransition.sourceGeneration = 0;
    m_preparedGaplessTransition.streamId         = InvalidStreamId;
    m_preparedGaplessTransition.boundaryFadeMode = false;
    m_preparedGaplessTransition.decoderAdopted   = false;
    m_preparedGaplessTransition.preCommitTimingStream.reset();
}

void AudioEngine::discardPreparedGaplessTransition(bool preserveDecoderOwnedStream)
{
    if(!m_preparedGaplessTransition.active) {
        return;
    }

    const StreamId preparedStreamId{m_preparedGaplessTransition.streamId};
    const auto activeStream = m_decoder.activeStream();
    const bool decoderOwnsPreparedStream
        = activeStream && preparedStreamId != InvalidStreamId && activeStream->id() == preparedStreamId;

    if(preparedStreamId != InvalidStreamId && (!preserveDecoderOwnedStream || !decoderOwnsPreparedStream)) {
        qCDebug(ENGINE) << "Discarding prepared gapless transition stream:"
                        << "trackId=" << m_preparedGaplessTransition.targetTrack.id()
                        << "itemId=" << m_preparedGaplessTransition.targetItemId
                        << "generation=" << m_preparedGaplessTransition.sourceGeneration
                        << "preparedStreamId=" << preparedStreamId << "decoderOwned=" << decoderOwnsPreparedStream;
        m_pipeline.removeStream(preparedStreamId);
        m_pipeline.unregisterStream(preparedStreamId);

        if(m_preparedNext && m_preparedNext->preparedStream
           && m_preparedNext->preparedStream->id() == preparedStreamId) {
            m_preparedNext.reset();
        }
    }

    clearPreparedGaplessTransition();
}

void AudioEngine::disarmStalePreparedTransitions(const Track& contextTrack, uint64_t contextGeneration)
{
    const auto clearStaleCrossfade = [&]() {
        if(!m_preparedCrossfadeTransition.active) {
            return;
        }

        const bool generationMismatch = m_preparedCrossfadeTransition.sourceGeneration != contextGeneration;
        const bool trackMismatch      = !sameTrackIdentity(m_preparedCrossfadeTransition.targetTrack, contextTrack);
        if(!generationMismatch && !trackMismatch) {
            return;
        }

        qCDebug(ENGINE) << "Disarming stale prepared crossfade transition:"
                        << "contextTrackId=" << contextTrack.id() << "contextGeneration=" << contextGeneration
                        << "preparedTrackId=" << m_preparedCrossfadeTransition.targetTrack.id()
                        << "preparedGeneration=" << m_preparedCrossfadeTransition.sourceGeneration
                        << "trackMismatch=" << trackMismatch << "generationMismatch=" << generationMismatch;
        clearPreparedCrossfadeTransition();
    };

    const auto clearStaleGapless = [&]() {
        if(!m_preparedGaplessTransition.active) {
            return;
        }

        const bool generationMismatch = m_preparedGaplessTransition.sourceGeneration != contextGeneration;
        const bool trackMismatch      = !sameTrackIdentity(m_preparedGaplessTransition.targetTrack, contextTrack);
        if(!generationMismatch && !trackMismatch) {
            return;
        }

        qCDebug(ENGINE) << "Disarming stale prepared gapless transition:"
                        << "contextTrackId=" << contextTrack.id() << "contextGeneration=" << contextGeneration
                        << "preparedTrackId=" << m_preparedGaplessTransition.targetTrack.id()
                        << "preparedGeneration=" << m_preparedGaplessTransition.sourceGeneration
                        << "trackMismatch=" << trackMismatch << "generationMismatch=" << generationMismatch;
        discardPreparedGaplessTransition(false);
    };

    clearStaleCrossfade();
    clearStaleGapless();
}

void AudioEngine::clearPreparedNextTrackAndCancelPendingJobs()
{
    cancelPendingPrepareJobs();
    clearPreparedNextTrack();
}

bool AudioEngine::prepareNextTrackImmediate(const Engine::PlaybackItem& item, const uint64_t prefillTargetMs)
{
    const Track& track = item.track;
    if(!track.isValid()) {
        return false;
    }

    const uint64_t preparedBufferMs = transitionReserveMs();
    NextTrackPreparer::Context context;
    context.audioLoader        = m_audioLoader;
    context.currentTrack       = m_currentTrack;
    context.playbackState      = m_playbackState.load(std::memory_order_relaxed);
    context.playbackHints      = m_decoderPlaybackHints;
    context.bufferLengthMs     = preparedBufferMs;
    context.preferredPrefillMs = (prefillTargetMs > 0) ? prefillTargetMs : preferredPreparedPrefillMs();

    auto prepared = NextTrackPreparer::prepare(track, context);
    if(!prepared.isValid()) {
        clearPreparedNextTrack();
        return false;
    }

    prepared.item  = item;
    m_preparedNext = std::move(prepared);

    const char* readinessReason = "eligible";
    (void)evaluateAutoTransitionEligibility(track, false, false, &readinessReason);
    if(isFormatMismatchReadinessReason(readinessReason) && m_preparedNext->preparedStream) {
        qCDebug(ENGINE) << "Discarding prepared next-track stream after format mismatch:"
                        << "trackId=" << track.id() << "itemId=" << item.itemId
                        << "preparedBufferedMs=" << m_preparedNext->preparedStream->bufferedDurationMs()
                        << "preparedDecodePosMs=" << m_preparedNext->preparedDecodePositionMs;
        m_preparedNext->preparedStream.reset();
    }

    return true;
}

void AudioEngine::cancelPendingPrepareJobs()
{
    m_nextTrackPrepareWorker.cancelPendingJobs();
}

void AudioEngine::enqueuePrepareNextTrack(const Engine::PlaybackItem& item, uint64_t requestId,
                                          uint64_t prefillTargetMs)
{
    const Track& track              = item.track;
    const uint64_t itemId           = item.itemId;
    const uint64_t preparedBufferMs = transitionReserveMs();
    NextTrackPrepareWorker::Request request;
    request.requestId                  = requestId;
    request.item                       = item;
    request.context.audioLoader        = m_audioLoader;
    request.context.currentTrack       = m_currentTrack;
    request.context.playbackState      = m_playbackState.load(std::memory_order_relaxed);
    request.context.playbackHints      = m_decoderPlaybackHints;
    request.context.bufferLengthMs     = preparedBufferMs;
    request.context.preferredPrefillMs = (prefillTargetMs > 0) ? prefillTargetMs : preferredPreparedPrefillMs();

    qCDebug(ENGINE) << "Queued next-track preparation:" << "currentTrackId=" << m_currentTrack.id()
                    << "currentItemId=" << m_currentTrackItemId << "trackId=" << track.id() << "itemId=" << itemId
                    << "requestId=" << requestId << "preferredPrefillMs=" << request.context.preferredPrefillMs
                    << "bufferLengthMs=" << request.context.bufferLengthMs
                    << "configuredMode=" << Utils::Enum::toString(configuredTrackEndAutoTransitionMode());

    m_nextTrackPrepareWorker.replacePending(std::move(request));
}

void AudioEngine::applyPreparedNextTrackResult(uint64_t jobToken, uint64_t requestId, const Engine::PlaybackItem& item,
                                               NextTrackPreparationState prepared)
{
    const Track& track    = item.track;
    const uint64_t itemId = item.itemId;
    if(jobToken != m_nextTrackPrepareWorker.activeJobToken()) {
        qCDebug(ENGINE) << "Dropping stale next-track preparation result:" << "trackId=" << track.id()
                        << "itemId=" << itemId << "requestId=" << requestId << "jobToken=" << jobToken
                        << "activeJobToken=" << m_nextTrackPrepareWorker.activeJobToken();
        return;
    }

    bool ready{false};
    const bool preparedValid           = prepared.isValid();
    const bool preparedStreamAvailable = preparedValid && prepared.preparedStream != nullptr;
    const uint64_t preparedBufferedMs  = preparedStreamAvailable ? prepared.preparedStream->bufferedDurationMs() : 0;
    const uint64_t preparedDecodePosMs = preparedValid ? prepared.preparedDecodePositionMs : 0;
    const char* readinessReason        = preparedValid ? "eligible" : "prepare-failed";

    if(preparedValid) {
        m_preparedNext = std::move(prepared);
        ready          = evaluateAutoTransitionEligibility(track, false, false, &readinessReason).has_value();

        if(isFormatMismatchReadinessReason(readinessReason) && m_preparedNext->preparedStream) {
            qCDebug(ENGINE) << "Discarding prepared next-track stream after format mismatch:"
                            << "trackId=" << track.id() << "itemId=" << itemId
                            << "preparedBufferedMs=" << preparedBufferedMs
                            << "preparedDecodePosMs=" << preparedDecodePosMs;
            m_preparedNext->preparedStream.reset();
        }
    }
    else {
        clearPreparedNextTrack();
    }

    if(samePlaybackItem(item, upcomingTrackCandidateItem())) {
        m_autoAdvanceState.generation            = m_trackGeneration;
        m_autoAdvanceState.drainPrepareRequested = false;
    }

    qCDebug(ENGINE) << "Next-track preparation completed:" << "trackId=" << track.id() << "requestId=" << requestId
                    << "itemId=" << itemId << "prepared=" << preparedValid
                    << "preparedStream=" << preparedStreamAvailable << "preparedBufferedMs=" << preparedBufferedMs
                    << "preparedDecodePosMs=" << preparedDecodePosMs << "ready=" << ready
                    << "readyReason=" << readinessReason
                    << "matchesUpcomingCandidate=" << samePlaybackItem(item, upcomingTrackCandidateItem());

    emit nextTrackReadiness(item, ready, requestId);

    if(samePlaybackItem(item, upcomingTrackCandidateItem())) {
        tryAutoAdvanceCommit();
    }
}

void AudioEngine::cleanupActiveStream()
{
    m_pipeline.cleanupOrphanImmediate();
    cleanupDecoderActiveStreamFromPipeline(true);
}

void AudioEngine::cleanupOrphanedStream()
{
    if(!m_pipeline.hasOrphanStream()) {
        const bool hadCrossfadePhase = isCrossfading(m_phase);

        if(hadCrossfadePhase && hasPlaybackState(Engine::PlaybackState::Playing)) {
            updateTrackStatus(Engine::TrackStatus::Buffered);
            setPhase(Playback::Phase::Playing, PhaseChangeReason::CrossfadeCleanupResumePlaying);
        }
        return;
    }

    if(!isCrossfading(m_phase)) {
        qCWarning(ENGINE) << "Orphaned stream exists while crossfade state is idle; cleaning up immediately";
        m_pipeline.cleanupOrphanImmediate();
    }
}

void AudioEngine::resetStreamToTrackOrigin()
{
    m_streamToTrackOriginMs = 0;
}

TrackLoadContext AudioEngine::buildLoadContext(const Track& track, bool manualChange) const
{
    return TrackLoadContext{
        .manualChange                = manualChange,
        .isPlaying                   = hasPlaybackState(Engine::PlaybackState::Playing),
        .decoderValid                = m_decoder.isValid(),
        .hasPendingTransportFade     = m_fadeController.hasPendingFade(),
        .isContiguousSameFileSegment = isContiguousSameFileSegment(m_currentTrack, track),
    };
}

bool AudioEngine::executeSegmentSwitchLoad(const Engine::PlaybackItem& item, bool preserveTransportFade)
{
    const Track& track = item.track;
    clearAutoCrossfadeTailFadeState();
    clearAutoBoundaryFadeState(true);

    uint64_t streamPosMs{0};
    if(const auto stream = m_decoder.activeStream()) {
        streamPosMs = stream->positionMs();
    }

    if(setStreamToTrackOriginForSegmentSwitch(track, streamPosMs)) {
        qCDebug(ENGINE) << "Switching contiguous segment on active stream:" << track.filenameExt();

        const auto previousTrackStatus = m_trackStatus.load(std::memory_order_relaxed);

        if(!m_decoder.switchContiguousTrack(track)) {
            qCWarning(ENGINE) << "Contiguous segment switch could not retarget decoder state; falling back:"
                              << track.filenameExt();
            return false;
        }

        clearPreparedNextTrack();
        setCurrentTrackContext(item);
        clearTrackEndLatch();
        m_transitions.clearTrackEnding();

        if(!preserveTransportFade) {
            m_fadeController.invalidateActiveFade();
        }

        if(previousTrackStatus == Engine::TrackStatus::End) {
            updateTrackStatus(Engine::TrackStatus::Buffered);
        }

        const auto stream           = m_decoder.activeStream();
        const bool streamWasStopped = stream && stream->state() == AudioStream::State::Stopped;

        if(stream && streamWasStopped) {
            m_pipeline.switchToStream(stream->id());
        }

        if(hasPlaybackState(Engine::PlaybackState::Playing)) {
            if(streamWasStopped) {
                const int restartPrefillMs = std::max(20, m_decoder.lowWatermarkMs());
                prefillActiveStream(restartPrefillMs);
            }

            if(const auto activeStream = m_decoder.activeStream()) {
                m_pipeline.sendStreamCommand(activeStream->id(), AudioStream::Command::Play);
            }
            m_pipeline.play();
            m_decoder.ensureDecodeTimerRunning();
        }

        updatePositionContext(m_pipeline.currentStatus().timelineEpoch);
        publishPosition(0, 0, 1.0, AudioClock::UpdateMode::Discontinuity, true);
        finaliseTrackCommit(Engine::TransitionMode::SegmentSwitch);
        return true;
    }

    qCWarning(ENGINE) << "Falling back to full track load for contiguous segment switch:" << track.filenameExt();
    return false;
}

bool AudioEngine::executeManualFadeDeferredLoad(const Engine::PlaybackItem& item)
{
    if(!handleManualChangeFade(item)) {
        return false;
    }

    qCDebug(ENGINE) << "Deferring manual track load until fade transition completes:" << item.track.filenameExt();
    return true;
}

bool AudioEngine::executeAutoTransitionLoad(const Engine::PlaybackItem& item)
{
    if(!startTrackCrossfade(item, false)) {
        return false;
    }

    qCDebug(ENGINE) << "Starting automatic transition to:" << item.track.filenameExt();
    return true;
}

void AudioEngine::executeFullReinitLoad(const Engine::PlaybackItem& item, bool manualChange, bool preserveTransportFade)
{
    const Track& track = item.track;
    qCDebug(ENGINE) << "Loading track:" << track.filenameExt();

    clearAutoCrossfadeTailFadeState();
    clearAutoBoundaryFadeState(true);
    clearPendingAnalysisData();
    m_decoder.stopDecoding();
    cleanupActiveStream();

    const auto prevState = m_playbackState.load(std::memory_order_relaxed);

    setCurrentTrackContext(item);
    updateTrackStatus(Engine::TrackStatus::Loading);
    setStreamToTrackOriginForTrack(track);
    clearTrackEndLatch();
    if(!preserveTransportFade) {
        m_fadeController.invalidateActiveFade();
    }

    const bool preparedNextMatchesItem
        = !manualChange && m_preparedNext && samePlaybackItem(item, m_preparedNext->item);
    const AudioFormat oldInputFormat = m_pipeline.inputFormat();
    const AudioFormat currentOutput  = m_pipeline.outputFormat();
    const bool hasOutput             = m_pipeline.hasOutput();

    const auto desiredOutputForFormat = [this](const AudioFormat& format) {
        AudioFormat desiredOutput = m_pipeline.predictOutputFormat(format);
        if(!desiredOutput.isValid()) {
            desiredOutput = format;
        }
        return desiredOutput;
    };

    if(preparedNextMatchesItem && hasOutput && m_preparedNext->format.isValid()) {
        const AudioFormat preparedDesiredOutput = desiredOutputForFormat(m_preparedNext->format);
        const bool outputFormatChanged          = outputReinitRequired(currentOutput, preparedDesiredOutput);

        if(outputFormatChanged) {
            qCDebug(ENGINE) << "Format change bypasses prepared decoder reuse; reopening decoder at track start";
            clearPreparedNextTrack();
        }
    }

    const auto initDecoderForLoad = [this, &item](bool allowPreparedStream) {
        if(!initDecoder(item, allowPreparedStream)) {
            return false;
        }

        syncDecoderTrackMetadata();
        m_format = m_decoder.format();
        return true;
    };

    if(!initDecoderForLoad(!manualChange)) {
        updateTrackStatus(Engine::TrackStatus::Invalid);
        return;
    }

    m_transitions.clearTrackEnding();
    const AudioFormat desiredOutput = desiredOutputForFormat(m_format);
    const bool inputFormatChanged   = oldInputFormat.isValid() && !Audio::inputFormatsMatch(oldInputFormat, m_format);
    const bool outputFormatChanged  = outputReinitRequired(currentOutput, desiredOutput);

    if(!hasOutput || outputFormatChanged) {
        if(outputFormatChanged && hasOutput) {
            qCDebug(ENGINE) << "Format changed, reinitializing output";
            m_outputController.uninitOutput();
        }
        if(!m_outputController.initOutput(m_format, m_volume)) {
            updateTrackStatus(Engine::TrackStatus::Invalid);
            return;
        }
    }
    else if(inputFormatChanged) {
        m_pipeline.applyInputFormat(m_format);
    }

    const Track& activeTrack    = m_currentTrack;
    const bool applyPendingSeek = m_transitions.hasPendingInitialSeekForTrack(activeTrack.id());

    if(!setupNewTrackStream(activeTrack, applyPendingSeek)) {
        updateTrackStatus(Engine::TrackStatus::Invalid);
        return;
    }

    updateTrackStatus(Engine::TrackStatus::Loaded);

    if(prevState == Engine::PlaybackState::Playing
       && (m_pipeline.faderIsFading() || m_fadeController.hasPendingFade())) {
        m_fadeController.setFadeOnNext(true);
    }
    else if(prevState != Engine::PlaybackState::Playing) {
        m_fadeController.setFadeOnNext(false);
    }

    if(prevState == Engine::PlaybackState::Playing) {
        play();
    }

    if(!applyPendingSeek) {
        updatePositionContext(m_pipeline.currentStatus().timelineEpoch);
        publishPosition(0, 0, 1.0, AudioClock::UpdateMode::Discontinuity, true);
    }

    finaliseTrackCommit(Engine::TransitionMode::Direct);
}

void AudioEngine::executeLoadPlan(const Engine::PlaybackItem& item, bool manualChange, const TrackLoadContext& context,
                                  const LoadPlan& plan)
{
    const bool preserveTransportFade = context.hasPendingTransportFade;

    switch(plan.firstStrategy) {
        case LoadStrategy::SegmentSwitch:
            if(executeSegmentSwitchLoad(item, preserveTransportFade)) {
                return;
            }
            if(plan.tryAutoTransition && executeAutoTransitionLoad(item)) {
                return;
            }
            executeFullReinitLoad(item, manualChange, preserveTransportFade);
            return;
        case LoadStrategy::ManualFadeDefer:
            if(executeManualFadeDeferredLoad(item)) {
                return;
            }
            executeFullReinitLoad(item, manualChange, preserveTransportFade);
            return;
        case LoadStrategy::AutoTransition:
            if(executeAutoTransitionLoad(item)) {
                return;
            }
            executeFullReinitLoad(item, manualChange, preserveTransportFade);
            return;
        case LoadStrategy::FullReinit:
            executeFullReinitLoad(item, manualChange, preserveTransportFade);
            return;
    }
}

void AudioEngine::loadTrack(const Engine::PlaybackItem& item, bool manualChange)
{
    clearPendingAudiblePause();

    const Track& track = item.track;
    if(manualChange) {
        m_pendingManualTrackItemId = 0;
    }

    if(manualChange) {
        clearPreparedCrossfadeTransition();
        discardPreparedGaplessTransition(false);
    }
    else {
        disarmStalePreparedTransitions(track, m_trackGeneration);
    }

    const TrackLoadContext loadContext = buildLoadContext(track, manualChange);
    const LoadPlan loadPlan            = planTrackLoad(loadContext);
    executeLoadPlan(item, manualChange, loadContext, loadPlan);
}

void AudioEngine::setUpcomingTrackCandidate(const Engine::PlaybackItem& item)
{
    const Track& track    = item.track;
    const uint64_t itemId = item.itemId;

    if(samePlaybackItem(item, upcomingTrackCandidateItem())) {
        return;
    }

    if(m_preparedGaplessTransition.active && !samePlaybackItem(item, preparedGaplessTargetItem())) {
        if(m_preparedGaplessTransition.decoderAdopted) {
            qCDebug(ENGINE) << "Ignoring upcoming track candidate change after prepared gapless decoder adoption:"
                            << "currentTrackId=" << m_currentTrack.id() << "currentItemId=" << m_currentTrackItemId
                            << "preparedTrackId=" << m_preparedGaplessTransition.targetTrack.id()
                            << "preparedItemId=" << m_preparedGaplessTransition.targetItemId
                            << "candidateTrackId=" << track.id() << "candidateItemId=" << itemId;
            return;
        }

        discardPreparedGaplessTransition(false);
    }

    cancelPendingPrepareJobs();

    m_upcomingTrackCandidate                 = track;
    m_upcomingTrackCandidateItemId           = itemId;
    m_autoAdvanceState.generation            = m_trackGeneration;
    m_autoAdvanceState.drainPrepareRequested = false;
    m_drainFillPrepareDiagnostic             = {};

    const auto configuredMode = configuredTrackEndAutoTransitionMode();

    qCDebug(ENGINE) << "Upcoming track candidate updated:" << "currentTrackId=" << m_currentTrack.id()
                    << "currentItemId=" << m_currentTrackItemId << "candidateTrackId=" << track.id()
                    << "candidateItemId=" << itemId << "sameAsCurrent=" << samePlaybackItem(item, currentPlaybackItem())
                    << "configuredMode=" << Utils::Enum::toString(configuredMode);

    if(!track.isValid() || samePlaybackItem(item, currentPlaybackItem())) {
        qCDebug(ENGINE) << "Upcoming track candidate will not be prepared immediately:"
                        << "candidateTrackId=" << track.id() << "candidateItemId=" << itemId
                        << "reason=" << (!track.isValid() ? "invalid-candidate" : "candidate-matches-current");
        clearPreparedNextTrack();
        return;
    }

    if(m_preparedNext && !samePlaybackItem(item, m_preparedNext->item)) {
        clearPreparedNextTrack();
    }

    if(configuredMode == AutoTransitionMode::Crossfade) {
        if(m_preparedNext && samePlaybackItem(item, m_preparedNext->item) && m_preparedNext->format.isValid()
           && m_preparedNext->preparedStream) {
            qCDebug(ENGINE) << "Upcoming track candidate reusing prepared crossfade state:"
                            << "candidateTrackId=" << track.id() << "candidateItemId=" << itemId
                            << "preparedBufferedMs=" << m_preparedNext->preparedStream->bufferedDurationMs();
            return;
        }

        enqueuePrepareNextTrack(item, 0, preferredPreparedPrefillMs());
        return;
    }

    qCDebug(ENGINE) << "Upcoming track candidate stored for deferred end-of-track preparation:"
                    << "candidateTrackId=" << track.id() << "candidateItemId=" << itemId
                    << "mode=" << Utils::Enum::toString(configuredMode);
}

void AudioEngine::setTrackEndAutoTransitionEnabled(bool enabled)
{
    if(std::exchange(m_trackEndAutoTransitionEnabled, enabled) == enabled) {
        return;
    }

    if(!enabled) {
        clearTrackEndAutoTransitions();
    }
}

void AudioEngine::clearTrackEndAutoTransitions()
{
    m_upcomingTrackCandidate       = {};
    m_upcomingTrackCandidateItemId = 0;
    clearAutoAdvanceState();
    clearPreparedCrossfadeTransition();
    discardPreparedGaplessTransition(false);
    clearPreparedNextTrackAndCancelPendingJobs();
}

void AudioEngine::invalidatePreparedAutoTransitionsForSeek()
{
    const StreamId preparedCrossfadeStreamId{m_preparedCrossfadeTransition.streamId};

    if(m_preparedCrossfadeTransition.active) {
        qCDebug(ENGINE) << "Invalidating prepared crossfade transition due to seek:"
                        << "currentTrackId=" << m_currentTrack.id() << "currentItemId=" << m_currentTrackItemId
                        << "preparedTrackId=" << m_preparedCrossfadeTransition.targetTrack.id()
                        << "preparedItemId=" << m_preparedCrossfadeTransition.targetItemId
                        << "preparedStreamId=" << preparedCrossfadeStreamId
                        << "generation=" << m_preparedCrossfadeTransition.sourceGeneration;
        clearPreparedCrossfadeTransition();
    }

    if(preparedCrossfadeStreamId != InvalidStreamId && m_preparedNext && m_preparedNext->preparedStream
       && m_preparedNext->preparedStream->id() == preparedCrossfadeStreamId) {
        qCDebug(ENGINE) << "Discarding prepared next-track state after seek invalidated armed crossfade:"
                        << "trackId=" << m_preparedNext->item.track.id() << "itemId=" << m_preparedNext->item.itemId
                        << "preparedStreamId=" << preparedCrossfadeStreamId;
        m_preparedNext.reset();
    }

    if(m_preparedGaplessTransition.active) {
        qCDebug(ENGINE) << "Discarding prepared gapless transition due to seek:"
                        << "currentTrackId=" << m_currentTrack.id() << "currentItemId=" << m_currentTrackItemId
                        << "preparedTrackId=" << m_preparedGaplessTransition.targetTrack.id()
                        << "preparedItemId=" << m_preparedGaplessTransition.targetItemId
                        << "preparedStreamId=" << m_preparedGaplessTransition.streamId
                        << "generation=" << m_preparedGaplessTransition.sourceGeneration;
        discardPreparedGaplessTransition(false);
    }
}

void AudioEngine::prepareNextTrack(const Engine::PlaybackItem& item, uint64_t requestId)
{
    const Track& track    = item.track;
    const uint64_t itemId = item.itemId;
    if(!track.isValid()) {
        qCDebug(ENGINE) << "Next-track prepare request cleared:" << "requestId=" << requestId;
        clearPreparedNextTrackAndCancelPendingJobs();
        emit nextTrackReadiness(item, false, requestId);
        return;
    }

    if(m_preparedNext && samePlaybackItem(item, m_preparedNext->item) && m_preparedNext->format.isValid()) {
        const char* readinessReason = "eligible";
        const bool ready = evaluateAutoTransitionEligibility(track, false, false, &readinessReason).has_value();
        qCDebug(ENGINE) << "Next-track prepare request reused prepared state:" << "trackId=" << track.id()
                        << "itemId=" << itemId << "requestId=" << requestId
                        << "preparedStream=" << (m_preparedNext->preparedStream != nullptr) << "preparedBufferedMs="
                        << (m_preparedNext->preparedStream ? m_preparedNext->preparedStream->bufferedDurationMs() : 0)
                        << "ready=" << ready << "readyReason=" << readinessReason;
        emit nextTrackReadiness(item, ready, requestId);
        return;
    }

    qCDebug(ENGINE) << "Next-track prepare request queued:" << "trackId=" << track.id() << "itemId=" << itemId
                    << "requestId=" << requestId << "preferredPrefillMs=" << preferredPreparedPrefillMs();
    clearPreparedNextTrack();
    enqueuePrepareNextTrack(item, requestId, preferredPreparedPrefillMs());
}

bool AudioEngine::armPreparedCrossfadeTransition(const Engine::PlaybackItem& item, uint64_t generation)
{
    const Track& track = item.track;
    disarmStalePreparedTransitions(track, generation);

    if(!track.isValid() || generation != m_trackGeneration || !hasPlaybackState(Engine::PlaybackState::Playing)
       || m_transitions.isSeekInProgress()) {
        return false;
    }

    if(effectiveAutoTransitionMode() != AutoTransitionMode::Crossfade
       || isMultiTrackFileTransition(m_currentTrack, track)) {
        return false;
    }

    if(m_preparedCrossfadeTransition.active) {
        return m_preparedCrossfadeTransition.sourceGeneration == generation
            && samePlaybackItem(item, preparedCrossfadeTargetItem());
    }

    if(!m_preparedNext || !samePlaybackItem(item, m_preparedNext->item) || !m_preparedNext->format.isValid()
       || !m_preparedNext->preparedStream) {
        return false;
    }

    const auto eligibility = evaluateAutoTransitionEligibility(track, false, false);
    if(!eligibility.has_value() || !eligibility->crossfadeMix) {
        return false;
    }

    int fadeOutDurationMs = eligibility->fadeOutDurationMs;
    int fadeInDurationMs  = eligibility->fadeInDurationMs;

    auto currentStream = m_decoder.activeStream();
    if(!currentStream) {
        return false;
    }

    if(fadeOutDurationMs > 0) {
        prefillActiveStream(std::min(fadeOutDurationMs, m_playbackBufferLengthMs));
        currentStream = m_decoder.activeStream();
        if(!currentStream) {
            return false;
        }
        fadeOutDurationMs = std::min(fadeOutDurationMs, static_cast<int>(currentStream->bufferedDurationMs()));
    }

    const uint64_t transitionDelayMs  = m_pipeline.transitionPlaybackDelayMs();
    const uint64_t preparedBufferedMs = m_preparedNext->preparedStream->bufferedDurationMs();
    const uint64_t safetyWindowMs{100};

    int overlapDurationMs = std::min(std::max(0, fadeOutDurationMs), std::max(0, fadeInDurationMs));
    if(overlapDurationMs > 0) {
        const uint64_t availableOverlapMs = preparedBufferedMs > (transitionDelayMs + safetyWindowMs)
                                              ? (preparedBufferedMs - (transitionDelayMs + safetyWindowMs))
                                              : 0;
        const auto requestedOverlapMs     = static_cast<uint64_t>(overlapDurationMs);

        if(availableOverlapMs < requestedOverlapMs) {
            const int clampedOverlapMs
                = static_cast<int>(std::min<uint64_t>(availableOverlapMs, std::numeric_limits<int>::max()));
            if(clampedOverlapMs <= 0) {
                return false;
            }

            fadeOutDurationMs = std::min(fadeOutDurationMs, clampedOverlapMs);
            fadeInDurationMs  = std::min(fadeInDurationMs, clampedOverlapMs);
            overlapDurationMs = std::min(std::max(0, fadeOutDurationMs), std::max(0, fadeInDurationMs));
        }
    }

    const StreamId activeStreamId   = currentStream->id();
    const bool hasEarlyAutoTailFade = m_autoCrossfadeTailFadeActive && m_autoCrossfadeTailFadeGeneration == generation
                                   && m_autoCrossfadeTailFadeStreamId == activeStreamId
                                   && activeStreamId != InvalidStreamId;
    const bool skipFadeOutStart     = hasEarlyAutoTailFade && overlapDurationMs <= 0;

    const uint64_t overlapWindowMs = static_cast<uint64_t>(std::max(0, overlapDurationMs));
    const uint64_t requiredBufferedMs
        = saturatingAdd(saturatingAdd(overlapWindowMs, transitionDelayMs), safetyWindowMs);

    if(preparedBufferedMs < requiredBufferedMs) {
        return false;
    }

    const uint64_t currentRelativePosMs
        = relativeTrackPositionMs(currentStream->positionMs(), m_streamToTrackOriginMs, m_currentTrack.offset());
    const uint64_t remainingToBoundaryMs
        = (m_currentTrack.duration() > currentRelativePosMs) ? (m_currentTrack.duration() - currentRelativePosMs) : 0;
    const uint64_t boundarySwitchWindowMs = saturatingAdd(overlapWindowMs, transitionDelayMs);

    if(m_crossfadeSwitchPolicy == Engine::CrossfadeSwitchPolicy::Boundary
       && remainingToBoundaryMs > boundarySwitchWindowMs) {
        return false;
    }

    clearAutoCrossfadeTailFadeState();
    clearAutoBoundaryFadeState(true);

    AudioPipeline::TransitionRequest request;
    request.type             = AudioPipeline::TransitionType::Crossfade;
    request.stream           = m_preparedNext->preparedStream;
    request.fadeOutMs        = fadeOutDurationMs;
    request.fadeInMs         = fadeInDurationMs;
    request.skipFadeOutStart = skipFadeOutStart;
    request.reanchorTimeline = false;

    const auto transitionResult = m_pipeline.executeTransition(request);
    if(!transitionResult.success || transitionResult.streamId == InvalidStreamId) {
        return false;
    }

    const bool armedWithOrphan = m_pipeline.hasOrphanStream();

    if(armedWithOrphan) {
        setPhase(Playback::Phase::TrackCrossfading, PhaseChangeReason::AutoTransitionCrossfadeActive);
    }
    else if(hasPlaybackState(Engine::PlaybackState::Playing)) {
        setPhase(Playback::Phase::Playing, PhaseChangeReason::AutoTransitionPlayingNoOrphan);
    }

    if(m_decoder.isDecodeTimerActive() || hasPlaybackState(Engine::PlaybackState::Playing)) {
        m_decoder.ensureDecodeTimerRunning();
    }

    m_preparedCrossfadeTransition.active                = true;
    m_preparedCrossfadeTransition.targetTrack           = track;
    m_preparedCrossfadeTransition.targetItemId          = item.itemId;
    m_preparedCrossfadeTransition.sourceGeneration      = generation;
    m_preparedCrossfadeTransition.streamId              = transitionResult.streamId;
    m_preparedCrossfadeTransition.boundaryLeadMs        = remainingToBoundaryMs;
    m_preparedCrossfadeTransition.overlapMidpointLeadMs = overlapWindowMs / 2;
    m_preparedCrossfadeTransition.bufferedAtArmMs       = preparedBufferedMs;
    m_preparedCrossfadeTransition.boundarySignalled     = false;
    discardPreparedGaplessTransition(false);

    // If the current stream has already ended by the time we arm the prepared
    // transition, there is no live source stream left to provide an overlap
    // anchor. Treat the arm as an immediate boundary handoff so the prepared
    // stream is committed before its prebuffer drains.
    if(!armedWithOrphan) {
        m_preparedCrossfadeTransition.boundarySignalled = true;
        emit trackBoundaryReached(m_currentTrack, m_trackGeneration, 0, true);
    }

    qCDebug(ENGINE) << "Prepared crossfade transition armed:" << "trackId=" << track.id() << "generation=" << generation
                    << "streamId=" << transitionResult.streamId << "preparedBufferedMs=" << preparedBufferedMs
                    << "preparedPosMs=" << m_preparedNext->preparedDecodePositionMs
                    << "remainingToBoundaryMs=" << remainingToBoundaryMs << "transitionDelayMs=" << transitionDelayMs;
    return true;
}

bool AudioEngine::commitPreparedCrossfadeTransition(const Engine::PlaybackItem& item)
{
    const Track& track = item.track;
    disarmStalePreparedTransitions(track, m_trackGeneration);

    if(!track.isValid() || !m_preparedCrossfadeTransition.active
       || !samePlaybackItem(item, preparedCrossfadeTargetItem())) {
        return false;
    }

    const StreamId preparedStreamId = m_preparedCrossfadeTransition.streamId;

    if(!initDecoder(item, true)) {
        clearPreparedCrossfadeTransition();
        return false;
    }

    const auto preparedStream = m_decoder.activeStream();
    if(!preparedStream || preparedStream->id() != preparedStreamId) {
        qCWarning(ENGINE) << "Prepared crossfade commit could not adopt armed stream:"
                          << "expectedStreamId=" << preparedStreamId
                          << "actualStreamId=" << (preparedStream ? preparedStream->id() : InvalidStreamId);
        clearPreparedCrossfadeTransition();
        return false;
    }

    m_format = m_decoder.format();
    m_decoder.startDecoding();
    clearAutoCrossfadeTailFadeState();
    clearAutoBoundaryFadeState(true);

    setCurrentTrackContext(item);
    setStreamToTrackOriginForTrack(track);
    clearTrackEndLatch();
    m_transitions.clearTrackEnding();

    m_positionCoordinator.clearGaplessHold();

    const uint64_t committedSourcePosMs = (preparedStream->positionMs() > preparedStream->bufferedDurationMs())
                                            ? (preparedStream->positionMs() - preparedStream->bufferedDurationMs())
                                            : 0;
    const uint64_t committedRelativePosMs
        = relativeTrackPositionMs(committedSourcePosMs, m_streamToTrackOriginMs, track.offset());
    const uint64_t pipelineDelayMs  = m_pipeline.playbackDelayMs();
    const double delayToSourceScale = std::clamp(m_pipeline.playbackDelayToTrackScale(), 0.05, 8.0);
    updatePositionContext(m_pipeline.currentStatus().timelineEpoch);
    publishPosition(committedRelativePosMs, pipelineDelayMs, delayToSourceScale, AudioClock::UpdateMode::Discontinuity,
                    true);

    qCDebug(ENGINE) << "Prepared crossfade commit accepted:" << "trackId=" << track.id()
                    << "streamId=" << preparedStreamId << "preparedBufferedMs=" << preparedStream->bufferedDurationMs()
                    << "preparedPosMs=" << preparedStream->positionMs();

    updateTrackStatus(Engine::TrackStatus::Buffered);
    finaliseTrackCommit(Engine::TransitionMode::Crossfade);
    clearPreparedCrossfadeTransition();
    clearPreparedGaplessTransition();
    return true;
}

bool AudioEngine::armPreparedGaplessTransition(const Engine::PlaybackItem& item, uint64_t generation)
{
    const Track& track = item.track;
    disarmStalePreparedTransitions(track, generation);

    if(!track.isValid() || generation != m_trackGeneration || !hasPlaybackState(Engine::PlaybackState::Playing)
       || m_transitions.isSeekInProgress()) {
        return false;
    }

    const auto transitionMode = effectiveAutoTransitionMode();
    if((transitionMode != AutoTransitionMode::Gapless && transitionMode != AutoTransitionMode::BoundaryFade)
       || isMultiTrackFileTransition(m_currentTrack, track)) {
        return false;
    }

    if(!m_preparedNext || !samePlaybackItem(item, m_preparedNext->item) || !m_preparedNext->format.isValid()) {
        return false;
    }

    const AudioFormat currentMixFormat = m_pipeline.predictPerTrackFormat(m_format);
    const AudioFormat nextMixFormat    = m_pipeline.predictPerTrackFormat(m_preparedNext->format);
    if(!gaplessFormatsMatch(currentMixFormat, nextMixFormat)) {
        return false;
    }

    if(m_preparedGaplessTransition.active) {
        return m_preparedGaplessTransition.sourceGeneration == generation
            && samePlaybackItem(item, preparedGaplessTargetItem());
    }

    if(!m_preparedNext->preparedStream) {
        return false;
    }

    AudioPipeline::TransitionRequest request;
    request.type                    = AudioPipeline::TransitionType::Gapless;
    request.stream                  = m_preparedNext->preparedStream;
    request.signalCurrentEndOfInput = false;
    request.reanchorTimeline        = false;

    const auto transitionResult = m_pipeline.executeTransition(request);
    if(!transitionResult.success || transitionResult.streamId == InvalidStreamId) {
        return false;
    }

    m_preparedGaplessTransition.active           = true;
    m_preparedGaplessTransition.targetTrack      = track;
    m_preparedGaplessTransition.targetItemId     = item.itemId;
    m_preparedGaplessTransition.sourceGeneration = generation;
    m_preparedGaplessTransition.streamId         = transitionResult.streamId;
    m_preparedGaplessTransition.boundaryFadeMode = (transitionMode == AutoTransitionMode::BoundaryFade);
    m_preparedGaplessTransition.decoderAdopted   = false;
    m_preparedGaplessTransition.preCommitTimingStream.reset();
    clearPreparedCrossfadeTransition();
    qCDebug(ENGINE) << "Prepared gapless transition armed:" << "trackId=" << track.id() << "generation=" << generation
                    << "preparedStreamId=" << transitionResult.streamId
                    << "preparedBufferedMs=" << m_preparedNext->preparedStream->bufferedDurationMs()
                    << "currentStreamId="
                    << (m_decoder.activeStream() ? m_decoder.activeStream()->id() : InvalidStreamId)
                    << "mode=" << (transitionMode == AutoTransitionMode::BoundaryFade ? "boundary-fade" : "gapless");
    return true;
}

bool AudioEngine::stagePreparedGaplessDecoder(const Engine::PlaybackItem& item)
{
    const Track& track = item.track;
    disarmStalePreparedTransitions(track, m_trackGeneration);

    const bool boundaryFadeMode     = m_preparedGaplessTransition.boundaryFadeMode;
    const auto previousActiveStream = m_decoder.activeStream();

    if(!track.isValid() || !m_preparedGaplessTransition.active
       || !samePlaybackItem(item, preparedGaplessTargetItem())) {
        if(boundaryFadeMode) {
            clearAutoBoundaryFadeState(true);
        }
        return false;
    }

    const StreamId preparedStreamId = m_preparedGaplessTransition.streamId;

    auto preparedStream = m_decoder.activeStream();
    if(!m_preparedGaplessTransition.decoderAdopted) {
        const auto preservedPreparedGaplessTransition = m_preparedGaplessTransition;
        if(!initDecoder(item, true)) {
            if(boundaryFadeMode) {
                clearAutoBoundaryFadeState(true);
            }
            discardPreparedGaplessTransition(false);
            return false;
        }

        if(!m_preparedGaplessTransition.active && preservedPreparedGaplessTransition.active
           && samePlaybackItem(Engine::PlaybackItem{.track  = preservedPreparedGaplessTransition.targetTrack,
                                                    .itemId = preservedPreparedGaplessTransition.targetItemId},
                               item)) {
            m_preparedGaplessTransition = preservedPreparedGaplessTransition;
        }

        preparedStream = m_decoder.activeStream();
        if(!preparedStream || preparedStream->id() != preparedStreamId) {
            if(boundaryFadeMode) {
                clearAutoBoundaryFadeState(true);
            }
            discardPreparedGaplessTransition(false);
            return false;
        }

        if(previousActiveStream && previousActiveStream->id() != preparedStreamId) {
            m_preparedGaplessTransition.preCommitTimingStream = previousActiveStream;
            previousActiveStream->setEndOfInput();
        }
        else {
            m_preparedGaplessTransition.preCommitTimingStream.reset();
        }

        m_format = m_decoder.format();

        const uint64_t bufferedBeforePrefillMs = preparedStream->bufferedDurationMs();

        m_decoder.startDecoding();

        const int gaplessCommitPrefillMs = std::max(20, m_decoder.lowWatermarkMs());
        if(std::cmp_less(bufferedBeforePrefillMs, gaplessCommitPrefillMs)) {
            prefillActiveStream(gaplessCommitPrefillMs);
        }

        if(m_decoder.isDecodeTimerActive() || hasPlaybackState(Engine::PlaybackState::Playing)) {
            m_decoder.ensureDecodeTimerRunning();
        }

        m_preparedGaplessTransition.decoderAdopted = true;

        qCDebug(ENGINE) << "Prepared gapless decoder staged:" << "trackId=" << track.id()
                        << "generation=" << m_trackGeneration
                        << "previousStreamId=" << (previousActiveStream ? previousActiveStream->id() : InvalidStreamId)
                        << "preparedStreamId=" << preparedStreamId
                        << "preparedBufferedMs=" << preparedStream->bufferedDurationMs() << "timingStreamId="
                        << (m_preparedGaplessTransition.preCommitTimingStream
                                ? m_preparedGaplessTransition.preCommitTimingStream->id()
                                : InvalidStreamId);
    }

    return preparedStream && preparedStream->id() == preparedStreamId;
}

bool AudioEngine::commitPreparedGaplessTransition(const Engine::PlaybackItem& item)
{
    const Track& track = item.track;
    if(!stagePreparedGaplessDecoder(item)) {
        return false;
    }

    const bool boundaryFadeMode = m_preparedGaplessTransition.boundaryFadeMode;
    const auto preparedStream   = m_decoder.activeStream();

    clearAutoCrossfadeTailFadeState();
    if(!boundaryFadeMode) {
        clearAutoBoundaryFadeState();
    }

    setCurrentTrackContext(item);
    setStreamToTrackOriginForTrack(track);
    clearTrackEndLatch();
    m_transitions.clearTrackEnding();

    m_positionCoordinator.clearGaplessHold();

    const uint64_t committedSourcePosMs = (preparedStream->positionMs() > preparedStream->bufferedDurationMs())
                                            ? (preparedStream->positionMs() - preparedStream->bufferedDurationMs())
                                            : 0;
    const uint64_t committedRelativePosMs
        = relativeTrackPositionMs(committedSourcePosMs, m_streamToTrackOriginMs, track.offset());
    const uint64_t pipelineDelayMs  = m_pipeline.playbackDelayMs();
    const double delayToSourceScale = std::clamp(m_pipeline.playbackDelayToTrackScale(), 0.05, 8.0);
    updatePositionContext(m_pipeline.currentStatus().timelineEpoch);
    publishPosition(committedRelativePosMs, pipelineDelayMs, delayToSourceScale, AudioClock::UpdateMode::Discontinuity,
                    true);

    if(boundaryFadeMode) {
        applyAutoBoundaryFadeIn(true);
    }

    qCDebug(ENGINE) << "Prepared gapless commit accepted:" << "trackId=" << track.id()
                    << "streamId=" << (preparedStream ? preparedStream->id() : InvalidStreamId)
                    << "preparedBufferedMs=" << (preparedStream ? preparedStream->bufferedDurationMs() : 0)
                    << "preparedPosMs=" << (preparedStream ? preparedStream->positionMs() : 0);

    updateTrackStatus(Engine::TrackStatus::Buffered);
    finaliseTrackCommit(boundaryFadeMode ? Engine::TransitionMode::BoundaryFade : Engine::TransitionMode::Gapless);
    clearPreparedGaplessTransition();
    return true;
}

void AudioEngine::play()
{
    const auto status = m_trackStatus.load(std::memory_order_relaxed);
    if(status == Engine::TrackStatus::Loading || status == Engine::TrackStatus::Invalid
       || status == Engine::TrackStatus::Unreadable) {
        return;
    }

    if(cancelPendingAudiblePause()) {
        return;
    }

    const auto prevState  = m_playbackState.load(std::memory_order_relaxed);
    const auto playAction = reducePlaybackIntent(
        {
            .playbackState  = prevState,
            .fadeState      = m_fadeController.state(),
            .fadingEnabled  = m_fadingEnabled,
            .pauseFadeOutMs = m_fadingValues.pause.effectiveOutMs(),
            .stopFadeOutMs  = m_fadingValues.stop.effectiveOutMs(),
        },
        PlaybackIntent::Play);

    if(playAction == PlaybackAction::NoOp) {
        return;
    }

    if(!m_decoder.isValid() || !m_decoder.activeStream()) {
        if(hasPlaybackState(Engine::PlaybackState::Stopped) && m_currentTrack.isValid()) {
            loadTrack(currentPlaybackItem(), false);
        }
        if(!m_decoder.isValid() || !m_decoder.activeStream()) {
            return;
        }
    }

    const bool hasActiveTransportFade = m_fadeController.hasPendingFade() || m_pipeline.faderIsFading();
    if(!hasActiveTransportFade && m_fadeController.fadeOnNext()) {
        m_fadeController.setFadeOnNext(false);
    }

    const bool preserveTransportFade
        = !m_fadeController.hasPendingResumeFadeIn() && m_fadeController.fadeOnNext() && hasActiveTransportFade;
    if(preserveTransportFade) {
        if(auto stream = m_decoder.activeStream()) {
            m_pipeline.sendStreamCommand(stream->id(), AudioStream::Command::Play);
        }

        m_decoder.startDecoding();
        m_pipeline.play();

        updatePosition();
        m_audioClock.start();

        updatePlaybackState(Engine::PlaybackState::Playing);
        updateTrackStatus(Engine::TrackStatus::Buffered);
        return;
    }

    m_fadeController.applyPlayFade(prevState, m_fadingEnabled, m_fadingValues, m_volume);

    if(auto stream = m_decoder.activeStream()) {
        m_pipeline.sendStreamCommand(stream->id(), AudioStream::Command::Play);
    }

    m_decoder.startDecoding();
    m_pipeline.play();

    updatePosition();
    m_audioClock.start();

    updatePlaybackState(Engine::PlaybackState::Playing);
    updateTrackStatus(Engine::TrackStatus::Buffered);
}

void AudioEngine::pause()
{
    if(m_pendingAudiblePause.active) {
        return;
    }

    const auto state       = m_playbackState.load(std::memory_order_relaxed);
    const auto pauseAction = reducePlaybackIntent(
        {
            .playbackState  = state,
            .fadeState      = m_fadeController.state(),
            .fadingEnabled  = m_fadingEnabled,
            .pauseFadeOutMs = m_fadingValues.pause.effectiveOutMs(),
            .stopFadeOutMs  = m_fadingValues.stop.effectiveOutMs(),
        },
        PlaybackIntent::Pause);

    if(pauseAction == PlaybackAction::NoOp) {
        return;
    }

    if(pauseAction == PlaybackAction::BeginFade) {
        const uint64_t transitionId = beginTransportTransition();
        if(m_fadeController.beginPauseFade(m_fadingEnabled, m_fadingValues, m_volume, transitionId)) {
            setPhase(Playback::Phase::FadingToPause, PhaseChangeReason::TransportPauseFadeQueued);
            return;
        }
        clearTransportTransition();
    }

    if(pauseAction != PlaybackAction::Immediate && pauseAction != PlaybackAction::BeginFade) {
        return;
    }

    m_pipeline.pause();
    clearPendingAnalysisData();
    m_audioClock.stop();
    updatePlaybackState(Engine::PlaybackState::Paused);
}

void AudioEngine::stop()
{
    clearPendingAudiblePause();
    m_transitions.cancelPendingSeek();

    if(auto stream = m_decoder.activeStream()) {
        if(stream->endOfInput() && stream->bufferEmpty()) {
            const bool naturalEndStop = m_trackStatus.load(std::memory_order_relaxed) == Engine::TrackStatus::End;
            const uint64_t remainingOutputMs = m_pipeline.playbackDelayMs();

            if(naturalEndStop && remainingOutputMs > 0) {
                // Only stop when output has been drained
                QTimer::singleShot(
                    std::chrono::milliseconds{std::min<uint64_t>(remainingOutputMs, std::numeric_limits<int>::max())},
                    this, [this, generation = m_trackGeneration]() {
                        if(generation != m_trackGeneration
                           || m_trackStatus.load(std::memory_order_relaxed) != Engine::TrackStatus::End) {
                            return;
                        }
                        stopImmediate();
                    });
                return;
            }
            stopImmediate();
            return;
        }
    }
    else {
        stopImmediate();
        return;
    }

    const auto state      = m_playbackState.load(std::memory_order_relaxed);
    const auto stopAction = reducePlaybackIntent(
        {
            .playbackState  = state,
            .fadeState      = m_fadeController.state(),
            .fadingEnabled  = m_fadingEnabled,
            .pauseFadeOutMs = m_fadingValues.pause.effectiveOutMs(),
            .stopFadeOutMs  = m_fadingValues.stop.effectiveOutMs(),
        },
        PlaybackIntent::Stop);

    if(stopAction == PlaybackAction::NoOp) {
        return;
    }

    if(stopAction == PlaybackAction::BeginFade) {
        const uint64_t transitionId = beginTransportTransition();
        if(m_fadeController.beginStopFade(m_fadingEnabled, m_fadingValues, m_volume, state, transitionId)) {
            setPhase(Playback::Phase::FadingToStop, PhaseChangeReason::TransportStopFadeQueued);
            return;
        }
        clearTransportTransition();
    }

    if(stopAction != PlaybackAction::Immediate && stopAction != PlaybackAction::BeginFade) {
        return;
    }

    stopImmediate();
}

void AudioEngine::stopImmediate()
{
    clearPendingAudiblePause();
    clearAutoCrossfadeTailFadeState();
    clearAutoBoundaryFadeState();
    m_upcomingTrackCandidate       = {};
    m_upcomingTrackCandidateItemId = 0;
    m_pendingManualTrackItemId     = 0;
    clearAutoAdvanceState();
    m_transitions.setSeekInProgress(false);
    m_transitions.clearForStop();

    m_fadeController.invalidateActiveFade();
    cancelAllFades();
    clearTransportTransition();

    m_audioClock.stop();

    m_decoder.stopDecoding();
    m_pipeline.stopPlayback();
    cleanupDecoderActiveStreamFromPipeline(false);
    m_pipeline.cleanupOrphanImmediate();
    m_outputController.uninitOutput();

    clearPreparedNextTrackAndCancelPendingJobs();
    m_decoder.reset();
    resetStreamToTrackOrigin();
    clearTrackEndLatch();
    clearPendingAnalysisData();
    m_positionCoordinator.reset();
    updatePositionContext(m_pipeline.currentStatus().timelineEpoch);

    publishPosition(0, 0, 1.0, AudioClock::UpdateMode::Discontinuity, true);

    updatePlaybackState(Engine::PlaybackState::Stopped);
    updateTrackStatus(Engine::TrackStatus::NoTrack);
    publishBitrate(0);
    emit finished();
}

void AudioEngine::restorePosition(uint64_t positionMs, bool pause)
{
    clearPendingAudiblePause();

    if(!m_currentTrack.isValid()) {
        return;
    }

    if(!m_decoder.isValid() || !m_decoder.activeStream()) {
        m_transitions.queueInitialSeek(positionMs, m_currentTrack.id(), 0);
        loadTrack(currentPlaybackItem(), false);
        if(!m_decoder.isValid() || !m_decoder.activeStream()) {
            return;
        }
    }
    else {
        performSimpleSeek(positionMs, 0);
        if(!m_decoder.isValid() || !m_decoder.activeStream()) {
            return;
        }
    }

    m_pipeline.pause();
    clearPendingAnalysisData();
    m_audioClock.stop();

    if(pause) {
        updatePlaybackState(Engine::PlaybackState::Paused);
    }
}

void AudioEngine::seek(uint64_t positionMs)
{
    seekWithRequest(positionMs, 0);
}

void AudioEngine::seekWithRequest(uint64_t positionMs, uint64_t requestId)
{
    const bool shouldStartPlayback = hasPlaybackState(Engine::PlaybackState::Stopped);

    if(!m_decoder.isValid()) {
        const int trackId = m_currentTrack.isValid() ? m_currentTrack.id() : -1;
        m_transitions.queueInitialSeek(positionMs, trackId, requestId);

        // Stopped seek is expected to begin playback from the requested position.
        // Queue the pending seek first so setupNewTrackStream applies it.
        if(shouldStartPlayback && m_currentTrack.isValid()) {
            loadTrack(currentPlaybackItem(), false);
            play();
        }
        return;
    }

    performSeek(positionMs, requestId);
    if(shouldStartPlayback) {
        play();
    }
}

void AudioEngine::setVolume(double volume)
{
    m_volume = std::clamp(volume, 0.0, 1.0);
    m_pipeline.setOutputVolume(m_volume);
}

void AudioEngine::setAudioOutput(const OutputCreator& output, const QString& device)
{
    qCDebug(ENGINE) << "Setting audio output, device:" << device;

    const bool wasPlaying = m_playbackState.load(std::memory_order_relaxed) == Engine::PlaybackState::Playing;

    if(wasPlaying) {
        cancelFadesForReinit();
        m_outputController.pauseOutputImmediate(true);
    }

    m_outputController.setOutputCreator(output);
    m_outputController.setOutputDevice(device);

    if(m_format.isValid()) {
        m_outputController.uninitOutput();
        const bool initOk = m_outputController.initOutput(m_format, m_volume);
        if(initOk) {
            if(wasPlaying) {
                play();
            }
        }
    }
}

void AudioEngine::applyOutputProfile(const OutputCreator& output, const QString& device, SampleFormat bitdepth,
                                     bool dither, const Engine::DspChains& chain)
{
    qCDebug(ENGINE) << "Applying output profile:" << "device=" << device << "bitdepth=" << static_cast<int>(bitdepth)
                    << "dither=" << dither;

    const bool wasPlaying = m_playbackState.load(std::memory_order_relaxed) == Engine::PlaybackState::Playing;

    if(wasPlaying) {
        cancelFadesForReinit();
        m_outputController.pauseOutputImmediate(true);
    }

    m_outputController.setOutputCreator(output);
    m_outputController.setOutputDevice(device);

    m_pipeline.setOutputBitdepth(bitdepth);
    m_pipeline.setDither(dither);

    const auto buildNodes = [this](const Engine::DspChain& defs) -> std::vector<DspNodePtr> {
        if(!m_dspRegistry) {
            return {};
        }

        std::vector<DspNodePtr> nodes;
        nodes.reserve(defs.size());

        for(const auto& entry : defs) {
            auto node = m_dspRegistry->create(entry.id);
            if(!node) {
                continue;
            }

            node->setInstanceId(entry.instanceId);
            node->setEnabled(entry.enabled);
            if(!entry.settings.isEmpty()) {
                node->loadSettings(entry.settings);
            }

            nodes.push_back(std::move(node));
        }

        return nodes;
    };

    std::vector<DspNodePtr> masterNodes = buildNodes(chain.masterChain);
    m_pipeline.setDspChain(std::move(masterNodes), chain.perTrackChain, m_format);

    if(m_format.isValid()) {
        m_outputController.uninitOutput();
        const bool initOk = m_outputController.initOutput(m_format, m_volume);
        if(initOk && wasPlaying) {
            play();
        }
    }
}

void AudioEngine::setOutputDevice(const QString& device)
{
    if(device == m_outputController.outputDevice()) {
        return;
    }

    qCDebug(ENGINE) << "Changing output device to:" << device;

    m_outputController.setOutputDevice(device);

    if(!m_format.isValid()) {
        m_pipeline.setOutputDevice(device);
        return;
    }

    const bool wasPlaying = m_playbackState.load(std::memory_order_relaxed) == Engine::PlaybackState::Playing;
    if(wasPlaying) {
        cancelFadesForReinit();
        m_outputController.pauseOutputImmediate(true);
    }

    m_outputController.uninitOutput();
    const bool initOk = m_outputController.initOutput(m_format, m_volume);
    if(initOk && wasPlaying) {
        play();
    }
}

void AudioEngine::setDspChain(const Engine::DspChains& chain)
{
    const auto buildNodes = [this](const Engine::DspChain& defs) -> std::vector<DspNodePtr> {
        if(!m_dspRegistry) {
            return {};
        }

        std::vector<DspNodePtr> nodes;
        nodes.reserve(defs.size());

        for(const auto& entry : defs) {
            auto node = m_dspRegistry->create(entry.id);
            if(!node) {
                continue;
            }

            node->setInstanceId(entry.instanceId);
            node->setEnabled(entry.enabled);
            if(!entry.settings.isEmpty()) {
                node->loadSettings(entry.settings);
            }

            nodes.push_back(std::move(node));
        }

        return nodes;
    };

    std::vector<DspNodePtr> masterNodes = buildNodes(chain.masterChain);

    const AudioFormat inputFormat   = m_format;
    const AudioFormat currentOutput = m_pipeline.outputFormat();
    AudioFormat desiredOutput       = m_pipeline.setDspChain(std::move(masterNodes), chain.perTrackChain, inputFormat);

    if(!desiredOutput.isValid() && inputFormat.isValid()) {
        desiredOutput = inputFormat;
    }

    const bool outputFormatChanged
        = inputFormat.isValid() && m_pipeline.hasOutput() && outputReinitRequired(currentOutput, desiredOutput);

    if(outputFormatChanged) {
        reinitOutputForCurrentFormat();
    }
}

void AudioEngine::updateLiveDspSettings(const Engine::LiveDspSettingsUpdate& update)
{
    m_pipeline.updateLiveDspSettings(update);
}

Engine::PlaybackState AudioEngine::playbackState() const
{
    return m_playbackState.load(std::memory_order_relaxed);
}

Engine::TrackStatus AudioEngine::trackStatus() const
{
    return m_trackStatus.load(std::memory_order_relaxed);
}

uint64_t AudioEngine::position() const
{
    return m_audioClock.position();
}

bool AudioEngine::event(QEvent* event)
{
    if(!event) {
        return false;
    }

    if(m_engineTaskQueue.isShuttingDown()) {
        if(event->type() == engineTaskEventType() || event->type() == pipelineWakeEventType()) {
            return true;
        }
    }

    if(event->type() == engineTaskEventType()) {
        m_engineTaskQueue.drain();
        return true;
    }

    if(event->type() == pipelineWakeEventType()) {
        if(!m_engineTaskQueue.isBarrierActive() && !m_engineTaskQueue.isDraining()) {
            handlePipelineWakeSignals(m_pipeline.drainPendingSignals());
        }
        else {
            schedulePipelineWakeDrainTask();
        }
        return true;
    }

    return QObject::event(event);
}

void AudioEngine::timerEvent(QTimerEvent* event)
{
    if(!event) {
        return;
    }

    const int timerId = event->timerId();
    if(QThread::currentThread() == thread() && !m_engineTaskQueue.isDraining()) {
        handleTimerTick(timerId);
        return;
    }

    m_engineTaskQueue.enqueue(
        EngineTaskType::Timer, [engine = this, timerId]() { engine->handleTimerTick(timerId); }, timerId);
}
} // namespace Fooyin
