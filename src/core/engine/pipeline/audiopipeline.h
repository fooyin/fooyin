/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "audiomixer.h"
#include "audiopipelinefader.h"
#include "core/engine/dsp/dspchain.h"
#include "core/engine/lockfreeringbuffer.h"
#include "core/engine/output/outputfader.h"
#include "core/engine/pipeline/audiostream.h"
#include "orphanstreamtracker.h"
#include "pipelinesignalmailbox.h"

#include <core/engine/audiobuffer.h>
#include <core/engine/audiooutput.h>
#include <core/engine/enginedefs.h>
#include <utils/compatutils.h>

#include <QDebug>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

class QString;
class QObject;

namespace Fooyin {
class DspRegistry;
class AudioAnalysisBus;

enum class PipelinePlaybackState : uint8_t
{
    Stopped = 0,
    Playing,
    Paused
};

struct PipelineStatus
{
    uint64_t position{0};         // Primary-stream playback position in milliseconds
    bool positionIsMapped{false}; // True when position is rendered source timeline from post-master mapping
    PipelinePlaybackState state{PipelinePlaybackState::Stopped};
    bool bufferUnderrun{false}; // True when output requested frames but mixer had no audio ready
};

/*!
 * Audio output pipeline executed on a dedicated worker thread.
 *
 * AudioPipeline owns and coordinates:
 * - `StreamRegistry` (stream lookup by id),
 * - `AudioMixer` (stream mixing + per-track processing),
 * - master `DspChain` (post-mix DSP),
 * - `OutputFader` (fade envelope),
 * - `AudioOutput` backend (device I/O).
 *
 * Per audio cycle, once playing/output-initialised, the audio thread:
 * 1. drains pending command queue,
 * 2. obtains free output frames and handles any pending partial write tail,
 * 3. asks `AudioMixer` for mixed frames,
 * 4. runs master DSP chain + output fader,
 * 5. coalesces chunks, converts to output format, and writes to device,
 * 6. updates latency/position snapshots and pending signal state.
 *
 * AudioPipeline does not decode files itself; decode code feeds `AudioStream`
 * buffers from other threads.
 *
 * Threading model:
 * - Public API is thread-safe.
 * - Calls are executed to the audio thread through a command queue.
 * - `enqueueAndWait` methods are synchronous barriers; async methods return
 *   after queueing only.
 * - Mixer/DSP/output state is mutated only on the audio thread.
 * - Command bodies execute on the audio thread and must be bounded and
 *   non-blocking.
 * - Command queue is bounded; enqueue fails when full (command is dropped).
 */
class FYCORE_EXPORT AudioPipeline : public AudioPipelineFader
{
public:
    struct FadeEvent
    {
        OutputFader::CompletionType type{OutputFader::CompletionType::FadeInComplete};
        uint64_t token{0};
    };

    struct PendingSignals
    {
        bool needsData{false};
        bool fadeEventsPending{false};
    };

    AudioPipeline();
    ~AudioPipeline();

    AudioPipeline(const AudioPipeline&)            = delete;
    AudioPipeline& operator=(const AudioPipeline&) = delete;

    // ========================================================================
    // Control (thread-safe, typically called from engine thread)
    // ========================================================================

    //! Start the audio worker thread.
    void start();
    //! Stop the audio worker thread and clear runtime state.
    void stop();

    //! Set active output backend.
    //! Safe while running: swap/uninit/init happen on audio thread via command barrier.
    void setOutput(std::unique_ptr<AudioOutput> output);
    //! Set the DSP registry for creating per-track DSP instances.
    //! Registry pointer is published atomically for audio-thread reads.
    void setDspRegistry(DspRegistry* registry);

    //! Register a per-track processor factory.
    void addTrackProcessorFactory(AudioMixer::TrackProcessorFactory factory);

    //! Initialize output path for input (pre-master-DSP) format.
    bool init(const AudioFormat& format);

    //! Reconfigure input and DSP/mixer formats without reinitializing backend.
    AudioFormat applyInputFormat(const AudioFormat& format);

    //! Uninitialize output backend and clear output-format state.
    void uninit();

    //! Register a stream and return its id for command/routing operations.
    StreamId registerStream(AudioStreamPtr stream);
    //! Unregister a stream id from registry lookup.
    void unregisterStream(StreamId id);

    // ========================================================================
    // Commands (thread-safe, called from main thread)
    // ========================================================================

