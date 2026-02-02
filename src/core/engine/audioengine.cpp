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
#include <utils/settings/settingsmanager.h>

#include <QCoreApplication>
#include <QEvent>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QThread>

#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <utility>

Q_LOGGING_CATEGORY(ENGINE, "fy.engine")

namespace {
constexpr auto BasePrefillDecodeFrames              = 4096;
constexpr auto MaxPrefillDecodeFrames               = 262144;
constexpr int PrefillChunkDurationMs                = 10;
constexpr int PositionSyncTimerCoalesceKey          = 0;
constexpr uint64_t GaplessPrepareLeadMs             = 300;
constexpr int MaxCrossfadePrefillMs                 = 1000;
constexpr int GaplessHandoffPrefillMs               = 250;
constexpr uint64_t MaxContinuousPositionJumpMs      = 750;
constexpr uint64_t MaxBackwardDriftToleranceMs      = 50;
constexpr auto SeekPositionGuardWindow              = std::chrono::milliseconds{750};
constexpr uint64_t SeekPositionGuardRejectDeltaMs   = 1000;
constexpr uint64_t SeekPositionGuardConvergeDeltaMs = 300;

std::pair<int, int> decodeWatermarksForBufferMs(int bufferLengthMs)
{
    const int clampedBufferMs = std::max(200, bufferLengthMs);
    const int lowMs           = std::clamp(clampedBufferMs / 2, 100, clampedBufferMs);
    const int highMs          = std::clamp((clampedBufferMs * 3) / 4, lowMs, clampedBufferMs);
    return {lowMs, highMs};
}

size_t prefillFramesPerChunk(int sampleRate)
{
    if(sampleRate <= 0) {
        return BasePrefillDecodeFrames;
    }

    const uint64_t desiredFrames
        = (static_cast<uint64_t>(sampleRate) * static_cast<uint64_t>(PrefillChunkDurationMs)) / 1000ULL;

    return static_cast<size_t>(std::clamp<uint64_t>(desiredFrames, static_cast<uint64_t>(BasePrefillDecodeFrames),
                                                    static_cast<uint64_t>(MaxPrefillDecodeFrames)));
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
    , m_phase{PlaybackPhase::Stopped}
    , m_trackGeneration{0}
    , m_streamToTrackOriginMs{0}
    , m_nextTransitionId{1}
    , m_decoder{this}
    , m_levelFrameDispatchQueued{false}
    , m_fadeController{&m_pipeline}
    , m_outputController{this, &m_pipeline}
    , m_volume{m_settings->value<Settings::Core::OutputVolume>()}
    , m_bufferLengthMs{m_settings->value<Settings::Core::BufferLength>()}
    , m_fadingEnabled{m_settings->value<Settings::Core::Internal::EngineFading>()}
    , m_crossfadeEnabled{m_settings->value<Settings::Core::Internal::EngineCrossfading>()}
    , m_gaplessEnabled{m_settings->value<Settings::Core::GaplessPlayback>()}
    , m_positionTracker{this}
    , m_lastReportedBitrate{0}
    , m_lastSourcePositionValid{false}
    , m_lastSourcePositionMs{0}
    , m_autoCrossfadeTailFadeActive{false}
    , m_autoCrossfadeTailFadeStreamId{InvalidStreamId}
    , m_autoCrossfadeTailFadeGeneration{0}
    , m_seekPositionGuardActive{false}
    , m_seekPositionGuardMs{0}
{
    setupSettings();

    QObject::connect(&m_positionTracker, &PositionTracker::positionChanged, this, &AudioEngine::positionChanged);
    QObject::connect(&m_positionTracker, &PositionTracker::requestSyncPosition, this, [this]() {
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

bool AudioEngine::ensureCrossfadePrepared(const Track& track, const bool isManualChange)
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
    const bool manualCrossfadeConfigured
        = m_crossfadeEnabled && (m_crossfadingValues.manualChange.out > 0 || m_crossfadingValues.manualChange.in > 0);

    if(manualCrossfadeConfigured) {
        return startTrackCrossfade(track, true);
    }

    const uint64_t transitionId = nextTransitionId();
    if(m_fadeController.handleManualChangeFade(track, m_crossfadeEnabled, m_crossfadingValues,
                                               hasPlaybackState(Engine::PlaybackState::Playing), m_volume,
                                               transitionId)) {
        setPhase(PlaybackPhase::FadingToManualChange);
        return true;
    }

    return false;
}

bool AudioEngine::startTrackCrossfade(const Track& track, const bool isManualChange)
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

    const bool crossfadeMix   = eligibility->crossfadeMix;
    const bool gaplessHandoff = eligibility->gaplessHandoff;

    const uint64_t activeGeneration = m_trackGeneration;
    const StreamId activeStreamId   = (m_decoder.activeStream() ? m_decoder.activeStream()->id() : InvalidStreamId);
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

    const bool crossfadeInProgress = crossfadeMix && m_pipeline.hasOrphanStream();
    if(crossfadeInProgress) {
        setPhase(PlaybackPhase::TrackCrossfading);
    }
    else if(hasPlaybackState(Engine::PlaybackState::Playing)) {
        setPhase(PlaybackPhase::Playing);
    }

    if(wasDecoding || hasPlaybackState(Engine::PlaybackState::Playing)) {
        m_decoder.ensureDecodeTimerRunning();
    }

    updateTrackStatus(Engine::TrackStatus::Loaded);
    updateTrackStatus(Engine::TrackStatus::Buffered);
    if(isManualChange) {
        clearSeekPositionGuard();
        publishPosition(0, 0, 1.0, AudioClock::UpdateMode::Discontinuity, PositionEmitMode::Now);
    }

    return true;
}

void AudioEngine::performSeek(uint64_t positionMs)
{
    if(!m_decoder.isValid()) {
        return;
    }

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
        .autoCrossfadeOutMs     = m_crossfadingValues.autoChange.out,
        .seekFadeOutMs          = m_crossfadingValues.seek.out,
        .seekFadeInMs           = m_crossfadingValues.seek.in,
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
        performSimpleSeek(positionMs);
        return;
    }

    if(!currentStream) {
        performSimpleSeek(positionMs);
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
            performSimpleSeek(positionMs);
            return;
        }
    }

    startSeekCrossfade(positionMs, plan.fadeOutDurationMs, plan.fadeInDurationMs);
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
        startSeekCrossfade(params->seekPositionMs, params->fadeOutDurationMs, params->fadeInDurationMs);
    }
}

