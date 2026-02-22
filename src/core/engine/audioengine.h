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

#include "control/enginetaskqueue.h"
#include "control/playbackphase.h"
#include "control/trackloadplanner.h"
#include "control/transitionorchestrator.h"
#include "decode/decodingcontroller.h"
#include "decode/nexttrackpreparer.h"
#include "output/fadecontroller.h"
#include "output/outputcontroller.h"
#include "output/postprocessor/replaygainprocessor.h"
#include "pipeline/audiopipeline.h"
#include "timing/positiontracker.h"

#include <QEvent>
#include <QObject>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>

class QTimerEvent;

namespace Fooyin {
class AudioLoader;
class AudioAnalysisBus;
class DspRegistry;
class SettingsManager;
class AudioEngine;

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
        AudioFormat nextMixFormat;
    };

    enum class PositionEmitMode : uint8_t
    {
        Deferred = 0,
        Now,
    };

    AudioEngine(std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings, DspRegistry* dspRegistry,
                QObject* parent = nullptr);
    ~AudioEngine() override;

    AudioEngine(const AudioEngine&)            = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    [[nodiscard]] Engine::PlaybackState playbackState() const;
    [[nodiscard]] Engine::TrackStatus trackStatus() const;
    [[nodiscard]] uint64_t position() const;

    void setDspChain(const Engine::DspChains& chain);

public slots:
    void loadTrack(const Fooyin::Track& track, bool manualChange = false);
    void prepareNextTrack(const Fooyin::Track& track, uint64_t requestId = 0);

    void play();
    void pause();
    void stop();
    void stopImmediate();

    void seek(uint64_t positionMs);
    void setVolume(double volume);

    void setAudioOutput(const Fooyin::OutputCreator& output, const QString& device);
    void setOutputDevice(const QString& device);
    void setAnalysisDataSubscriptions(Fooyin::Engine::AnalysisDataTypes subscriptions);

signals:
    void stateChanged(Fooyin::Engine::PlaybackState state);
    void trackStatusContextChanged(Fooyin::Engine::TrackStatus status, const Fooyin::Track& track, uint64_t generation);
    void positionChanged(uint64_t positionMs);
    void bitrateChanged(int bitrate);

    void levelReady(const Fooyin::LevelFrame& frame);

    void trackAboutToFinish(const Fooyin::Track& track, uint64_t generation);
    void trackReadyToSwitch(const Fooyin::Track& track, uint64_t generation);
    void nextTrackReadiness(const Fooyin::Track& track, bool ready, uint64_t requestId);

    void finished();

    void deviceError(const QString& error);
    void trackChanged(const Fooyin::Track& track);

protected:
    bool event(QEvent* event) override;
    void timerEvent(QTimerEvent* event) override;