    //! Start output processing loop.
    void play();
    //! Pause output processing loop.
    void pause();
    //! Stop playback state and clear stream activity.
    void stopPlayback();
    //! Reset backend output buffers where supported.
    void resetOutput();
    //! Flush master DSP chain state according to mode.
    void flushDsp(DspNode::FlushMode mode);

    //! Set output volume scalar in range [0.0, 1.0].
    void setOutputVolume(double volume);
    //! Route output backend to a specific device id/name.
    void setOutputDevice(const QString& device);
    //! Set preferred backend sample format and return predicted output format.
    AudioFormat setOutputBitdepth(SampleFormat bitdepth);
    //! Enable/disable TPDF dithering for float->S16 output conversion.
    void setDither(bool enabled);
    //! Replace master/per-track DSP definitions and return predicted output format.
    AudioFormat setDspChain(std::vector<DspNodePtr> masterNodes, const Engine::DspChain& perTrackDefs,
                            const AudioFormat& inputFormat);
    //! Predict per-track processing format from upstream input format.
    //! Uses command barrier when running to read audio-thread-owned DSP state.
    AudioFormat predictPerTrackFormat(const AudioFormat& inputFormat) const;
    //! Predict backend output format from upstream input format.
    //! Uses command barrier when running to read audio-thread-owned DSP state.
    AudioFormat predictOutputFormat(const AudioFormat& inputFormat) const;
    //! Queue a command for a specific stream id.
    void sendStreamCommand(StreamId id, AudioStream::Command command, int param = 0);
    //! Reset a stream for seek discontinuity.
    void resetStreamForSeek(StreamId id);

    //! Add a stream by ID (stream must already be registered in StreamRegistry).
    void addStream(StreamId id);
    //! Remove a stream by ID
    void removeStream(StreamId id);
    //! Switch to a stream by ID (clears all others)
    void switchToStream(StreamId id);

    enum class TransitionType : uint8_t
    {
        Crossfade = 0,
        Gapless,
        SeekCrossfade,
    };

    struct TransitionRequest
    {
        TransitionType type{TransitionType::Crossfade};
        AudioStreamPtr stream;
        int fadeOutMs{0};
        int fadeInMs{0};
        bool skipFadeOutStart{false};
    };

    struct TransitionResult
    {
        StreamId streamId{InvalidStreamId};
        bool success{false};
    };

    //! Atomically clean up any existing orphan stream and execute the transition.
    TransitionResult executeTransition(const TransitionRequest& request);

    //! True when a fading-out orphan stream is being managed by the pipeline.
    [[nodiscard]] bool hasOrphanStream() const;
    //! Force immediate orphan cleanup.
    void cleanupOrphanImmediate();

    // ========================================================================
    // Fader / DSP Controls (thread-safe)
    // ========================================================================

    void setFaderCurve(Engine::FadeCurve curve) override;
    void faderFadeIn(int durationMs, double targetVolume, uint64_t token = 0) override;
    void faderFadeOut(int durationMs, double currentVolume, uint64_t token = 0) override;
    void faderStop() override;
    void faderReset() override;
    [[nodiscard]] bool faderIsFading() const override;
    //! Configure the QObject that receives coalesced pipeline wake events.
    //! Set once before start and clear before teardown.
    void setSignalWakeTarget(QObject* target, int wakeEventType = 0);
    //! Set optional analysis bus tap for post-master processed PCM.
    void setAnalysisBus(AudioAnalysisBus* analysisBus);
    //! Drain and clear pending level-triggered signals on engine thread.
    [[nodiscard]] PendingSignals drainPendingSignals();

    // ========================================================================
    // Status (thread-safe)
    // ========================================================================

    [[nodiscard]] PipelineStatus currentStatus() const;

    //! Reported pipeline playback delay in milliseconds (output + DSP latency).
    [[nodiscard]] uint64_t playbackDelayMs() const;
    //! Scale to convert output-time delay to source timeline delay.
    [[nodiscard]] double playbackDelayToTrackScale() const;

    //! True while audio worker thread is alive.
    [[nodiscard]] bool isRunning() const;
    //! True when an output backend is currently attached.
    [[nodiscard]] bool hasOutput() const;

    [[nodiscard]] AudioFormat inputFormat() const;
    [[nodiscard]] AudioFormat outputFormat() const;

    [[nodiscard]] QString lastInitError() const;