void AudioEngine::startSeekCrossfade(uint64_t positionMs, int fadeOutDurationMs, int fadeInDurationMs)
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
        setPhase(PlaybackPhase::SeekCrossfading);
    }
    else {
        setPhase(PlaybackPhase::Seeking);
    }
    m_transitions.clearTrackEnding();

    if(wasDecoding || hasPlaybackState(Engine::PlaybackState::Playing)) {
        m_decoder.ensureDecodeTimerRunning();
    }

    m_transitions.setSeekInProgress(false);
    const uint64_t pipelineDelayMs  = m_pipeline.playbackDelayMs();
    const double delayToSourceScale = m_pipeline.playbackDelayToTrackScale();
    publishPosition(positionMs, pipelineDelayMs, delayToSourceScale, AudioClock::UpdateMode::Discontinuity,
                    PositionEmitMode::Now);
    armSeekPositionGuard(positionMs);
    if(!m_pipeline.hasOrphanStream() && hasPlaybackState(Engine::PlaybackState::Playing)) {
        setPhase(PlaybackPhase::Playing);
    }
}

void AudioEngine::performSimpleSeek(uint64_t positionMs)
{
    m_decoder.clearDecodeReserve();

    const bool wasPlaying  = hasPlaybackState(Engine::PlaybackState::Playing);
    const bool wasDecoding = m_decoder.isDecodeTimerActive();
    setPhase(PlaybackPhase::Seeking);

    const auto restorePhaseFromTransport = [this]() {
        if(hasPlaybackState(Engine::PlaybackState::Playing)) {
            setPhase(PlaybackPhase::Playing);
            return;
        }
        if(hasPlaybackState(Engine::PlaybackState::Paused)) {
            setPhase(PlaybackPhase::Paused);
            return;
        }
        if(hasPlaybackState(Engine::PlaybackState::Stopped)) {
            setPhase(PlaybackPhase::Stopped);
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

    m_pipeline.resetStreamForSeek(stream->id());

    const uint64_t seekPosMs = absoluteTrackPositionMs(positionMs, m_currentTrack.offset());
    m_decoder.seek(seekPosMs);
    m_decoder.syncStreamPosition();

    if(!m_decoder.isDecoding()) {
        m_decoder.start();
    }

    prefillActiveStream(m_bufferLengthMs / 4);

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
    publishPosition(positionMs, pipelineDelayMs, delayToSourceScale, AudioClock::UpdateMode::Discontinuity,
                    PositionEmitMode::Now);
    armSeekPositionGuard(positionMs);
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

void AudioEngine::prefillActiveStreamBurst(int targetMs, int maxChunks)
{
    if(maxChunks <= 0) {
        return;
    }

    int sampleRate = m_format.sampleRate();
    if(const auto stream = m_decoder.activeStream()) {
        sampleRate = stream->sampleRate();
    }

    const int chunksDecoded = m_decoder.prefillActiveStreamMs(targetMs, maxChunks, prefillFramesPerChunk(sampleRate));
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
        if(auto stream = m_decoder.activeStream()) {
            m_pipeline.sendStreamCommand(stream->id(), AudioStream::Command::Pause);
        }
        m_pipeline.pause();
        m_pipeline.resetOutput();
        clearPendingAnalysisData();
        m_positionTracker.stop();
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

void AudioEngine::publishPosition(uint64_t sourcePositionMs, uint64_t outputDelayMs, const double delayToSourceScale,
                                  AudioClock::UpdateMode mode, const PositionEmitMode emitMode)
{
    m_positionTracker.applyPosition(sourcePositionMs, outputDelayMs, delayToSourceScale, mode,
                                    emitMode == PositionEmitMode::Now);
    if(mode == AudioClock::UpdateMode::Discontinuity) {
        m_lastSourcePositionMs    = sourcePositionMs;
        m_lastSourcePositionValid = true;
    }
}

void AudioEngine::maybeBeginAutoCrossfadeTailFadeOut(const AudioStreamPtr& stream, const uint64_t relativePosMs)
{
    if(!stream || !hasPlaybackState(Engine::PlaybackState::Playing)) {
        return;
    }

    if(m_transitions.autoTransitionMode() != AutoTransitionMode::Crossfade) {
        return;
    }

    const int fadeOutMs    = std::max(0, m_crossfadingValues.autoChange.out);
    const int fadeInMs     = std::max(0, m_crossfadingValues.autoChange.in);
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
    const int remainingWindowMs     = static_cast<int>(
        std::min<uint64_t>(remainingTrackMs, static_cast<uint64_t>(std::numeric_limits<int>::max())));

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

void AudioEngine::armSeekPositionGuard(const uint64_t seekPositionMs)
{
    m_seekPositionGuardActive = true;
    m_seekPositionGuardMs     = seekPositionMs;
    m_seekPositionGuardUntil  = std::chrono::steady_clock::now() + SeekPositionGuardWindow;
}

void AudioEngine::clearSeekPositionGuard()
{
    m_seekPositionGuardActive = false;
}

void AudioEngine::updatePosition()
{
    if(m_transitions.isSeekInProgress()) {
        return;
    }

    // During crossfade, report position from active (fade-in) stream
    auto stream = m_decoder.activeStream();
    if(!stream) {
        return;
    }

    const uint64_t decodeLowWatermarkMs = static_cast<uint64_t>(std::max(1, m_decoder.lowWatermarkMs()));
    if(hasPlaybackState(Engine::PlaybackState::Playing) && !m_decoder.isDecodeTimerActive()
       && stream->state() != AudioStream::State::Pending && stream->state() != AudioStream::State::Paused
       && !stream->endOfInput() && stream->bufferedDurationMs() < decodeLowWatermarkMs) {
        // Safety rearm: recover from rare command-ordering races where decode
        // demand notification is missed after seek/transition.
        m_decoder.ensureDecodeTimerRunning();
    }

    const uint64_t pipelineDelayMs   = m_pipeline.playbackDelayMs();
    const double delayToSourceScale  = m_pipeline.playbackDelayToTrackScale();
    const auto pipelineStatus        = m_pipeline.currentStatus();
    const uint64_t decodeSourcePosMs = stream->positionMs();
    uint64_t sourcePosMs             = decodeSourcePosMs;

    if(pipelineStatus.positionIsMapped && !m_pipeline.hasOrphanStream()) {
        sourcePosMs = pipelineStatus.position;

        // During handoff, mapped timeline can briefly report stale source positions
        // from the previous stream. Reject implausible mapped/decode divergence.
        const double clampedScale = std::clamp(delayToSourceScale, 0.05, 8.0);
        const auto expectedGapMs
            = static_cast<uint64_t>(std::llround(static_cast<double>(pipelineDelayMs) * clampedScale));
        const uint64_t maxAllowedGapMs = expectedGapMs + std::max<uint64_t>(1000, MaxContinuousPositionJumpMs);
        const uint64_t gapMs
            = sourcePosMs >= decodeSourcePosMs ? (sourcePosMs - decodeSourcePosMs) : (decodeSourcePosMs - sourcePosMs);
        if(gapMs > maxAllowedGapMs) {
            sourcePosMs = decodeSourcePosMs;
        }
    }

    uint64_t relativePosMs = relativeTrackPositionMs(sourcePosMs, m_streamToTrackOriginMs, m_currentTrack.offset());

    if(m_seekPositionGuardActive) {
        const uint64_t guardPosMs = m_seekPositionGuardMs;
        const uint64_t deltaMs
            = relativePosMs >= guardPosMs ? (relativePosMs - guardPosMs) : (guardPosMs - relativePosMs);
        const auto now = std::chrono::steady_clock::now();
        if(deltaMs <= SeekPositionGuardConvergeDeltaMs || now >= m_seekPositionGuardUntil) {
            clearSeekPositionGuard();
        }
        else if(deltaMs >= SeekPositionGuardRejectDeltaMs) {
            relativePosMs = guardPosMs;
        }
    }

    auto updateMode = AudioClock::UpdateMode::Continuous;
    auto emitMode   = PositionEmitMode::Deferred;

    if(m_lastSourcePositionValid) {
        const bool backwardDrift = relativePosMs < m_lastSourcePositionMs
                                && (m_lastSourcePositionMs - relativePosMs) > MaxBackwardDriftToleranceMs;
        const bool forwardJump = relativePosMs > m_lastSourcePositionMs
                              && (relativePosMs - m_lastSourcePositionMs) > MaxContinuousPositionJumpMs;
        if(backwardDrift || forwardJump) {
            updateMode = AudioClock::UpdateMode::Discontinuity;
            emitMode   = PositionEmitMode::Now;
        }
    }

    publishPosition(relativePosMs, pipelineDelayMs, delayToSourceScale, updateMode, emitMode);
    m_lastSourcePositionMs    = relativePosMs;
    m_lastSourcePositionValid = true;

    const uint64_t streamRelativePosMs
        = relativeTrackPositionMs(stream->positionMs(), m_streamToTrackOriginMs, m_currentTrack.offset());
    const auto result = checkTrackEnding(stream, streamRelativePosMs);
    if(result.aboutToFinish) {
        maybeBeginAutoCrossfadeTailFadeOut(stream, streamRelativePosMs);
        emit trackAboutToFinish(m_currentTrack, m_trackGeneration);
    }
    if(result.readyToSwitch) {
        emit trackReadyToSwitch(m_currentTrack, m_trackGeneration);
    }
    if(result.endReached) {
        // Same-file logical segment transitions can reach "track end" before
        // the underlying stream is actually drained. In that case, avoid
        // flushing DSP state.
        const bool shouldFlushDspOnEnd = stream->endOfInput() && stream->bufferEmpty();
        if(!shouldFlushDspOnEnd) {
            qCDebug(ENGINE) << "Logical track end detected before stream drain; skipping EndOfTrack DSP flush";
        }
        signalTrackEndOnce(shouldFlushDspOnEnd);
    }
}

void AudioEngine::handleTimerTick(const int timerId)
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

    {
        const std::scoped_lock lock{m_levelFrameDispatchMutex};
        m_pendingLevelFrame.reset();
    }
    m_levelFrameDispatchQueued.store(false, std::memory_order_release);
}

void AudioEngine::onLevelFrameReady(const LevelFrame& frame)
{
    {
        const std::scoped_lock lock{m_levelFrameDispatchMutex};
        m_pendingLevelFrame = frame;
    }

    if(m_levelFrameDispatchQueued.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    m_engineTaskQueue.enqueue(EngineTaskType::Worker, [this]() { dispatchPendingLevelFrames(); });
}

void AudioEngine::dispatchPendingLevelFrames()
{
    while(true) {
        std::optional<LevelFrame> pendingFrame;
        {
            const std::scoped_lock lock{m_levelFrameDispatchMutex};
            if(m_pendingLevelFrame.has_value()) {
                pendingFrame = m_pendingLevelFrame;
                m_pendingLevelFrame.reset();
            }
            else {
                m_levelFrameDispatchQueued.store(false, std::memory_order_release);
            }
        }

        if(!pendingFrame.has_value()) {
            break;
        }

        emit levelReady(*pendingFrame);
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

uint64_t AudioEngine::beginCrossfadeTransition()
{
    return nextTransitionId();
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
        const auto [lowWatermarkMs, highWatermarkMs] = decodeWatermarksForBufferMs(m_bufferLengthMs);
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

    m_replayGainSharedSettings           = ReplayGainProcessor::makeSharedSettings();
    const auto refreshReplayGainSettings = [this]() {
        ReplayGainProcessor::refreshSharedSettings(*m_settings, *m_replayGainSharedSettings);
    };
    refreshReplayGainSettings();

    m_settings->subscribe<Settings::Core::PlayMode>(this, refreshReplayGainSettings);
    m_settings->subscribe<Settings::Core::RGMode>(this, refreshReplayGainSettings);
    m_settings->subscribe<Settings::Core::RGType>(this, refreshReplayGainSettings);
    m_settings->subscribe<Settings::Core::RGPreAmp>(this, refreshReplayGainSettings);
    m_settings->subscribe<Settings::Core::NonRGPreAmp>(this, refreshReplayGainSettings);

    m_pipeline.setOutputBitdepth(static_cast<SampleFormat>(m_settings->value<Settings::Core::OutputBitDepth>()));
    m_pipeline.setDither(m_settings->value<Settings::Core::OutputDither>());
    m_settings->subscribe<Settings::Core::OutputBitDepth>(this, [this](const int bitdepth) {
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
    m_settings->subscribe<Settings::Core::OutputDither>(this,
                                                        [this](const bool dither) { m_pipeline.setDither(dither); });
}

void AudioEngine::updatePlaybackState(Engine::PlaybackState state)
{
    const auto prevState = m_playbackState.exchange(state, std::memory_order_release);
    if(prevState != state) {
        switch(state) {
            case Engine::PlaybackState::Playing:
                m_positionTracker.setPlaying(m_positionTracker.position());
                if(!isCrossfading(m_phase) && !isFadingTransport(m_phase) && m_phase != PlaybackPhase::Seeking) {
                    setPhase(PlaybackPhase::Playing);
                }
                break;
            case Engine::PlaybackState::Paused:
                m_positionTracker.setPaused();
                setPhase(PlaybackPhase::Paused);
                break;
            case Engine::PlaybackState::Stopped:
                setPhase(PlaybackPhase::Stopped);
                m_positionTracker.setStopped();
                break;
            case Engine::PlaybackState::Error:
                setPhase(PlaybackPhase::Error);
                m_positionTracker.setStopped();
                break;
        }
        emit stateChanged(state);
    }
}

void AudioEngine::updateTrackStatus(Engine::TrackStatus status, bool flushDspOnEnd)
{
    const auto prevStatus = m_trackStatus.exchange(status, std::memory_order_release);
    if(prevStatus != status) {
        emit trackStatusContextChanged(status, m_currentTrack, m_trackGeneration);
    }

    if(status == Engine::TrackStatus::Loading) {
        setPhase(PlaybackPhase::Loading);
    }

    if(status == Engine::TrackStatus::End && flushDspOnEnd) {
        m_pipeline.flushDsp(DspNode::FlushMode::EndOfTrack);
    }
}

void AudioEngine::setPhase(PlaybackPhase phase)
{
    m_phase = phase;
}

bool AudioEngine::hasPlaybackState(Engine::PlaybackState state) const
{
    return m_playbackState.load(std::memory_order_acquire) == state;
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

    const uint64_t bufferedMs    = stream->bufferedDurationMs();
    const uint64_t outputDelayMs = m_pipeline.playbackDelayMs();
    uint64_t remainingOutputMs{bufferedMs};

    if(remainingOutputMs > std::numeric_limits<uint64_t>::max() - outputDelayMs) {
        remainingOutputMs = std::numeric_limits<uint64_t>::max();
    }
    else {
        remainingOutputMs += outputDelayMs;
    }

    PlaybackTransitionCoordinator::TrackEndingInput input;

    input.positionMs        = relativePosMs;
    input.durationMs        = m_currentTrack.duration();
    input.remainingOutputMs = remainingOutputMs;
    input.endOfInput        = stream->endOfInput();
    input.bufferEmpty       = stream->bufferEmpty();
    input.autoCrossfadeEnabled
        = m_crossfadeEnabled && (m_crossfadingValues.autoChange.in > 0 || m_crossfadingValues.autoChange.out > 0);
    input.gaplessEnabled         = m_gaplessEnabled;
    input.autoFadeOutMs          = m_crossfadingValues.autoChange.out;
    input.autoFadeInMs           = m_crossfadingValues.autoChange.in;
    input.gaplessPrepareWindowMs = GaplessPrepareLeadMs;

    return m_transitions.evaluateTrackEnding(input);
}

std::optional<AudioEngine::AutoTransitionEligibility>
AudioEngine::evaluateAutoTransitionEligibility(const Track& track, bool isManualChange) const
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

    const int baseFadeOutMs
        = !isManualChange ? m_crossfadingValues.autoChange.out : m_crossfadingValues.manualChange.out;
    const int baseFadeInMs = !isManualChange ? m_crossfadingValues.autoChange.in : m_crossfadingValues.manualChange.in;

    const bool crossfadeMix = m_crossfadeEnabled && (baseFadeInMs > 0 || baseFadeOutMs > 0);
    const bool gaplessHandoff
        = !isManualChange && m_gaplessEnabled
       && (!m_crossfadeEnabled || (m_crossfadingValues.autoChange.in <= 0 && m_crossfadingValues.autoChange.out <= 0));

    if(!crossfadeMix && !gaplessHandoff) {
        return {};
    }

    if(!isManualChange) {
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
        const int remainingWindowMs = static_cast<int>(
            std::min<uint64_t>(remainingTrackMs, static_cast<uint64_t>(std::numeric_limits<int>::max())));
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
    return evaluateAutoTransitionEligibility(track, false).has_value();
}

void AudioEngine::setCurrentTrackContext(const Track& track)
{
    m_currentTrack = track;
    ++m_trackGeneration;
    publishBitrate(track.bitrate());
}

void AudioEngine::setStreamToTrackOriginForTrack(const Track& track)
{
    m_streamToTrackOriginMs = track.offset();
}

bool AudioEngine::setStreamToTrackOriginForSegmentSwitch(const Track& track, const uint64_t streamPosMs)
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
        int targetMs = m_bufferLengthMs / 4;

        if(!m_format.isValid()) {
            return targetMs;
        }

        const AudioFormat perTrackFormat = m_pipeline.predictPerTrackFormat(m_format);
        if(perTrackFormat.isValid() && perTrackFormat.sampleRate() > 0 && m_format.sampleRate() > 0
           && perTrackFormat.sampleRate() != m_format.sampleRate()) {
            targetMs = std::max(targetMs, m_decoder.highWatermarkMs());
        }

        return targetMs;
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

        performSimpleSeek(*pendingSeek);

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

void AudioEngine::cleanupDecoderActiveStreamFromPipeline(const bool removeFromMixer)
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
    context.audioLoader    = m_audioLoader;
    context.currentTrack   = m_currentTrack;
    context.playbackState  = m_playbackState.load(std::memory_order_acquire);
    context.bufferLengthMs = m_bufferLengthMs;

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
    request.requestId              = requestId;
    request.track                  = track;
    request.context.audioLoader    = m_audioLoader;
    request.context.currentTrack   = m_currentTrack;
    request.context.playbackState  = m_playbackState.load(std::memory_order_acquire);
    request.context.bufferLengthMs = m_bufferLengthMs;

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
            setPhase(PlaybackPhase::Playing);
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

TrackLoadContext AudioEngine::buildLoadContext(const Track& track, const bool manualChange) const
{
    return TrackLoadContext{
        .manualChange                = manualChange,
        .isPlaying                   = hasPlaybackState(Engine::PlaybackState::Playing),
        .decoderValid                = m_decoder.isValid(),
        .hasPendingTransportFade     = m_fadeController.hasPendingFade(),
        .isContiguousSameFileSegment = isContiguousSameFileSegment(m_currentTrack, track),
    };
}

bool AudioEngine::executeSegmentSwitchLoad(const Track& track, const bool preserveTransportFade)
{
    clearAutoCrossfadeTailFadeState();

    uint64_t streamPosMs{0};
    if(const auto stream = m_decoder.activeStream()) {
        streamPosMs = stream->positionMs();
    }

    if(setStreamToTrackOriginForSegmentSwitch(track, streamPosMs)) {
        qCDebug(ENGINE) << "Switching contiguous segment on active stream:" << track.filenameExt();
        clearPreparedNextTrack();
        setCurrentTrackContext(track);
        clearTrackEndLatch();
        m_transitions.clearTrackEnding();

        if(!preserveTransportFade) {
            m_fadeController.invalidateActiveFade();
        }

        if(const auto stream = m_decoder.activeStream()) {
            stream->setTrack(track);
        }

        if(m_trackStatus.load(std::memory_order_acquire) == Engine::TrackStatus::End) {
            updateTrackStatus(Engine::TrackStatus::Buffered);
        }

        clearSeekPositionGuard();
        publishPosition(0, 0, 1.0, AudioClock::UpdateMode::Discontinuity, PositionEmitMode::Now);
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

void AudioEngine::executeFullReinitLoad(const Track& track, const bool manualChange, const bool preserveTransportFade)
{
    qCDebug(ENGINE) << "Loading track:" << track.filenameExt();

    clearAutoCrossfadeTailFadeState();
    clearPendingAnalysisData();
    m_decoder.stopDecoding();
    cleanupActiveStream();

    const auto prevState = m_playbackState.load(std::memory_order_acquire);

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
        clearSeekPositionGuard();
        publishPosition(0, 0, 1.0, AudioClock::UpdateMode::Discontinuity, PositionEmitMode::Now);
    }
}

void AudioEngine::executeLoadPlan(const Track& track, const bool manualChange, const TrackLoadContext& context,
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

void AudioEngine::loadTrack(const Track& track, const bool manualChange)
{
    const TrackLoadContext loadContext = buildLoadContext(track, manualChange);
    const LoadPlan loadPlan            = planTrackLoad(loadContext);
    executeLoadPlan(track, manualChange, loadContext, loadPlan);
}

void AudioEngine::prepareNextTrack(const Track& track, const uint64_t requestId)
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

void AudioEngine::play()
{
    if(m_trackStatus.load(std::memory_order_acquire) == Engine::TrackStatus::Loading) {
        return;
    }

    const auto prevState  = m_playbackState.load(std::memory_order_acquire);
    const auto playAction = reducePlaybackIntent(
        {
            .playbackState  = prevState,
            .fadeState      = m_fadeController.state(),
            .fadingEnabled  = m_fadingEnabled,
            .pauseFadeOutMs = m_fadingValues.pause.out,
            .stopFadeOutMs  = m_fadingValues.stop.out,
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
        m_positionTracker.start();

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
    m_positionTracker.start();

    updatePlaybackState(Engine::PlaybackState::Playing);
    updateTrackStatus(Engine::TrackStatus::Buffered);
}

void AudioEngine::pause()
{
    const auto state       = m_playbackState.load(std::memory_order_acquire);
    const auto pauseAction = reducePlaybackIntent(
        {
            .playbackState  = state,
            .fadeState      = m_fadeController.state(),
            .fadingEnabled  = m_fadingEnabled,
            .pauseFadeOutMs = m_fadingValues.pause.out,
            .stopFadeOutMs  = m_fadingValues.stop.out,
        },
        PlaybackIntent::Pause);

    if(pauseAction == PlaybackAction::NoOp) {
        return;
    }

    if(pauseAction == PlaybackAction::BeginFade) {
        const uint64_t transitionId = beginTransportTransition();
        if(m_fadeController.beginPauseFade(m_fadingEnabled, m_fadingValues, m_volume, transitionId)) {
            setPhase(PlaybackPhase::FadingToPause);
            return;
        }
        clearTransportTransition();
    }

    if(pauseAction != PlaybackAction::Immediate && pauseAction != PlaybackAction::BeginFade) {
        return;
    }

    if(auto stream = m_decoder.activeStream()) {
        m_pipeline.sendStreamCommand(stream->id(), AudioStream::Command::Pause);
    }

    m_pipeline.pause();
    m_pipeline.resetOutput();
    clearPendingAnalysisData();
    m_positionTracker.stop();
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

    const auto state      = m_playbackState.load(std::memory_order_acquire);
    const auto stopAction = reducePlaybackIntent(
        {
            .playbackState  = state,
            .fadeState      = m_fadeController.state(),
            .fadingEnabled  = m_fadingEnabled,
            .pauseFadeOutMs = m_fadingValues.pause.out,
            .stopFadeOutMs  = m_fadingValues.stop.out,
        },
        PlaybackIntent::Stop);

    if(stopAction == PlaybackAction::NoOp) {
        return;
    }

    if(stopAction == PlaybackAction::BeginFade) {
        const uint64_t transitionId = beginTransportTransition();
        if(m_fadeController.beginStopFade(m_fadingEnabled, m_fadingValues, m_volume, state, transitionId)) {
            setPhase(PlaybackPhase::FadingToStop);
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
    clearSeekPositionGuard();

    m_fadeController.invalidateActiveFade();
    cancelAllFades();
    clearTransportTransition();

    m_positionTracker.stop();

    m_decoder.stopDecoding();
    m_pipeline.stopPlayback();
    cleanupDecoderActiveStreamFromPipeline(false);
    m_pipeline.cleanupOrphanImmediate();

    clearPreparedNextTrackAndCancelPendingJobs();
    m_decoder.reset();
    resetStreamToTrackOrigin();
    clearTrackEndLatch();
    clearPendingAnalysisData();

    updatePlaybackState(Engine::PlaybackState::Stopped);
    updateTrackStatus(Engine::TrackStatus::NoTrack);
    publishBitrate(0);

    publishPosition(0, 0, 1.0, AudioClock::UpdateMode::Discontinuity, PositionEmitMode::Now);
    emit finished();
}

void AudioEngine::seek(const uint64_t positionMs)
{
    const bool shouldStartPlayback = hasPlaybackState(Engine::PlaybackState::Stopped);

    if(!m_decoder.isValid()) {
        const int trackId = m_currentTrack.isValid() ? m_currentTrack.id() : -1;
        m_transitions.queueInitialSeek(positionMs, trackId);

        // Stopped seek is expected to begin playback from the requested position.
        // Queue the pending seek first so setupNewTrackStream applies it.
        if(shouldStartPlayback && m_currentTrack.isValid()) {
            loadTrack(m_currentTrack, false);
            play();
        }
        return;
    }

    performSeek(positionMs);
    if(shouldStartPlayback) {
        play();
    }
}

void AudioEngine::setVolume(const double volume)
{
    m_volume = std::clamp(volume, 0.0, 1.0);
    m_pipeline.setOutputVolume(m_volume);
}

void AudioEngine::setAudioOutput(const OutputCreator& output, const QString& device)
{
    qCDebug(ENGINE) << "Setting audio output, device:" << device;

    const bool wasPlaying = m_playbackState.load(std::memory_order_acquire) == Engine::PlaybackState::Playing;

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

Engine::PlaybackState AudioEngine::playbackState() const
{
    return m_playbackState.load(std::memory_order_acquire);
}

Engine::TrackStatus AudioEngine::trackStatus() const
{
    return m_trackStatus.load(std::memory_order_acquire);
}

uint64_t AudioEngine::position() const
{
    return m_positionTracker.position();
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
            m_engineTaskQueue.enqueue(EngineTaskType::PipelineWake, [engine = this]() {
                engine->handlePipelineWakeSignals(engine->m_pipeline.drainPendingSignals());
            });
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
