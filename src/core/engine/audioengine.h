/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#pragma once

#include "fycore_export.h"

#include <core/engine/audioformat.h>
#include <core/engine/audiooutput.h>
#include <core/engine/enginedefs.h>
#include <core/track.h>
#include <utils/lockfreeringbuffer.h>

#include "audioclock.h"
#include "control/enginetaskqueue.h"
#include "control/playbackphase.h"
#include "control/positioncoordinator.h"
#include "control/trackloadplanner.h"
#include "control/transitionorchestrator.h"
#include "decode/decodingcontroller.h"
#include "decode/nexttrackpreparer.h"
#include "output/fadecontroller.h"
#include "output/outputcontroller.h"
#include "output/postprocessor/replaygainprocessor.h"
#include "pipeline/audiopipeline.h"

#include <QEvent>

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>

class QTimerEvent;

namespace Fooyin {
class AudioLoader;
class AudioAnalysisBus;
class DspRegistry;
class SettingsManager;
class AudioEngine;
namespace Testing {
class AudioEngineTestAccessor;
}

/*!
 * Core playback engine orchestrating decode, pipeline, transitions and timing.
 *
 * `AudioEngine` runs on a dedicated engine thread. Public slots are invoked
 * from controller via queued calls. Internally it coordinates:
 * - decode lifecycle and buffering,
 * - stream/pipeline transitions (crossfade, seek fades, gapless handoff),
 * - output init/reinit and device selection,
 * - playback state and position publication.
 */
class FYCORE_EXPORT AudioEngine : public QObject
{
    Q_OBJECT

public:
    using TrackEndingResult = TransitionOrchestrator::TrackEndingResult;

    struct AutoTransitionEligibility
    {
        bool crossfadeMix{false};
        bool gaplessHandoff{false};
        int fadeOutDurationMs{0};
        int fadeInDurationMs{0};
    };

    enum class PhaseChangeReason : uint8_t
    {
        Unknown = 0,
        PlaybackStatePlaying,
        PlaybackStatePaused,
        PlaybackStateStopped,
        PlaybackStateError,
        TrackStatusLoading,
        ManualChangeFadeQueued,
        AutoTransitionCrossfadeActive,
        AutoTransitionPlayingNoOrphan,
        SeekCrossfadeActive,
        SeekSimpleActive,
        SeekRestorePlaying,
        SeekRestorePaused,
        SeekRestoreStopped,
        CrossfadeCleanupResumePlaying,
        TransportPauseFadeQueued,
        TransportStopFadeQueued,
    };
    Q_ENUM(PhaseChangeReason)

    enum class DrainFillPrepareDiagnosticReason : uint8_t
    {
        None = 0,
        NoUpcomingCandidate,
        CandidateMatchesCurrent,
        MultiTrackFileTransition,
        AutoTransitionDisabled,
        WaitingForEndOfInput,
        PreparedCrossfadeAlreadyActive,
        PreparedGaplessAlreadyActive,
        AlreadyBuffered,
        AlreadyRequested,
        Enqueued,
    };
    Q_ENUM(DrainFillPrepareDiagnosticReason)

    AudioEngine(std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings, DspRegistry* dspRegistry,
                QObject* parent = nullptr);
    ~AudioEngine() override;

    AudioEngine(const AudioEngine&)            = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    [[nodiscard]] Engine::PlaybackState playbackState() const;
    [[nodiscard]] Engine::TrackStatus trackStatus() const;
    //! Last published source position in milliseconds.
    [[nodiscard]] uint64_t position() const;

    //! Replace active DSP chain configuration.
    void setDspChain(const Engine::DspChains& chain);

public slots:
    void loadTrack(const Fooyin::Engine::PlaybackItem& item, bool manualChange = false);
    void setTrackEndAutoTransitionEnabled(bool enabled);
    void setUpcomingTrackCandidate(const Fooyin::Engine::PlaybackItem& item);
    //! Schedule/prepare candidate next track for seamless transition.
    void prepareNextTrack(const Fooyin::Engine::PlaybackItem& item, uint64_t requestId = 0);
    //! Stage a prepared crossfade stream in the pipeline without committing UI track context.
    [[nodiscard]] bool armPreparedCrossfadeTransition(const Fooyin::Engine::PlaybackItem& item, uint64_t generation);
    //! Commit a previously armed prepared crossfade transition after UI track context changes.
    [[nodiscard]] bool commitPreparedCrossfadeTransition(const Fooyin::Engine::PlaybackItem& item);
    [[nodiscard]] static bool shouldEnableTimelineTransitionHints(const Fooyin::Track& track,
                                                                  Fooyin::AudioDecoder::PlaybackHints playbackHints);
    //! Stage a prepared gapless stream in the pipeline without committing UI track context.
    [[nodiscard]] bool armPreparedGaplessTransition(const Fooyin::Engine::PlaybackItem& item, uint64_t generation);
    //! Commit a previously armed prepared gapless transition after UI track context changes.
    [[nodiscard]] bool commitPreparedGaplessTransition(const Fooyin::Engine::PlaybackItem& item);