    //! Drain up to @p capacity pending fade completion events.
    size_t drainFadeEvents(FadeEvent* outEvents, size_t capacity);

private:
    //! Main worker loop run by `m_thread`.
    void audioThreadFunc();
    //! Drain queued control commands; returns false when shutdown should stop cycle work.
    [[nodiscard]] bool processCommands();
    //! Execute one audio cycle (mix/process/write/state publication).
    void processAudio();
    [[nodiscard]] OutputState stateWithWrites(const OutputState& state, int framesWrittenThisCycle) const;

    enum class PositionBasis : uint8_t
    {
        DecodeHead = 0,
        RenderedSource
    };
    //! Publish delay and timeline scaling from output+mixer state.
    void updatePlaybackDelay(const OutputState& outputState, PositionBasis basis);
    //! Flush partial output tail from previous short write.
    bool drainPendingOutput(const OutputState& state, int& freeFrames, int& framesWrittenThisCycle);
    //! Handle no-free-frames backoff path.
    bool handleNoFreeFrames(const OutputState& state, int freeFrames, int framesWrittenThisCycle);
    //! Handle mixer-produced-0-frames path (underrun / wait).
    bool handleMixerUnderrun(const OutputState& state, int framesWrittenThisCycle);
    //! Apply master DSP chain and output fader to `m_processChunks`.
    void applyMasterProcessing();
    //! Tap post-master processing audio to analysis bus.
    void tapAnalysis();
    [[nodiscard]] bool sourceTimelinePositionForFrameOffset(const ProcessingBufferList& chunks, int frameOffset,
                                                            double fallbackScale, uint64_t& positionMsOut);
    //! Convert/write processed buffers to output backend.
    void writeToOutput(int& framesWrittenThisCycle);
    //! Update published transport/position snapshots after a cycle.
    void updatePlaybackState(const OutputState& state, int framesWrittenThisCycle);
    //! Rebuild DSP/mixer format path from new input format.
    AudioFormat configureInputAndDspForFormat(const AudioFormat& format);
    //! Build per-track DSP node instances from configured definitions.
    [[nodiscard]] std::vector<DspNodePtr> buildPerTrackNodes() const;
    //! Predict per-track stage output format for given upstream input.
    [[nodiscard]] AudioFormat perTrackOutputFormat(const AudioFormat& inputFormat) const;
    //! Slice frame window from interleaved audio buffer.
    static AudioBuffer sliceBufferFrames(const AudioBuffer& buffer, int frameOffset, int frameCount);
    //! Write frame range to output backend.
    int writeOutputBufferFrames(const AudioBuffer& buffer, int frameOffset);
    [[nodiscard]] bool hasPendingOutput() const;
    void appendPendingOutput(const AudioBuffer& buffer);
    void clearPendingOutput();
    //! Push fade-completion event into SPSC queue.
    void queueFadeEvent(const FadeEvent& event);
    void setLastInitError(QString error);
    //! Track currently orphaned stream and arm cleanup deadline.
    void setOrphanState(StreamId streamId, uint64_t playbackDelayMs);
    void clearOrphanState();
    //! Cleanup orphan if fully drained or deadline timed out.
    void cleanupOrphanIfDone();

    template <typename T>
    struct CommandResponse
    {
        std::mutex mutex;
        std::condition_variable cv;
        bool done{false};
        std::optional<T> value;
    };

    struct CommandResponseVoid
    {
        std::mutex mutex;
        std::condition_variable cv;
        bool done{false};
    };

    struct FormatSnapshot
    {
        int sampleRate{0};
        int channelCount{0};
        SampleFormat sampleFormat{SampleFormat::Unknown};
        bool hasChannelLayout{false};
        std::array<AudioFormat::ChannelPosition, AudioFormat::MaxChannels> channelLayout{
            AudioFormat::ChannelPosition::UnknownPosition};
    };

    struct TimelineChunk
    {
        uint64_t startNs{0};
        int frames{0};
        int sampleRate{0};
    };

    static void notifyResponse(const std::shared_ptr<CommandResponseVoid>& response);
    static std::shared_ptr<const FormatSnapshot> makeSnapshot(const AudioFormat& format);
    static AudioFormat formatFromSnapshot(const std::shared_ptr<const FormatSnapshot>& snapshot);
    void resetMasterRateObservation();
    //! Signal decode side that stream data demand changed.
    void notifyDataDemand(bool required);

