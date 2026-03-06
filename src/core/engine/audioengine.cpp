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

#include <algorithm>
#include <array>
#include <atomic>
#include <limits>
#include <utility>

Q_LOGGING_CATEGORY(ENGINE, "fy.engine")

namespace {
constexpr auto BasePrefillDecodeFrames      = 4096;
constexpr auto MaxPrefillDecodeFrames       = 262144;
constexpr auto PrefillChunkDurationMs       = 10;
constexpr auto PositionSyncTimerCoalesceKey = 0;
constexpr uint64_t GaplessPrepareLeadMs     = 300;
constexpr auto MaxCrossfadePrefillMs        = 1000;
constexpr auto GaplessHandoffPrefillMs      = 250;
constexpr auto DecodeHighWatermarkMaxRatio  = 0.99;
constexpr auto DecodeWatermarkMinHeadroomMs = 20;
constexpr auto DecodeWatermarkMinGapMs      = 30;

uint64_t saturatingAddWindow(const uint64_t lhs, const uint64_t rhs)
{
    return lhs > (std::numeric_limits<uint64_t>::max() - rhs) ? std::numeric_limits<uint64_t>::max() : lhs + rhs;
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

} // namespace

namespace Fooyin {
using EngineTaskType = EngineTaskQueue::TaskType;

AudioEngine::AudioEngine(std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings, DspRegistry* dspRegistry,
                         QObject* parent)
    : QObject{parent}
    , m_settings{settings}
    , m_dspRegistry{dspRegistry}
    , m_audioLoader{std::move(audioLoader)}
    , m_engineTaskQueue{this, AudioEngine::engineTaskEventType()}
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
    , m_fadeController{&m_pipeline}
    , m_outputController{this, &m_pipeline}
    , m_volume{m_settings->value<Settings::Core::OutputVolume>()}
    , m_bufferLengthMs{m_settings->value<Settings::Core::BufferLength>()}
    , m_decodeLowWatermarkRatio{m_settings->value<Settings::Core::Internal::DecodeLowWatermarkRatio>()}
    , m_decodeHighWatermarkRatio{m_settings->value<Settings::Core::Internal::DecodeHighWatermarkRatio>()}
    , m_fadingEnabled{m_settings->value<Settings::Core::Internal::EngineFading>()}
    , m_crossfadeEnabled{m_settings->value<Settings::Core::Internal::EngineCrossfading>()}
    , m_gaplessEnabled{m_settings->value<Settings::Core::GaplessPlayback>()}
    , m_audioClock{this}
    , m_lastReportedBitrate{0}
    , m_lastAppliedSeekRequestId{0}
    , m_positionContextTrackGeneration{0}
    , m_positionContextTimelineEpoch{0}
    , m_positionContextSeekRequestId{0}
    , m_autoCrossfadeTailFadeActive{false}
    , m_autoCrossfadeTailFadeStreamId{InvalidStreamId}
    , m_autoCrossfadeTailFadeGeneration{0}
{
    setupSettings();

    QObject::connect(&m_audioClock, &AudioClock::positionChanged, this, [this](uint64_t positionMs) {
        emit positionChanged(positionMs);
        emit positionChangedWithContext(positionMs, m_positionContextTrackGeneration, m_positionContextTimelineEpoch,
                                        m_positionContextSeekRequestId);
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
    m_pipeline.setSignalWakeTarget(this, AudioEngine::pipelineWakeEventType());
    m_pipeline.setAnalysisBus(m_analysisBus.get());

    m_nextTrackPrepareWorker.start(
        [this](uint64_t jobToken, uint64_t requestId, const Track& track, NextTrackPreparationState preparedState) {
            m_engineTaskQueue.enqueue(EngineTaskType::Worker, [this, jobToken, requestId, track,
                                                               preparedState = std::move(preparedState)]() mutable {
                applyPreparedNextTrackResult(jobToken, requestId, track, std::move(preparedState));
            });
        });
}

QEvent::Type AudioEngine::engineTaskEventType()
{
    static const auto eventType = static_cast<QEvent::Type>(QEvent::registerEventType());
    return eventType;
}

QEvent::Type AudioEngine::pipelineWakeEventType()
{
    static const auto eventType = static_cast<QEvent::Type>(QEvent::registerEventType());
    return eventType;
}

void AudioEngine::beginShutdown()
{
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

bool AudioEngine::ensureCrossfadePrepared(const Track& track, bool isManualChange)
{
    if(!m_preparedNext || track != m_preparedNext->track) {
        if(isManualChange) {
            // Manual crossfades need prepared state immediately
            cancelPendingPrepareJobs();
            if(!prepareNextTrackImmediate(track)) {
                return false;
            }
        }
        else {
            prepareNextTrack(track);
        }
    }

    return true;
}

bool AudioEngine::handleManualChangeFade(const Track& track)
{
    const bool manualCrossfadeConfigured = m_crossfadeEnabled && m_crossfadingValues.manualChange.isConfigured();

    if(manualCrossfadeConfigured) {
        return startTrackCrossfade(track, true);
    }

    const uint64_t transitionId = nextTransitionId();
    if(m_fadeController.handleManualChangeFade(track, m_crossfadeEnabled, m_crossfadingValues,
                                               hasPlaybackState(Engine::PlaybackState::Playing), m_volume,
                                               transitionId)) {
        setPhase(Playback::Phase::FadingToManualChange, PhaseChangeReason::ManualChangeFadeQueued);
        return true;
    }

    return false;
}

bool AudioEngine::startTrackCrossfade(const Track& track, bool isManualChange)
{
    if(!track.isValid()) {
        return false;
    }

    if(!isManualChange && m_fadeController.hasPendingFade()) {
        return false;
    }

    if(!isManualChange && isMultiTrackFileTransition(m_currentTrack, track)) {
        return false;
    }

    if(!ensureCrossfadePrepared(track, isManualChange)) {
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
        prefillActiveStream(std::min(fadeOutDurationMs, m_bufferLengthMs));

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

    setCurrentTrackContext(track);
    updateTrackStatus(Engine::TrackStatus::Loading);
    m_streamToTrackOriginMs = track.offset();
    clearTrackEndLatch();
    m_transitions.clearTrackEnding();

    const bool wasDecoding = m_decoder.isDecodeTimerActive();
    if(wasDecoding) {
        m_decoder.stopDecodeTimer();
    }

    if(!initDecoder(track, true)) {
        m_pipeline.cleanupOrphanImmediate();
        updateTrackStatus(Engine::TrackStatus::Invalid);
        return true;
    }

    syncDecoderTrackMetadata();
    m_format = m_decoder.format();

    const auto newStream = m_decoder.setupCrossfadeStream(m_bufferLengthMs, Engine::FadeCurve::Sine);
    if(!newStream) {
        updateTrackStatus(Engine::TrackStatus::Invalid);
        return true;
    }

    const int basePrefillMs = std::min(m_bufferLengthMs / 4, MaxCrossfadePrefillMs);
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

    return true;
}

void AudioEngine::performSeek(uint64_t positionMs, uint64_t requestId)
{
    if(!m_decoder.isValid()) {
        return;
    }

    m_positionCoordinator.clearGaplessHold();

    clearAutoCrossfadeTailFadeState();

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
        .bufferLengthMs         = m_bufferLengthMs,
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
        = m_decoder.prepareSeekStream(seekPosMs, m_bufferLengthMs, Engine::FadeCurve::Sine, m_currentTrack);
    if(!newStream) {
        m_transitions.setSeekInProgress(false);
        stopImmediate();
        return;
    }

    const int requestedPrefillMs = std::max(fadeInDurationMs, m_bufferLengthMs / 4);
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

    const int seekPrefillMs = prefillTargetMs > 0 ? prefillTargetMs : (m_bufferLengthMs / 4);
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
        requestedMs = m_bufferLengthMs / 4;
    }

    return std::clamp(requestedMs, 1, std::max(1, m_bufferLengthMs));
}

void AudioEngine::cancelAllFades()
{
    m_fadeController.cancelAll(m_fadingValues.pause.curve);
}

void AudioEngine::cancelFadesForReinit()
{
    m_fadeController.cancelForReinit(m_fadingValues.pause.curve);
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
        clearTransportTransition();
        m_pipeline.pause();
        m_pipeline.resetOutput();
        clearPendingAnalysisData();
        m_audioClock.stop();
        m_fadeController.setState(FadeState::Idle);
        m_fadeController.setFadeOnNext(false);
        updatePlaybackState(Engine::PlaybackState::Paused);
    }
    if(result.loadTrack) {
        loadTrack(*result.loadTrack, false);
    }
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
    if(changedTrack.bitrate() > 0) {
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

    const int decoderBitrate = m_decoder.bitrate();
    if(decoderBitrate > 0) {
        publishBitrate(decoderBitrate);
    }
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

void AudioEngine::clearAutoCrossfadeTailFadeState()
{
    m_autoCrossfadeTailFadeActive     = false;
    m_autoCrossfadeTailFadeStreamId   = InvalidStreamId;
    m_autoCrossfadeTailFadeGeneration = 0;
}

void AudioEngine::handleTrackEndingSignals(const AudioStreamPtr& stream, uint64_t trackEndingPosMs,
                                           bool preparedCrossfadeArmed, bool boundaryFallbackReached)
{
    const auto result         = checkTrackEnding(stream, trackEndingPosMs);
    const auto transitionMode = m_transitions.autoTransitionMode();

    if(result.aboutToFinish) {
        maybeBeginAutoCrossfadeTailFadeOut(stream, trackEndingPosMs);
        emit trackAboutToFinish(m_currentTrack, m_trackGeneration);
    }
    if(result.readyToSwitch && transitionMode == AutoTransitionMode::Crossfade) {
        emit trackReadyToSwitch(m_currentTrack, m_trackGeneration);
    }
    if(result.boundaryReached || (preparedCrossfadeArmed && boundaryFallbackReached)) {
        qCDebug(ENGINE) << "Prepared boundary signal:"
                        << "trackId=" << m_currentTrack.id() << "generation=" << m_trackGeneration
                        << "trackPosMs=" << trackEndingPosMs << "durationMs=" << m_currentTrack.duration()
                        << "boundaryReached=" << result.boundaryReached << "fallbackReached=" << boundaryFallbackReached
                        << "preparedCrossfadeArmed=" << preparedCrossfadeArmed
                        << "streamBufferedMs=" << (stream ? stream->bufferedDurationMs() : 0)
                        << "streamEndOfInput=" << (stream ? stream->endOfInput() : false);
        m_preparedCrossfadeTransition.boundarySignalled = true;
        emit trackBoundaryReached(m_currentTrack, m_trackGeneration);
    }
    if(result.endReached) {
        // Same-file logical segment transitions can reach "track end" before
        // the underlying stream is actually drained. In that case, avoid
        // flushing DSP state.
        const bool shouldFlushDspOnEnd = stream->endOfInput() && stream->bufferEmpty();
        signalTrackEndOnce(shouldFlushDspOnEnd);
    }
}

void AudioEngine::updatePosition()
{
    auto stream = m_decoder.activeStream();
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

    if(output.evaluateTrackEnding) {
        handleTrackEndingSignals(stream, output.trackEndingPosMs, output.preparedCrossfadeArmed,
                                 boundaryFallbackReached);
    }
}

void AudioEngine::handleTimerTick(int timerId)
{
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
            = decodeWatermarksForBufferMs(m_bufferLengthMs, safeLowRatio, safeHighRatio);

        const int clampedBufferMs = std::max(200, m_bufferLengthMs);
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
    m_settings->subscribe<Settings::Core::Internal::CrossfadingValues>(this, updateFadeDurations);
    m_settings->subscribe<Settings::Core::BufferLength>(this, [this, updateDecodeWatermarks](int bufferLengthMs) {
        m_bufferLengthMs = std::max(200, bufferLengthMs);
        updateDecodeWatermarks();
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
            clearPreparedGaplessTransition();
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

        const bool outputFormatChanged = inputFormat.isValid() && m_pipeline.hasOutput() && desiredOutput.isValid()
                                      && currentOutput.isValid()
                                      && !Audio::outputFormatsMatch(currentOutput, desiredOutput);

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

    updateTrackStatus(Engine::TrackStatus::End, flushDspOnEnd);
    return true;
}

void AudioEngine::clearTrackEndLatch()
{
    m_lastEndedTrack = {};
}

AudioEngine::TrackEndingResult AudioEngine::checkTrackEnding(const AudioStreamPtr& stream, uint64_t relativePosMs)
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

    input.positionMs              = relativePosMs;
    input.durationMs              = m_currentTrack.duration();
    input.durationBoundaryEnabled = m_currentTrack.hasCue();
    input.timelineDelayMs         = timelineDelayMs;
    input.remainingOutputMs       = remainingOutputMs;
    input.endOfInput              = stream->endOfInput();
    input.bufferEmpty             = stream->bufferEmpty();
    input.autoCrossfadeEnabled    = m_crossfadeEnabled && m_crossfadingValues.autoChange.isConfigured();
    input.gaplessEnabled          = m_gaplessEnabled;
    input.autoFadeOutMs           = m_crossfadingValues.autoChange.effectiveOutMs();
    input.autoFadeInMs            = m_crossfadingValues.autoChange.effectiveInMs();
    input.gaplessPrepareWindowMs  = GaplessPrepareLeadMs;

    return m_transitions.evaluateTrackEnding(input);
}

uint64_t AudioEngine::preferredPreparedPrefillMs() const
{
    const uint64_t transitionDelayMs = m_pipeline.transitionPlaybackDelayMs();
    const double delayToTrackScale   = std::clamp(m_pipeline.playbackDelayToTrackScale(), 0.05, 8.0);
    const auto scaledDelay           = static_cast<uint64_t>(std::max<long double>(
        0.0, std::llround(static_cast<long double>(transitionDelayMs) * static_cast<long double>(delayToTrackScale))));

    uint64_t overlapLeadMs{0};
    if(m_crossfadeEnabled && m_crossfadingValues.autoChange.isConfigured()) {
        const uint64_t fadeOutMs = static_cast<uint64_t>(std::max(0, m_crossfadingValues.autoChange.effectiveOutMs()));
        const uint64_t fadeInMs  = static_cast<uint64_t>(std::max(0, m_crossfadingValues.autoChange.effectiveInMs()));
        overlapLeadMs            = std::min(fadeOutMs, fadeInMs);
    }

    static constexpr uint64_t PrefillSafetyMarginMs = 100;

    const auto withOverlap = saturatingAddWindow(scaledDelay, overlapLeadMs);
    return saturatingAddWindow(withOverlap, PrefillSafetyMarginMs);
}

std::optional<AudioEngine::AutoTransitionEligibility>
AudioEngine::evaluateAutoTransitionEligibility(const Track& track, bool isManualChange,
                                               bool requireTransitionReady) const
{
    if(!isManualChange && m_fadeController.hasPendingFade()) {
        return {};
    }

    if(!track.isValid()) {
        return {};
    }

    if(!isManualChange && isMultiTrackFileTransition(m_currentTrack, track)) {
        // Keep multi-track same-file transitions on the non-crossfade path
        return {};
    }

    const auto& transitionSpec = !isManualChange ? m_crossfadingValues.autoChange : m_crossfadingValues.manualChange;
    const int baseFadeOutMs    = transitionSpec.effectiveOutMs();
    const int baseFadeInMs     = transitionSpec.effectiveInMs();

    const bool crossfadeMix   = m_crossfadeEnabled && (baseFadeInMs > 0 || baseFadeOutMs > 0);
    const bool gaplessHandoff = !isManualChange && m_gaplessEnabled
                             && (!m_crossfadeEnabled || !m_crossfadingValues.autoChange.isConfigured());

    if(!crossfadeMix && !gaplessHandoff) {
        return {};
    }

    if(!isManualChange && requireTransitionReady) {
        if(!m_transitions.isReadyForAutoTransition()) {
            return {};
        }
        const auto readyMode = m_transitions.autoTransitionMode();
        if(crossfadeMix && readyMode != AutoTransitionMode::Crossfade) {
            return {};
        }
        if(!crossfadeMix && gaplessHandoff && readyMode != AutoTransitionMode::Gapless) {
            return {};
        }
    }

    if(!hasPlaybackState(Engine::PlaybackState::Playing)) {
        return {};
    }

    if(!m_decoder.isValid()) {
        return {};
    }

    if(m_transitions.isSeekInProgress()) {
        return {};
    }

    if(!m_preparedNext || track != m_preparedNext->track || !m_preparedNext->format.isValid()) {
        return {};
    }

    const auto inputFormat = m_pipeline.inputFormat();
    if(!inputFormat.isValid()) {
        return {};
    }

    const AudioFormat currentMixFormat = m_pipeline.predictPerTrackFormat(inputFormat);
    const AudioFormat nextMixFormat    = m_pipeline.predictPerTrackFormat(m_preparedNext->format);
    if(!currentMixFormat.isValid() || !nextMixFormat.isValid()
       || !Audio::inputFormatsMatch(currentMixFormat, nextMixFormat)) {
        return {};
    }

    auto currentStream = m_decoder.activeStream();
    if(!currentStream) {
        return {};
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
        return {};
    }

    return AutoTransitionEligibility{
        .crossfadeMix      = crossfadeMix,
        .gaplessHandoff    = gaplessHandoff,
        .fadeOutDurationMs = fadeOutDurationMs,
        .fadeInDurationMs  = fadeInDurationMs,
        .nextMixFormat     = nextMixFormat,
    };
}

bool AudioEngine::isAutoTransitionEligible(const Track& track) const
{
    return evaluateAutoTransitionEligibility(track, false, false).has_value();
}

void AudioEngine::setCurrentTrackContext(const Track& track)
{
    m_currentTrack = track;
    ++m_trackGeneration;
    m_lastAppliedSeekRequestId = 0;
    m_positionCoordinator.resetContinuity();

    updatePositionContext(m_pipeline.currentStatus().timelineEpoch);
    publishBitrate(track.bitrate());
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

bool AudioEngine::initDecoder(const Track& track, bool allowPreparedStream)
{
    m_decoder.reset();

    if(!track.isValid()) {
        return false;
    }

    if(m_preparedNext && track == m_preparedNext->track) {
        auto& prepared               = *m_preparedNext;
        const bool hasPreparedStream = prepared.preparedStream != nullptr;

        if(!m_decoder.adoptPreparedDecoder(std::move(prepared.decoder), std::move(prepared.file), prepared.source,
                                           track, prepared.format)) {
            qCWarning(ENGINE) << "Failed to adopt prepared decoder";
            clearPreparedNextTrack();
            return false;
        }

        if(hasPreparedStream) {
            if(allowPreparedStream) {
                m_decoder.setActiveStream(prepared.preparedStream);
            }
            else {
                m_decoder.seek(m_decoder.startPosition());
            }
        }

        clearPreparedNextTrack();
        return true;
    }

    auto decoder = m_audioLoader->decoderForTrack(track);
    if(!decoder) {
        qCWarning(ENGINE) << "No decoder available for track";
        return false;
    }

    if(!m_decoder.init(std::move(decoder), track)) {
        qCWarning(ENGINE) << "Failed to initialse decoder";
        return false;
    }

    return true;
}

bool AudioEngine::setupNewTrackStream(const Track& track, bool applyPendingSeek)
{
    const auto startupPrefillMs = [this]() {
        const int baseMs          = m_bufferLengthMs / 4;
        const int decodeHighMs    = std::max(1, m_decoder.highWatermarkMs());
        const auto outputSnapshot = m_pipeline.outputQueueSnapshot();
        const int outputDemandMs  = startupPrefillFromOutputQueueMs(outputSnapshot, m_pipeline.outputFormat());
        const int targetMs        = std::max({baseMs, decodeHighMs, outputDemandMs});
        return std::clamp(targetMs, 1, std::max(1, m_bufferLengthMs));
    }();

    const size_t bufferSamples
        = Audio::bufferSamplesFromMs(m_bufferLengthMs, m_format.sampleRate(), m_format.channelCount());

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
    m_preparedNext.reset();
    clearPreparedCrossfadeTransition();
    clearPreparedGaplessTransition();
}

void AudioEngine::clearPreparedCrossfadeTransition()
{
    m_preparedCrossfadeTransition.active            = false;
    m_preparedCrossfadeTransition.targetTrack       = {};
    m_preparedCrossfadeTransition.sourceGeneration  = 0;
    m_preparedCrossfadeTransition.streamId          = InvalidStreamId;
    m_preparedCrossfadeTransition.boundaryLeadMs    = 0;
    m_preparedCrossfadeTransition.bufferedAtArmMs   = 0;
    m_preparedCrossfadeTransition.boundarySignalled = false;
}

void AudioEngine::clearPreparedGaplessTransition()
{
    m_preparedGaplessTransition.active           = false;
    m_preparedGaplessTransition.targetTrack      = {};
    m_preparedGaplessTransition.sourceGeneration = 0;
    m_preparedGaplessTransition.streamId         = InvalidStreamId;
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
        clearPreparedGaplessTransition();
    };

    clearStaleCrossfade();
    clearStaleGapless();
}

void AudioEngine::clearPreparedNextTrackAndCancelPendingJobs()
{
    cancelPendingPrepareJobs();
    clearPreparedNextTrack();
}

bool AudioEngine::prepareNextTrackImmediate(const Track& track)
{
    if(!track.isValid()) {
        return false;
    }

    NextTrackPreparer::Context context;
    context.audioLoader        = m_audioLoader;
    context.currentTrack       = m_currentTrack;
    context.playbackState      = m_playbackState.load(std::memory_order_relaxed);
    context.playbackHints      = m_decoderPlaybackHints;
    context.bufferLengthMs     = m_bufferLengthMs;
    context.preferredPrefillMs = preferredPreparedPrefillMs();

    auto prepared = NextTrackPreparer::prepare(track, context);
    if(!prepared.isValid()) {
        clearPreparedNextTrack();
        return false;
    }

    m_preparedNext = std::move(prepared);
    return true;
}

void AudioEngine::cancelPendingPrepareJobs()
{
    m_nextTrackPrepareWorker.cancelPendingJobs();
}

void AudioEngine::enqueuePrepareNextTrack(const Track& track, uint64_t requestId)
{
    NextTrackPrepareWorker::Request request;
    request.requestId                  = requestId;
    request.track                      = track;
    request.context.audioLoader        = m_audioLoader;
    request.context.currentTrack       = m_currentTrack;
    request.context.playbackState      = m_playbackState.load(std::memory_order_relaxed);
    request.context.playbackHints      = m_decoderPlaybackHints;
    request.context.bufferLengthMs     = m_bufferLengthMs;
    request.context.preferredPrefillMs = preferredPreparedPrefillMs();

    m_nextTrackPrepareWorker.replacePending(std::move(request));
}

void AudioEngine::applyPreparedNextTrackResult(uint64_t jobToken, uint64_t requestId, const Track& track,
                                               NextTrackPreparationState prepared)
{
    if(jobToken != m_nextTrackPrepareWorker.activeJobToken()) {
        return;
    }

    bool ready{false};

    if(prepared.isValid()) {
        m_preparedNext = std::move(prepared);
        ready          = isAutoTransitionEligible(track);
    }
    else {
        clearPreparedNextTrack();
    }

    emit nextTrackReadiness(track, ready, requestId);
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

bool AudioEngine::executeSegmentSwitchLoad(const Track& track, bool preserveTransportFade)
{
    clearAutoCrossfadeTailFadeState();

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
        setCurrentTrackContext(track);
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
        return true;
    }

    qCWarning(ENGINE) << "Falling back to full track load for contiguous segment switch:" << track.filenameExt();
    return false;
}

bool AudioEngine::executeManualFadeDeferredLoad(const Track& track)
{
    if(!handleManualChangeFade(track)) {
        return false;
    }

    qCDebug(ENGINE) << "Deferring manual track load until fade transition completes:" << track.filenameExt();
    return true;
}

bool AudioEngine::executeAutoTransitionLoad(const Track& track)
{
    if(!startTrackCrossfade(track, false)) {
        return false;
    }

    qCDebug(ENGINE) << "Starting automatic transition to:" << track.filenameExt();
    return true;
}

void AudioEngine::executeFullReinitLoad(const Track& track, bool manualChange, bool preserveTransportFade)
{
    qCDebug(ENGINE) << "Loading track:" << track.filenameExt();

    clearAutoCrossfadeTailFadeState();
    clearPendingAnalysisData();
    m_decoder.stopDecoding();
    cleanupActiveStream();

    const auto prevState = m_playbackState.load(std::memory_order_relaxed);

    setCurrentTrackContext(track);
    updateTrackStatus(Engine::TrackStatus::Loading);
    setStreamToTrackOriginForTrack(track);
    clearTrackEndLatch();
    if(!preserveTransportFade) {
        m_fadeController.invalidateActiveFade();
    }

    if(!initDecoder(track, !manualChange)) {
        updateTrackStatus(Engine::TrackStatus::Invalid);
        return;
    }

    syncDecoderTrackMetadata();
    m_format = m_decoder.format();
    m_transitions.clearTrackEnding();

    const AudioFormat oldInputFormat = m_pipeline.inputFormat();
    AudioFormat desiredOutput        = m_pipeline.predictOutputFormat(m_format);
    if(!desiredOutput.isValid()) {
        desiredOutput = m_format;
    }

    const AudioFormat currentOutput = m_pipeline.outputFormat();
    const bool inputFormatChanged   = oldInputFormat.isValid() && !Audio::inputFormatsMatch(oldInputFormat, m_format);
    const bool outputFormatChanged  = currentOutput.isValid() && desiredOutput.isValid()
                                  && !Audio::outputFormatsMatch(currentOutput, desiredOutput);
    const bool hasOutput = m_pipeline.hasOutput();

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
}

void AudioEngine::executeLoadPlan(const Track& track, bool manualChange, const TrackLoadContext& context,
                                  const LoadPlan& plan)
{
    const bool preserveTransportFade = context.hasPendingTransportFade;

    switch(plan.firstStrategy) {
        case LoadStrategy::SegmentSwitch:
            if(executeSegmentSwitchLoad(track, preserveTransportFade)) {
                return;
            }
            if(plan.tryAutoTransition && executeAutoTransitionLoad(track)) {
                return;
            }
            executeFullReinitLoad(track, manualChange, preserveTransportFade);
            return;
        case LoadStrategy::ManualFadeDefer:
            if(executeManualFadeDeferredLoad(track)) {
                return;
            }
            executeFullReinitLoad(track, manualChange, preserveTransportFade);
            return;
        case LoadStrategy::AutoTransition:
            if(executeAutoTransitionLoad(track)) {
                return;
            }
            executeFullReinitLoad(track, manualChange, preserveTransportFade);
            return;
        case LoadStrategy::FullReinit:
            executeFullReinitLoad(track, manualChange, preserveTransportFade);
            return;
    }
}

void AudioEngine::loadTrack(const Track& track, bool manualChange)
{
    if(manualChange) {
        clearPreparedCrossfadeTransition();
        clearPreparedGaplessTransition();
    }
    else {
        disarmStalePreparedTransitions(track, m_trackGeneration);
    }

    const TrackLoadContext loadContext = buildLoadContext(track, manualChange);
    const LoadPlan loadPlan            = planTrackLoad(loadContext);
    executeLoadPlan(track, manualChange, loadContext, loadPlan);
}

void AudioEngine::prepareNextTrack(const Track& track, uint64_t requestId)
{
    if(!track.isValid()) {
        clearPreparedNextTrackAndCancelPendingJobs();
        emit nextTrackReadiness(track, false, requestId);
        return;
    }

    if(m_preparedNext && track == m_preparedNext->track && m_preparedNext->format.isValid()) {
        const bool ready = isAutoTransitionEligible(track);
        emit nextTrackReadiness(track, ready, requestId);
        return;
    }

    clearPreparedNextTrack();
    enqueuePrepareNextTrack(track, requestId);
}

bool AudioEngine::armPreparedCrossfadeTransition(const Track& track, uint64_t generation)
{
    disarmStalePreparedTransitions(track, generation);

    if(!track.isValid() || generation != m_trackGeneration || !hasPlaybackState(Engine::PlaybackState::Playing)
       || m_transitions.isSeekInProgress()) {
        return false;
    }

    if(m_transitions.autoTransitionMode() != AutoTransitionMode::Crossfade
       || isMultiTrackFileTransition(m_currentTrack, track)) {
        return false;
    }

    if(m_preparedCrossfadeTransition.active) {
        return m_preparedCrossfadeTransition.sourceGeneration == generation
            && sameTrackIdentity(track, m_preparedCrossfadeTransition.targetTrack);
    }

    if(!m_preparedNext || !sameTrackIdentity(track, m_preparedNext->track) || !m_preparedNext->format.isValid()
       || !m_preparedNext->preparedStream) {
        return false;
    }

    const auto eligibility = evaluateAutoTransitionEligibility(track, false);
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
        prefillActiveStream(std::min(fadeOutDurationMs, m_bufferLengthMs));
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
                qCWarning(ENGINE) << "Prepared crossfade arm rejected: insufficient prebuffer for boundary overlap:"
                                  << "trackId=" << track.id() << "generation=" << generation
                                  << "preparedBufferedMs=" << preparedBufferedMs << "requiredBufferedMs="
                                  << saturatingAddWindow(saturatingAddWindow(requestedOverlapMs, transitionDelayMs),
                                                         safetyWindowMs)
                                  << "overlapWindowMs=" << requestedOverlapMs
                                  << "transitionDelayMs=" << transitionDelayMs;
                return false;
            }

            qCWarning(ENGINE) << "Prepared crossfade arm overlap clamped due prebuffer shortfall:"
                              << "trackId=" << track.id() << "generation=" << generation
                              << "preparedBufferedMs=" << preparedBufferedMs
                              << "requestedOverlapMs=" << requestedOverlapMs << "clampedOverlapMs=" << clampedOverlapMs
                              << "transitionDelayMs=" << transitionDelayMs;
            fadeOutDurationMs = std::min(fadeOutDurationMs, clampedOverlapMs);
            fadeInDurationMs  = std::min(fadeInDurationMs, clampedOverlapMs);
            overlapDurationMs = std::min(std::max(0, fadeOutDurationMs), std::max(0, fadeInDurationMs));
        }
    }

    const StreamId activeStreamId   = currentStream->id();
    const bool hasEarlyAutoTailFade = m_autoCrossfadeTailFadeActive && m_autoCrossfadeTailFadeGeneration == generation
                                   && m_autoCrossfadeTailFadeStreamId == activeStreamId
                                   && activeStreamId != InvalidStreamId;
    const bool skipFadeOutStart = hasEarlyAutoTailFade && overlapDurationMs <= 0;

    const uint64_t overlapWindowMs = static_cast<uint64_t>(std::max(0, overlapDurationMs));
    const uint64_t requiredBufferedMs
        = saturatingAddWindow(saturatingAddWindow(overlapWindowMs, transitionDelayMs), safetyWindowMs);

    if(preparedBufferedMs < requiredBufferedMs) {
        qCWarning(ENGINE) << "Prepared crossfade arm rejected: insufficient prebuffer for boundary overlap:"
                          << "trackId=" << track.id() << "generation=" << generation
                          << "preparedBufferedMs=" << preparedBufferedMs << "requiredBufferedMs=" << requiredBufferedMs
                          << "overlapWindowMs=" << overlapWindowMs << "transitionDelayMs=" << transitionDelayMs;
        return false;
    }

    const uint64_t currentRelativePosMs
        = relativeTrackPositionMs(currentStream->positionMs(), m_streamToTrackOriginMs, m_currentTrack.offset());
    const uint64_t remainingToBoundaryMs
        = (m_currentTrack.duration() > currentRelativePosMs) ? (m_currentTrack.duration() - currentRelativePosMs) : 0;

    clearAutoCrossfadeTailFadeState();

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

    m_preparedCrossfadeTransition.active            = true;
    m_preparedCrossfadeTransition.targetTrack       = track;
    m_preparedCrossfadeTransition.sourceGeneration  = generation;
    m_preparedCrossfadeTransition.streamId          = transitionResult.streamId;
    m_preparedCrossfadeTransition.boundaryLeadMs    = remainingToBoundaryMs;
    m_preparedCrossfadeTransition.bufferedAtArmMs   = preparedBufferedMs;
    m_preparedCrossfadeTransition.boundarySignalled = false;
    clearPreparedGaplessTransition();

    // If the current stream has already ended by the time we arm the prepared
    // transition, there is no live source stream left to provide an overlap
    // anchor. Treat the arm as an immediate boundary handoff so the prepared
    // stream is committed before its prebuffer drains.
    if(!armedWithOrphan) {
        m_preparedCrossfadeTransition.boundarySignalled = true;
        emit trackBoundaryReached(m_currentTrack, m_trackGeneration);
    }

    qCDebug(ENGINE) << "Prepared crossfade transition armed:" << "trackId=" << track.id() << "generation=" << generation
                    << "streamId=" << transitionResult.streamId << "preparedBufferedMs=" << preparedBufferedMs
                    << "preparedPosMs=" << m_preparedNext->preparedDecodePositionMs
                    << "remainingToBoundaryMs=" << remainingToBoundaryMs << "transitionDelayMs=" << transitionDelayMs;
    return true;
}

bool AudioEngine::commitPreparedCrossfadeTransition(const Track& track)
{
    disarmStalePreparedTransitions(track, m_trackGeneration);

    if(!track.isValid() || !m_preparedCrossfadeTransition.active
       || !sameTrackIdentity(track, m_preparedCrossfadeTransition.targetTrack)) {
        return false;
    }

    const StreamId preparedStreamId = m_preparedCrossfadeTransition.streamId;

    if(!initDecoder(track, true)) {
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

    setCurrentTrackContext(track);
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
    clearPreparedCrossfadeTransition();
    clearPreparedGaplessTransition();
    return true;
}

bool AudioEngine::armPreparedGaplessTransition(const Track& track, uint64_t generation)
{
    disarmStalePreparedTransitions(track, generation);

    if(!track.isValid() || generation != m_trackGeneration || !hasPlaybackState(Engine::PlaybackState::Playing)
       || m_transitions.isSeekInProgress()) {
        return false;
    }

    if(m_transitions.autoTransitionMode() != AutoTransitionMode::Gapless
       || isMultiTrackFileTransition(m_currentTrack, track)) {
        return false;
    }

    if(m_preparedGaplessTransition.active) {
        return m_preparedGaplessTransition.sourceGeneration == generation
            && sameTrackIdentity(track, m_preparedGaplessTransition.targetTrack);
    }

    if(!m_preparedNext || !sameTrackIdentity(track, m_preparedNext->track) || !m_preparedNext->format.isValid()
       || !m_preparedNext->preparedStream) {
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
    m_preparedGaplessTransition.sourceGeneration = generation;
    m_preparedGaplessTransition.streamId         = transitionResult.streamId;
    clearPreparedCrossfadeTransition();
    return true;
}

bool AudioEngine::commitPreparedGaplessTransition(const Track& track)
{
    disarmStalePreparedTransitions(track, m_trackGeneration);

    if(!track.isValid() || !m_preparedGaplessTransition.active
       || !sameTrackIdentity(track, m_preparedGaplessTransition.targetTrack)) {
        return false;
    }

    const StreamId preparedStreamId = m_preparedGaplessTransition.streamId;

    if(!initDecoder(track, true)) {
        clearPreparedGaplessTransition();
        return false;
    }

    const auto preparedStream = m_decoder.activeStream();
    if(!preparedStream || preparedStream->id() != preparedStreamId) {
        qCWarning(ENGINE) << "Prepared gapless commit could not adopt armed stream:"
                          << "expectedStreamId=" << preparedStreamId
                          << "actualStreamId=" << (preparedStream ? preparedStream->id() : InvalidStreamId);
        clearPreparedGaplessTransition();
        return false;
    }

    m_format = m_decoder.format();
    m_decoder.startDecoding();
    clearAutoCrossfadeTailFadeState();

    setCurrentTrackContext(track);
    setStreamToTrackOriginForTrack(track);
    clearTrackEndLatch();
    m_transitions.clearTrackEnding();

    m_positionCoordinator.setGaplessHold(preparedStreamId);

    updateTrackStatus(Engine::TrackStatus::Buffered);
    clearPreparedGaplessTransition();
    return true;
}

void AudioEngine::play()
{
    if(m_trackStatus.load(std::memory_order_relaxed) == Engine::TrackStatus::Loading) {
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
            loadTrack(m_currentTrack, false);
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
    m_pipeline.resetOutput();
    clearPendingAnalysisData();
    m_audioClock.stop();
    updatePlaybackState(Engine::PlaybackState::Paused);
}

void AudioEngine::stop()
{
    m_transitions.cancelPendingSeek();

    if(auto stream = m_decoder.activeStream()) {
        if(stream->endOfInput() && stream->bufferEmpty()) {
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
    clearAutoCrossfadeTailFadeState();
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
            loadTrack(m_currentTrack, false);
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

void AudioEngine::setOutputDevice(const QString& device)
{
    if(device == m_outputController.outputDevice()) {
        return;
    }

    qCDebug(ENGINE) << "Changing output device to:" << device;

    m_outputController.setOutputDevice(device);
    m_pipeline.setOutputDevice(device);
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

    const bool outputFormatChanged = inputFormat.isValid() && m_pipeline.hasOutput() && desiredOutput.isValid()
                                  && currentOutput.isValid()
                                  && !Audio::outputFormatsMatch(currentOutput, desiredOutput);

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
        if(event->type() == AudioEngine::engineTaskEventType()
           || event->type() == AudioEngine::pipelineWakeEventType()) {
            return true;
        }
    }

    if(event->type() == AudioEngine::engineTaskEventType()) {
        m_engineTaskQueue.drain();
        return true;
    }

    if(event->type() == AudioEngine::pipelineWakeEventType()) {
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