    void play();
    void pause();
    void stop();
    void stopImmediate();
    void restorePosition(uint64_t positionMs, bool pause);

    void seek(uint64_t positionMs);
    void seekWithRequest(uint64_t positionMs, uint64_t requestId);
    void setVolume(double volume);

    void setAudioOutput(const Fooyin::OutputCreator& output, const QString& device);
    void applyOutputProfile(const Fooyin::OutputCreator& output, const QString& device, SampleFormat bitdepth,
                            bool dither, const Fooyin::Engine::DspChains& chain);
    void setOutputDevice(const QString& device);
    void setAnalysisDataSubscriptions(Fooyin::Engine::AnalysisDataTypes subscriptions);
    void updateLiveDspSettings(const Fooyin::Engine::LiveDspSettingsUpdate& update);

signals:
    void stateChanged(Fooyin::Engine::PlaybackState state);
    void trackStatusContextChanged(Fooyin::Engine::TrackStatus status, const Fooyin::Track& track, uint64_t generation);
    void positionChanged(uint64_t positionMs);
    void positionChangedWithContext(uint64_t positionMs, uint64_t trackGeneration, uint64_t timelineEpoch,
                                    uint64_t seekRequestId);
    void positionContextChanged(uint64_t trackGeneration, uint64_t timelineEpoch, uint64_t seekRequestId);

    void seekPositionApplied(uint64_t positionMs, uint64_t requestId);
    void bitrateChanged(int bitrate);

    void levelReady(const Fooyin::LevelFrame& frame);

    void trackAboutToFinish(const Fooyin::Track& track, uint64_t generation);
    void trackReadyToSwitch(const Fooyin::Track& track, uint64_t generation);
    void trackBoundaryReached(const Fooyin::Track& track, uint64_t generation, uint64_t remainingOutputMs,
                              bool engineOwnsTransition);
    void nextTrackReadiness(const Fooyin::Engine::PlaybackItem& item, bool ready, uint64_t requestId);

    void finished();

    void deviceError(const QString& error);
    void trackChanged(const Fooyin::Track& track);
    void trackCommitted(const Fooyin::Engine::TrackCommitContext& context);

protected:
    bool event(QEvent* event) override;
    void timerEvent(QTimerEvent* event) override;

private:
    friend class Testing::AudioEngineTestAccessor;

    void beginShutdown();

    bool ensureCrossfadePrepared(const Engine::PlaybackItem& item, bool isManualChange);
    bool handleManualChangeFade(const Engine::PlaybackItem& item);
    bool startTrackCrossfade(const Engine::PlaybackItem& item, bool isManualChange);
    void performSeek(uint64_t positionMs, uint64_t requestId);
    void checkPendingSeek();
    void startSeekCrossfade(uint64_t positionMs, int fadeOutDurationMs, int fadeInDurationMs, uint64_t requestId);
    void performSimpleSeek(uint64_t positionMs, uint64_t requestId = 0, int prefillTargetMs = 0);
    void prefillActiveStream(int targetMs);
    [[nodiscard]] int adjustedCrossfadePrefillMs(int requestedMs) const;
    void cancelAllFades();
    void cancelFadesForReinit();
    void reinitOutputForCurrentFormat();
    void applyFadeResult(const FadeController::FadeResult& result);
    void beginAudiblePauseCompletion(uint64_t transportTransitionId);
    void maybeCompletePendingAudiblePause(uint64_t serial);
    void finalisePausedState();
    void clearPendingAudiblePause();
    [[nodiscard]] bool cancelPendingAudiblePause();
    void handlePipelineFadeEvent(const AudioPipeline::FadeEvent& event);
    void syncDecoderTrackMetadata();
    void publishBitrate(int bitrate);
    void syncDecoderBitrate();
    void publishPosition(uint64_t sourcePositionMs, uint64_t outputDelayMs, double delayToSourceScale,
                         AudioClock::UpdateMode mode, bool emitNow = true);
    void updatePositionContext(uint64_t timelineEpoch);
    void maybeBeginAutoCrossfadeTailFadeOut(const AudioStreamPtr& stream, uint64_t relativePosMs);
    void maybeBeginAutoBoundaryFadeOut(uint64_t relativePosMs);
    void applyAutoBoundaryFadeIn(bool allowFadeInOnly = false);
    void clearAutoCrossfadeTailFadeState();
    void clearAutoBoundaryFadeState(bool restoreOutput = false);
    void clearTrackEndAutoTransitions();
    void invalidatePreparedAutoTransitionsForSeek();
    void clearAutoAdvanceState();