    enum class PendingSignal : uint32_t
    {
        //! One or more streams are below low-water and decoder should be nudged.
        NeedsData = 1U << 0,
        //! One or more fade completion events are queued.
        FadeEventsReady = 1U << 1
    };

    static constexpr uint32_t pendingSignalMask(PendingSignal signal)
    {
        return static_cast<uint32_t>(signal);
    }

    void clearPendingSignals(uint32_t signalMask);
    void resetQueuedNotifications(bool resetWakeState);
    void markPendingSignal(PendingSignal signal);

    template <typename T, typename U>
    static void notifyResponse(const std::shared_ptr<CommandResponse<T>>& response, U&& value)
        requires std::constructible_from<T, U&&>
    {
        {
            std::lock_guard lock{response->mutex};
            response->value.emplace(std::forward<U>(value));
            response->done = true;
        }
        response->cv.notify_one();
    }

    template <class Self>
    using PipelineView
        = std::conditional_t<std::is_const_v<std::remove_reference_t<Self>>, const AudioPipeline&, AudioPipeline&>;

    template <class Self>
    static constexpr auto selfView(Self&& self) noexcept -> PipelineView<Self>
    {
        return static_cast<PipelineView<Self>>(self);
    }

    template <class Self>
    static constexpr auto pipelineView(AudioPipeline& pipeline) noexcept -> PipelineView<Self>
    {
        return static_cast<PipelineView<Self>>(pipeline);
    }

    template <class Self, class Fn>
    static auto invokeDirectly(Self&& self, Fn&& fn) -> auto
    {
        using Result = std::invoke_result_t<Fn, PipelineView<Self>>;
        if constexpr(std::is_void_v<Result>) {
            std::invoke(std::forward<Fn>(fn), selfView(std::forward<Self>(self)));
            return true;
        }
        else {
            return std::invoke(std::forward<Fn>(fn), selfView(std::forward<Self>(self)));
        }
    }

    template <class Self, class Fn>
    static auto enqueueAndWaitImpl(Self&& self, Fn&& fn) -> auto
        requires std::invocable<Fn, PipelineView<Self>>
    {
        using View   = PipelineView<Self>;
        using Result = std::invoke_result_t<Fn, View>;
        using Stored = std::conditional_t<std::is_void_v<Result>, std::monostate, Result>;

        if(self.isAudioThread()) {
            return invokeDirectly(std::forward<Self>(self), std::forward<Fn>(fn));
        }

        auto response = std::make_shared<CommandResponse<Stored>>();

        if(!self.enqueueCommand([fn = std::forward<Fn>(fn), response](AudioPipeline& pipeline) mutable {
               if constexpr(std::is_void_v<Result>) {
                   std::invoke(fn, pipelineView<Self>(pipeline));
                   notifyResponse(response, std::monostate{});
               }
               else {
                   notifyResponse(response, std::invoke(fn, pipelineView<Self>(pipeline)));
               }
           })) {
            qWarning() << "AudioPipeline command enqueue failed";
            if constexpr(std::is_void_v<Result>) {
                return false;
            }
            else {
                return Result{};
            }
        }

        std::unique_lock lock{response->mutex};
        if(!response->cv.wait_for(lock, CommandWaitTimeout, [response] { return response->done; })) {
            qWarning() << "AudioPipeline command timed out";
            if constexpr(std::is_void_v<Result>) {
                return false;
            }
            else {
                return Result{};
            }
        }

        if constexpr(std::is_void_v<Result>) {
            return true;
        }
        else {
            return std::move(*response->value);
        }
    }

    template <class Fn>
    auto enqueueAndWait(Fn&& fn) -> auto
        requires std::invocable<Fn, AudioPipeline&>
    {
        return enqueueAndWaitImpl(*this, std::forward<Fn>(fn));
    }

    template <class Fn>
    auto enqueueAndWait(Fn&& fn) const -> auto
        requires std::invocable<Fn, const AudioPipeline&>
    {
        return enqueueAndWaitImpl(*this, std::forward<Fn>(fn));
    }

    template <class Fn>
    auto onAudioThread(Fn&& fn) -> auto
        requires std::invocable<Fn, AudioPipeline&>
    {
        if(!m_running.load(std::memory_order_acquire)) {
            return invokeDirectly(*this, std::forward<Fn>(fn));
        }
        return enqueueAndWait(std::forward<Fn>(fn));
    }