private:
    [[nodiscard]] static QEvent::Type engineTaskEventType();
    [[nodiscard]] static QEvent::Type pipelineWakeEventType();

    void beginShutdown();

    bool ensureCrossfadePrepared(const Track& track, bool isManualChange);
    bool handleManualChangeFade(const Track& track);
    bool startTrackCrossfade(const Track& track, bool isManualChange);
    void performSeek(uint64_t positionMs);
    void checkPendingSeek();
    void startSeekCrossfade(uint64_t positionMs, int fadeOutDurationMs, int fadeInDurationMs);
    void performSimpleSeek(uint64_t positionMs);
    void prefillActiveStream(int targetMs);
    void prefillActiveStreamBurst(int targetMs, int maxChunks);
    [[nodiscard]] int adjustedCrossfadePrefillMs(int requestedMs) const;
    void cancelAllFades();
    void cancelFadesForReinit();
    void reinitOutputForCurrentFormat();
    void applyFadeResult(const FadeController::FadeResult& result);
    void handlePipelineFadeEvent(const AudioPipeline::FadeEvent& event);
    void syncDecoderTrackMetadata();
    void publishBitrate(int bitrate);
    void syncDecoderBitrate();
    void publishPosition(uint64_t sourcePositionMs, uint64_t outputDelayMs, double delayToSourceScale,
                         AudioClock::UpdateMode mode, PositionEmitMode emitMode = PositionEmitMode::Now);
    void maybeBeginAutoCrossfadeTailFadeOut(const AudioStreamPtr& stream, uint64_t relativePosMs);
    void clearAutoCrossfadeTailFadeState();
    void armSeekPositionGuard(uint64_t seekPositionMs);
    void clearSeekPositionGuard();
    void updatePosition();
    void handleTimerTick(int timerId);
    void clearPendingAnalysisData();
    void onLevelFrameReady(const LevelFrame& frame);
    void dispatchPendingLevelFrames();
    void handlePipelineWakeSignals(const AudioPipeline::PendingSignals& pendingSignals);

    [[nodiscard]] uint64_t beginTransportTransition();
    [[nodiscard]] uint64_t beginCrossfadeTransition();
    void clearTransportTransition();
    [[nodiscard]] uint64_t nextTransitionId();
    void setupSettings();
    void updatePlaybackState(Engine::PlaybackState state);
    void updateTrackStatus(Engine::TrackStatus status, bool flushDspOnEnd = true);
    void setPhase(PlaybackPhase phase);
    bool hasPlaybackState(Engine::PlaybackState state) const;
    bool signalTrackEndOnce(bool flushDspOnEnd);
    void clearTrackEndLatch();
    TrackEndingResult checkTrackEnding(const AudioStreamPtr& stream, uint64_t relativePosMs);
    std::optional<AutoTransitionEligibility> evaluateAutoTransitionEligibility(const Track& track,
                                                                               bool isManualChange) const;
    bool isAutoTransitionEligible(const Track& track) const;
    void setCurrentTrackContext(const Track& track);
    void setStreamToTrackOriginForTrack(const Track& track);
    [[nodiscard]] bool setStreamToTrackOriginForSegmentSwitch(const Track& track, uint64_t streamPosMs);

    bool initDecoder(const Track& track, bool allowPreparedStream);
    bool setupNewTrackStream(const Track& track, bool applyPendingSeek);
    [[nodiscard]] bool registerAndSwitchStream(const AudioStreamPtr& stream, const char* failureMessage);
    void cleanupDecoderActiveStreamFromPipeline(bool removeFromMixer);
    void clearPreparedNextTrack();
    void clearPreparedNextTrackAndCancelPendingJobs();
    [[nodiscard]] bool prepareNextTrackImmediate(const Track& track);
    void cancelPendingPrepareJobs();
    void enqueuePrepareNextTrack(const Track& track, uint64_t requestId);
    void applyPreparedNextTrackResult(uint64_t jobToken, uint64_t requestId, const Track& track,
                                      NextTrackPreparationState prepared);
    void cleanupActiveStream();
    void cleanupOrphanedStream();
    void resetStreamToTrackOrigin();
    [[nodiscard]] TrackLoadContext buildLoadContext(const Track& track, bool manualChange) const;
    [[nodiscard]] bool executeSegmentSwitchLoad(const Track& track, bool preserveTransportFade);
    [[nodiscard]] bool executeManualFadeDeferredLoad(const Track& track);
    [[nodiscard]] bool executeAutoTransitionLoad(const Track& track);
    void executeFullReinitLoad(const Track& track, bool manualChange, bool preserveTransportFade);
    void executeLoadPlan(const Track& track, bool manualChange, const TrackLoadContext& context, const LoadPlan& plan);

    SettingsManager* m_settings;
    DspRegistry* m_dspRegistry;
    std::shared_ptr<AudioLoader> m_audioLoader;
    EngineTaskQueue m_engineTaskQueue;

    std::atomic<Engine::PlaybackState> m_playbackState;
    std::atomic<Engine::TrackStatus> m_trackStatus;
    PlaybackPhase m_phase;

    Track m_currentTrack;
    Track m_lastEndedTrack;
    uint64_t m_trackGeneration;
    uint64_t m_streamToTrackOriginMs;
    uint64_t m_nextTransitionId;
    AudioFormat m_format;

    DecodingController m_decoder;

    std::optional<NextTrackPreparationState> m_preparedNext;

    std::atomic<bool> m_levelFrameDispatchQueued;
    std::mutex m_levelFrameDispatchMutex;
    std::optional<LevelFrame> m_pendingLevelFrame;

    std::unique_ptr<AudioAnalysisBus> m_analysisBus;
    AudioPipeline m_pipeline;
    FadeController m_fadeController;
    OutputController m_outputController;
    TransitionOrchestrator m_transitions;
    ReplayGainProcessor::SharedSettingsPtr m_replayGainSharedSettings;

    double m_volume;
    int m_bufferLengthMs;
    bool m_fadingEnabled;
    bool m_crossfadeEnabled;
    bool m_gaplessEnabled;
    Engine::FadingValues m_fadingValues;
    Engine::CrossfadingValues m_crossfadingValues;

    PositionTracker m_positionTracker;
    int m_lastReportedBitrate;
    NextTrackPrepareWorker m_nextTrackPrepareWorker;

    bool m_lastSourcePositionValid;
    uint64_t m_lastSourcePositionMs;
    bool m_autoCrossfadeTailFadeActive;
    StreamId m_autoCrossfadeTailFadeStreamId;
    uint64_t m_autoCrossfadeTailFadeGeneration;

    bool m_seekPositionGuardActive;
    uint64_t m_seekPositionGuardMs;
    std::chrono::steady_clock::time_point m_seekPositionGuardUntil;
};
} // namespace Fooyin