    void rememberAutoTransitionMode(AutoTransitionMode mode);
    [[nodiscard]] AutoTransitionMode effectiveAutoTransitionMode() const;
    [[nodiscard]] AutoTransitionMode configuredTrackEndAutoTransitionMode() const;
    [[nodiscard]] AutoTransitionMode configuredTrackEndAutoTransitionMode(const Track& track) const;

    [[nodiscard]] uint64_t scaledPlaybackDelayMs() const;
    [[nodiscard]] uint64_t scaledTransitionDelayMs() const;
    [[nodiscard]] uint64_t boundaryCrossfadeOverlapMs() const;
    [[nodiscard]] uint64_t transitionReserveMs() const;
    [[nodiscard]] uint64_t aggressivePreparedPrefillMs() const;

    [[nodiscard]] Engine::PlaybackItem autoAdvanceTargetTrack() const;
    void maybeLogDrainFillPrepareGate(const AudioStreamPtr& stream, const TrackEndingResult& result);
    void logDrainFillPrepareDiagnostic(DrainFillPrepareDiagnosticReason reason, const AudioStreamPtr& stream,
                                       uint64_t prefillTargetMs = 0);
    void maybePrepareUpcomingTrackForDrainFill(const AudioStreamPtr& stream);
    void noteOverlapStartAnchor();
    void noteOverlapMidpointAnchor();
    void noteBoundaryAnchor();
    [[nodiscard]] Engine::TrackCommitContext makeTrackCommitContext(Engine::TransitionMode mode,
                                                                    uint64_t audibleDelayMs = 0) const;
    void clearPendingAudibleTrackCommit();
    void finaliseTrackCommitCleanup();
    void tryAutoAdvanceCommit();
    void finaliseTrackCommit(Engine::TransitionMode mode, uint64_t audibleDelayMs = 0);
    bool finaliseTrackCommitWhenAudible(Engine::TransitionMode mode, StreamId streamId);
    void maybeEmitPendingAudibleTrackCommit(StreamId audibleOutputStreamId);
    void handleTrackEndingSignals(const AudioStreamPtr& stream, uint64_t trackEndingPosMs,
                                  uint64_t publishedAudiblePosMs, uint64_t boundaryAudiblePosMs,
                                  bool preparedCrossfadeArmed, bool boundaryFallbackReached,
                                  bool preparedGaplessRendered);

    void updatePosition();
    void handleTimerTick(int timerId);
    void clearPendingAnalysisData();
    void onLevelFrameReady(const LevelFrame& frame);
    void dispatchPendingLevelFrames();
    void schedulePipelineWakeDrainTask();
    void handlePipelineWakeSignals(const AudioPipeline::PendingSignals& pendingSignals);