    template <class Fn>
    auto onAudioThread(Fn&& fn) const -> auto
        requires std::invocable<Fn, const AudioPipeline&>
    {
        if(!m_running.load(std::memory_order_acquire)) {
            return invokeDirectly(*this, std::forward<Fn>(fn));
        }
        return enqueueAndWait(std::forward<Fn>(fn));
    }

    template <class Fn>
    void enqueueAsync(Fn&& fn)
        requires std::is_void_v<std::invoke_result_t<Fn, AudioPipeline&>> && std::invocable<Fn, AudioPipeline&>
    {
        if(!enqueueCommand(std::forward<Fn>(fn))) {
            qWarning() << "AudioPipeline async command enqueue failed";
        }
    }

    template <class Fn>
    void onAudioThreadAsync(Fn&& fn)
        requires std::is_void_v<std::invoke_result_t<Fn, AudioPipeline&>> && std::invocable<Fn, AudioPipeline&>
    {
        if(!m_running.load(std::memory_order_acquire)) {
            std::invoke(std::forward<Fn>(fn), *this);
            return;
        }
        enqueueAsync(std::forward<Fn>(fn));
    }

    using PipelineCommand = MoveOnlyFunction<void(AudioPipeline&)>;
    //! Returns false when pipeline is not running/shutting down, or queue is full.
    bool enqueueCommand(PipelineCommand cmd) const;

    static uint64_t currentThreadIdHash();
    [[nodiscard]] bool isAudioThread() const;

    static constexpr auto CommandWaitTimeout{std::chrono::seconds(2)};
    static constexpr size_t MaxCommandsPerCycle{64};
    static constexpr size_t MaxQueuedCommands{4096};
    static constexpr size_t MaxQueuedFadeEvents{128};

    LockFreeRingBuffer<FadeEvent> m_fadeEvents;
    std::thread m_thread;
    std::atomic<uint64_t> m_audioThreadIdHash;
    std::unique_ptr<AudioOutput> m_output;
    double m_masterVolume;
    std::atomic<DspRegistry*> m_dspRegistry;
    AudioBuffer m_coalescedOutputBuffer;
    AudioBuffer m_pendingOutputBuffer;
    std::atomic<uint64_t> m_position;
    std::atomic<bool> m_positionIsMapped;
    std::atomic<uint64_t> m_playbackDelayMs;
    std::atomic<double> m_playbackDelayToTrackScale;
    size_t m_fadeEventDropCount;

    AtomicSharedPtr<const FormatSnapshot> m_inputSnapshot;
    AtomicSharedPtr<const FormatSnapshot> m_outputSnapshot;
    AtomicSharedPtr<const QString> m_lastInitError;
    std::atomic<AudioAnalysisBus*> m_analysisBus;

    Engine::DspChain m_perTrackChainDefs;
    ProcessingBufferList m_processChunks;
    std::vector<float> m_analysisScratch;
    std::vector<TimelineChunk> m_timelineChunksScratch;
    PipelineSignalMailbox m_signalMailbox;
    OrphanStreamTracker m_orphanTracker;

    mutable std::mutex m_lifecycleMutex;
    mutable std::mutex m_commandMutex;
    std::mutex m_wakeMutex;
    mutable std::condition_variable m_wakeCondition;

    ProcessingBuffer m_processBuffer;
    mutable std::deque<PipelineCommand> m_commandQueue;
    OutputFader m_outputFader;
    StreamRegistry m_streamRegistry;
    DspChain m_dspChain;
    AudioMixer m_mixer;

    int m_bufferFrames;
    int m_pendingOutputFrameOffset;

    AudioFormat m_inputFormat;
    AudioFormat m_outputFormat;

    std::atomic<PipelinePlaybackState> m_playbackState;
    std::atomic<bool> m_audioThreadReady;
    std::atomic<bool> m_running;
    std::atomic<bool> m_shutdownRequested;
    std::atomic<bool> m_playing;

    std::atomic<bool> m_outputInitialized;
    bool m_outputSupportsVolume;

    SampleFormat m_outputBitdepth;
    bool m_ditherEnabled;

    bool m_dataDemandNotified;
    std::atomic<bool> m_bufferUnderrun;
    bool m_cycleMappedPositionValid;
    uint64_t m_cycleMappedPositionMs;
    double m_observedMasterRateScale;
    uint64_t m_prevMasterOutputStartMs;
    int m_prevMasterOutputFrames;
    bool m_hasMasterRateObservation;
};
} // namespace Fooyin