    [[nodiscard]] uint64_t beginTransportTransition();
    void clearTransportTransition();
    [[nodiscard]] uint64_t nextTransitionId();
    void refreshVbrUpdateTimer();
    void setupSettings();
    void reconfigureActiveStreamBuffering(uint64_t positionMs);
    void updatePlaybackState(Engine::PlaybackState state);
    void updateTrackStatus(Engine::TrackStatus status, bool flushDspOnEnd = true);
    void setPhase(Playback::Phase phase, PhaseChangeReason reason);
    bool hasPlaybackState(Engine::PlaybackState state) const;
    bool signalTrackEndOnce(bool flushDspOnEnd);
    void clearTrackEndLatch();
    TrackEndingResult checkTrackEnding(const AudioStreamPtr& stream, uint64_t relativePosMs, uint64_t audiblePosMs);
    [[nodiscard]] uint64_t preferredPreparedPrefillMs() const;
    [[nodiscard]] Engine::PlaybackItem currentPlaybackItem() const;
    [[nodiscard]] Engine::PlaybackItem upcomingTrackCandidateItem() const;
    [[nodiscard]] Engine::PlaybackItem preparedCrossfadeTargetItem() const;
    [[nodiscard]] Engine::PlaybackItem preparedGaplessTargetItem() const;
    std::optional<AutoTransitionEligibility>
    evaluateAutoTransitionEligibility(const Track& track, bool isManualChange, bool requireTransitionReady = true,
                                      const char** rejectionReason = nullptr) const;
    bool isAutoTransitionEligible(const Track& track) const;
    void setCurrentTrackContext(const Engine::PlaybackItem& item);
    void setStreamToTrackOriginForTrack(const Track& track);
    [[nodiscard]] bool setStreamToTrackOriginForSegmentSwitch(const Track& track, uint64_t streamPosMs);
    [[nodiscard]] AudioStreamPtr currentTrackTimingStream() const;
    void scheduleGaplessBoundaryStallDiagnostic(uint64_t generation, StreamId currentStreamId,
                                                StreamId preparedStreamId);
    void logGaplessBoundaryDiagnostic(const char* reason, StreamId triggerCurrentStreamId,
                                      StreamId triggerPreparedStreamId) const;

    bool initDecoder(const Engine::PlaybackItem& item, bool allowPreparedStream);
    bool setupNewTrackStream(const Track& track, bool applyPendingSeek);
    [[nodiscard]] bool stagePreparedGaplessDecoder(const Engine::PlaybackItem& item);
    [[nodiscard]] bool registerAndSwitchStream(const AudioStreamPtr& stream, const char* failureMessage);
    void cleanupDecoderActiveStreamFromPipeline(bool removeFromMixer);
    void clearPreparedNextTrack();
    void clearPreparedCrossfadeTransition();
    void clearPreparedGaplessTransition();
    void discardPreparedGaplessTransition(bool preserveDecoderOwnedStream = true);
    void disarmStalePreparedTransitions(const Track& contextTrack, uint64_t contextGeneration);
    void clearPreparedNextTrackAndCancelPendingJobs();
    [[nodiscard]] bool prepareNextTrackImmediate(const Engine::PlaybackItem& item, uint64_t prefillTargetMs = 0);
    void cancelPendingPrepareJobs();
    void enqueuePrepareNextTrack(const Engine::PlaybackItem& item, uint64_t requestId, uint64_t prefillTargetMs);
    void applyPreparedNextTrackResult(uint64_t jobToken, uint64_t requestId, const Engine::PlaybackItem& item,
                                      NextTrackPreparationState prepared);
    void cleanupActiveStream();
    void cleanupOrphanedStream();
    void resetStreamToTrackOrigin();
    [[nodiscard]] TrackLoadContext buildLoadContext(const Track& track, bool manualChange) const;
    [[nodiscard]] bool executeSegmentSwitchLoad(const Engine::PlaybackItem& item, bool preserveTransportFade);
    [[nodiscard]] bool executeManualFadeDeferredLoad(const Engine::PlaybackItem& item);
    [[nodiscard]] bool executeAutoTransitionLoad(const Engine::PlaybackItem& item);
    void executeFullReinitLoad(const Engine::PlaybackItem& item, bool manualChange, bool preserveTransportFade);
    void executeLoadPlan(const Engine::PlaybackItem& item, bool manualChange, const TrackLoadContext& context,
                         const LoadPlan& plan);

    SettingsManager* m_settings;
    DspRegistry* m_dspRegistry;
    std::shared_ptr<AudioLoader> m_audioLoader;
    EngineTaskQueue m_engineTaskQueue;

    std::atomic<Engine::PlaybackState> m_playbackState;
    std::atomic<Engine::TrackStatus> m_trackStatus;
    Playback::Phase m_phase;

    Track m_currentTrack;
    uint64_t m_currentTrackItemId{0};
    Track m_lastEndedTrack;
    uint64_t m_trackGeneration;
    uint64_t m_streamToTrackOriginMs;
    uint64_t m_nextTransitionId;
    uint64_t m_pendingManualTrackItemId{0};
    AudioFormat m_format;

    DecodingController m_decoder;

    std::optional<NextTrackPreparationState> m_preparedNext;

    struct PreparedCrossfadeTransition
    {
        bool active{false};
        Track targetTrack;
        uint64_t targetItemId{0};
        uint64_t sourceGeneration{0};
        StreamId streamId{InvalidStreamId};
        uint64_t boundaryLeadMs{0};
        uint64_t overlapMidpointLeadMs{0};
        uint64_t bufferedAtArmMs{0};
        bool boundarySignalled{false};
    };
    PreparedCrossfadeTransition m_preparedCrossfadeTransition;

    struct PreparedGaplessTransition
    {
        bool active{false};
        Track targetTrack;
        uint64_t targetItemId{0};
        uint64_t sourceGeneration{0};
        StreamId streamId{InvalidStreamId};
        bool boundaryFadeMode{false};
        bool decoderAdopted{false};
        AudioStreamPtr preCommitTimingStream;
    };
    PreparedGaplessTransition m_preparedGaplessTransition;

    LockFreeRingBuffer<LevelFrame> m_levelFrameMailbox;
    std::atomic<bool> m_levelFrameDispatchQueued;
    std::atomic<bool> m_pipelineWakeTaskQueued;

    std::unique_ptr<AudioAnalysisBus> m_analysisBus;
    AudioPipeline m_pipeline;
    FadeController m_fadeController;
    OutputController m_outputController;
    TransitionOrchestrator m_transitions;
    ReplayGainProcessor::SharedSettingsPtr m_replayGainSharedSettings;

    double m_volume;
    int m_playbackBufferLengthMs;
    double m_decodeLowWatermarkRatio;
    double m_decodeHighWatermarkRatio;
    bool m_fadingEnabled;
    bool m_crossfadeEnabled;
    bool m_gaplessEnabled;
    bool m_trackEndAutoTransitionEnabled{true};
    Engine::CrossfadeSwitchPolicy m_crossfadeSwitchPolicy;
    AudioDecoder::PlaybackHints m_decoderPlaybackHints{AudioDecoder::NoHints};
    Engine::FadingValues m_fadingValues;
    Engine::CrossfadingValues m_crossfadingValues;

    AudioClock m_audioClock;
    int m_lastReportedBitrate;
    int m_vbrUpdateIntervalMs;
    std::chrono::steady_clock::time_point m_lastVbrUpdateAt;
    int m_vbrUpdateTimerId;
    NextTrackPrepareWorker m_nextTrackPrepareWorker;
    PositionCoordinator m_positionCoordinator;

    uint64_t m_lastAppliedSeekRequestId;
    uint64_t m_positionContextTrackGeneration;
    uint64_t m_positionContextTimelineEpoch;
    uint64_t m_positionContextSeekRequestId;
    bool m_autoCrossfadeTailFadeActive;
    StreamId m_autoCrossfadeTailFadeStreamId;
    uint64_t m_autoCrossfadeTailFadeGeneration;
    bool m_autoBoundaryFadeActive;
    uint64_t m_autoBoundaryFadeGeneration;

    Track m_upcomingTrackCandidate;
    uint64_t m_upcomingTrackCandidateItemId{0};

    struct AutoAdvanceState
    {
        uint64_t generation{0};
        AutoTransitionMode mode{AutoTransitionMode::None};
        bool overlapStartAnchorSeen{false};
        bool overlapMidpointAnchorSeen{false};
        bool boundaryAnchorSeen{false};
        bool boundaryPendingUntilAudible{false};
        bool drainPrepareRequested{false};
    };
    AutoAdvanceState m_autoAdvanceState;

    struct DrainFillPrepareDiagnosticState
    {
        uint64_t generation{0};
        DrainFillPrepareDiagnosticReason reason{DrainFillPrepareDiagnosticReason::None};
        int candidateTrackId{0};
    };
    DrainFillPrepareDiagnosticState m_drainFillPrepareDiagnostic;

    struct PendingAudibleTrackCommit
    {
        bool active{false};
        Engine::TrackCommitContext context;
        StreamId streamId{InvalidStreamId};
    };
    PendingAudibleTrackCommit m_pendingAudibleTrackCommit;

    struct PendingAudiblePause
    {
        bool active{false};
        uint64_t transitionId{0};
        uint64_t serial{0};
        std::chrono::steady_clock::time_point startedAt;
        int watchdogMs{0};
    };
    PendingAudiblePause m_pendingAudiblePause;
    uint64_t m_nextPendingAudiblePauseSerial{0};
};
} // namespace Fooyin
