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

#include "core/engine/output/postprocessor/trackprocessor.h"
#include "core/engine/pipeline/audiostream.h"
#include "core/engine/pipeline/buffereddspstage.h"

#include <core/engine/dsp/dspnode.h>
#include <core/engine/dsp/processingbuffer.h>
#include <core/engine/dsp/processingbufferlist.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace Fooyin {
class MixerOutput;

/*!
 * Audio-thread stream mixing stage used by `AudioPipeline`.
 *
 * AudioMixer owns the set of active `AudioStream`s and produces a single mixed
 * F64 `ProcessingBuffer` block for each audio callback cycle.
 *
 * For each active stream it:
 * 1. reads decoded samples from the stream ring buffer,
 * 2. runs per-stream `TrackProcessor`s (e.g. replaygain),
 * 3. runs optional per-track DSP node chain for that stream,
 * 4. applies stream fade ramp and accumulates into the shared mix buffer.
 *
 * It also tracks per-stream DSP latency, primary-stream selection, and
 * crossfade overlap semantics (active stream + optional orphaned/pending
 * stream handoff).
 *
 * Threading:
 * - `AudioMixer` itself is not thread-safe.
 * - It is intended to be owned and called only from the AudioPipeline audio
 *   thread.
 * - No internal locks are used.
 *
 * Format model:
 * - Input format is the expected per-stream PCM format before per-track DSP.
 * - Output format is the post per-track DSP format consumed by master DSP/output.
 */
class FYCORE_EXPORT AudioMixer
{
public:
    using TrackProcessorFactory = std::function<std::unique_ptr<TrackProcessor>()>;
    struct PerTrackPatchPlan
    {
        size_t prefixKeep{0};
        size_t oldMiddleCount{0};
        size_t newMiddleCount{0};
        bool hasChanges{false};
    };

    struct TimelineSegment
    {
        bool valid{false};
        uint64_t startNs{0};
        uint64_t frameDurationNs{0};
        int frames{0};
        StreamId streamId{InvalidStreamId};
    };

    struct ReadResult
    {
        int producedFrames{0};
        int sourceFrames{0};
        bool buffering{false};
        StreamId primaryStreamId{InvalidStreamId};
        std::vector<TimelineSegment> primaryTimeline;
    };

    explicit AudioMixer(const AudioFormat& format = {});
    ~AudioMixer();

    AudioMixer(const AudioMixer&)            = delete;
    AudioMixer& operator=(const AudioMixer&) = delete;

    //! Set expected per-stream input format (pre per-track DSP).
    void setFormat(const AudioFormat& format);
    //! Set post per-track DSP format emitted by read().
    void setOutputFormat(const AudioFormat& format);

    //! Add a stream to be mixed.
    void addStream(AudioStreamPtr stream);
    //! Add a stream with per-track DSP nodes.
    void addStream(AudioStreamPtr stream, std::vector<DspNodePtr> perTrackNodes);

    //! Remove a specific stream by pointer identity.
    void removeStream(const AudioStreamPtr& stream);
    //! Remove a specific stream by ID.
    void removeStream(StreamId id);

    //! Remove all active streams and per-stream processing state.
    void clear();

    //! True if stream id is already active.
    [[nodiscard]] bool hasStream(StreamId id) const;
    //! Primary stream (first non-fading-out, non-stopped stream).
    [[nodiscard]] AudioStreamPtr primaryStream() const;
    //! Number of stream channels currently tracked by the mixer (includes pending/paused).
    [[nodiscard]] size_t streamCount() const;
    //! Number of streams currently in a mixing state (excludes pending/paused/stopped).
    [[nodiscard]] size_t mixingStreamCount() const;

    //! Clear internal temporary FIFOs/leftovers used across reads.
    void resetBuffers();
    //! Reset per-track DSP/FIFO state for a stream seek discontinuity.
    void resetStreamForSeek(StreamId id);

    /*!
     * Switch to a new stream immediately (no crossfade).
     * Clears all existing streams.
     */
    void switchTo(AudioStreamPtr newStream);

    //! Switch to a new stream with per-track DSP nodes (no overlap/crossfade, audio thread only).
    void switchTo(AudioStreamPtr newStream, std::vector<DspNodePtr> perTrackNodes);

    /*!
     * Rebuild per-track DSP chains for all active streams.
     *
     * @param factory Produces a fresh full per-track node list per stream.
     * @param preserveBufferedOutput
     *        When true, keep per-stream buffered DSP output/runtime continuity and avoid
     *        clearing per-track buffered audio state.
     *        Intended for soft chain updates where per-track output format is unchanged.
     */
    void rebuildAllPerTrackDsps(const std::function<std::vector<DspNodePtr>()>& factory,
                                bool preserveBufferedOutput = false);
    //! True when per-track patch bounds are valid for every active stream.
    [[nodiscard]] bool canApplyIncrementalPerTrackPatch(const PerTrackPatchPlan& patchPlan) const;
    /*!
     * Apply a per-track patch to each active stream chain.
     *
     * @param patchPlan Prefix/middle replacement plan.
     * @param middleFactory Produces replacement middle nodes for each stream.
     * @param preserveBufferedOutput
     *        When true, keep per-stream buffered DSP output/runtime continuity and avoid
     *        clearing per-track buffered audio state.
     *        Intended for soft chain updates where per-track output format is unchanged.
     *
     * @return True when patch was applied for all active streams.
     */
    bool applyIncrementalPerTrackPatch(const PerTrackPatchPlan& patchPlan,
                                       const std::function<std::vector<DspNodePtr>()>& middleFactory,
                                       bool preserveBufferedOutput = false);
    //! Apply live settings to active per-track DSP instances matching instance id.
    void applyLivePerTrackDspSettings(uint64_t instanceId, const QByteArray& settings, uint64_t revision);

    //! Register factory for per-stream track processors (for example replaygain, audio thread only).
    void addTrackProcessorFactory(TrackProcessorFactory factory);

    //! Pause all active streams.
    void pauseAll();

    //! Resume paused streams.
    void resumeAll();

    /*!
     * Mix active streams into @p output.
     *
     * @param output Destination buffer (interleaved double samples, F64)
     * @param frames Number of frames to produce
     *
     * - Output format is always interleaved F64 in mixer output-channel layout.
     * - May return fewer than @p frames when streams are ending or data is unavailable.
     * - Returns 0 when no data could be produced for this cycle.
     * - When returning > 0, @p output contains exactly the returned frame count.
     *
     * @return Frames written
     */
    int read(ProcessingBuffer& output, int frames);
    //! Same as `read()` plus cycle metadata used by pipeline timeline logic.
    [[nodiscard]] ReadResult readWithStatus(ProcessingBuffer& output, int frames);

    /*!
     * True when decode side should be nudged to provide more data.
     *
     * - Returns true if any active stream is below its low-water threshold.
     * - Streams that already reached end-of-input are excluded.
     */
    [[nodiscard]] bool needsMoreData() const;

    //! Buffered duration of primary stream in milliseconds.
    [[nodiscard]] uint64_t bufferedDurationMs() const;

    //! Per-track DSP/post-processor latency on current primary stream in frames.
    [[nodiscard]] int primaryStreamDspLatencyFrames() const;
    //! Per-track DSP/post-processor latency on current primary stream in seconds.
    [[nodiscard]] double primaryStreamDspLatencySeconds() const;
    //! Per-track DSP in-flight delay in nanoseconds (source-domain) for the primary stream.
    [[nodiscard]] uint64_t primaryStreamPerTrackDspInflightNs() const;

private:
    struct MixerChannel;
    struct StreamCategorization;
    struct ReadPlan;
    struct ReadCycleState;
    struct StreamTopUpBudget;

    struct TimelineRange
    {
        int outputOffsetFrames{0};
        TimelineSegment segment;
    };

    struct PerTrackResult
    {
        // Pointer is only valid until the next per-track processing call that may
        // reassign/reallocate mixer-owned scratch buffers
        const double* data{nullptr};
        int frames{0};
        int channels{0};
        int sampleRate{0};
        std::span<const TimedAudioFifo::TimelineChunk> timelineChunks{};
    };

    PerTrackResult processPerTrackDsps(MixerChannel& channel, double* inputData, int inputFrames,
                                       uint64_t inputSourceStartNs);

    struct StreamDspFifo
    {
        BufferedDspStage stage;
        bool endFlushed{false};
    };

    struct PerTrackInflightEstimator
    {
        void resetBoundary();
        void update(uint64_t consumedInputFrames, int inputRate, uint64_t producedOutputFrames, int outputRate);
        [[nodiscard]] uint64_t inflightFramesAt(int outputSampleRate) const;

    private:
        [[nodiscard]] static uint64_t accumulateNsWithRemainder(uint64_t frames, int sampleRate, uint64_t& remainder);

        uint64_t pendingNs{0};
        uint64_t remIn{0};
        uint64_t remOut{0};
    };

    static uint64_t frameDurationNs(int sampleRate);
    static double calculateDspLatencySeconds(const std::vector<DspNodePtr>& nodes, const AudioFormat& inputFormat);
    [[nodiscard]] static int saturatingAddFrames(int lhs, int rhs) noexcept;
    [[nodiscard]] static int runtimeLatencyFrames(const MixerChannel& channel, int sampleRate);

    struct StreamTrackProcessorState
    {
        std::vector<std::unique_ptr<TrackProcessor>> processors;
        Track track;
        uint64_t trackRevision{0};
        bool hasTrack{false};
    };

    enum class ResetMode : uint8_t
    {
        OutputFormatChange = 0,
        RuntimeReset,
        SeekReset,
        RebuildPerTrack,
    };

    struct MixerChannel
    {
        MixerChannel()                                   = default;
        MixerChannel(const MixerChannel&)                = delete;
        MixerChannel& operator=(const MixerChannel&)     = delete;
        MixerChannel(MixerChannel&&) noexcept            = default;
        MixerChannel& operator=(MixerChannel&&) noexcept = default;

        AudioStreamPtr stream;
        StreamDspFifo dspFifo;
        double inputFrameAccumulator{0.0};
        std::vector<DspNodePtr> perTrackDsps;
        double dspLatencySeconds{0.0};
        PerTrackInflightEstimator inflightEstimator;
        StreamTrackProcessorState trackProcessors;
    };

    static void endTrackProcessors(StreamTrackProcessorState& state);
    void rebuildStreamTrackProcessors(MixerChannel& channel);
    StreamTrackProcessorState* ensureStreamTrackProcessors(MixerChannel& channel);
    static void resetChannelRuntimeState(MixerChannel& channel, ResetMode mode);

    MixerChannel* findChannel(StreamId id);
    const MixerChannel* findChannel(StreamId id) const;

    static bool isMixingState(AudioStream::State state);

    StreamCategorization categorizeStreams() const;
    size_t calculateMaxInputSamples(int outRate, int processFrames) const;
    void prepareMixBuffers(const ReadPlan& plan, size_t maxInputSamples);

    int mixSingleStream(MixerChannel& channel, const ReadPlan& plan, int targetFrames, int outputFrameOffset,
                        bool consumeOutput = true, bool captureTimeline = false,
                        std::vector<TimelineRange>* capturedTimeline = nullptr, int* sourceFramesReadOut = nullptr,
                        bool* bufferingOut = nullptr);
    [[nodiscard]] static StreamTopUpBudget buildStreamTopUpBudget(MixerChannel& channel, const ReadPlan& plan,
                                                                  int targetFrames);
    static bool flushStreamEndTail(MixerChannel& channel, const ReadPlan& plan, const AudioFormat& stageFormat);
    void fillStreamPendingOutput(MixerChannel& channel, const ReadPlan& plan, int targetFrames,
                                 const StreamTopUpBudget& budget, const AudioFormat& stageFormat, int& sourceFramesRead,
                                 bool& buffering);
    int consumeStreamPendingOutput(MixerChannel& channel, const ReadPlan& plan, int targetFrames, int outputFrameOffset,
                                   bool captureTimeline, std::vector<TimelineRange>* capturedTimeline);
    void applyStreamFadeToMix(const ReadPlan& plan, AudioStream& stream, int outputFrameOffset, int gotFrames);
    void warmPendingStreams(const ReadPlan& plan, const StreamCategorization& categorization);
    ReadCycleState mixActiveStreams(const ReadPlan& plan, StreamId pendingStreamToStartId);

    void handleGaplessTransition(const ReadPlan& plan, StreamId pendingStreamToStartId, ReadCycleState& cycleState);
    void pruneStoppedStreams(size_t writeIndex);

    void commitToOutput(const ReadPlan& plan, StreamId pendingStreamToStartId, const ReadCycleState& cycleState);
    int drainOutput(ProcessingBuffer& output, const ReadPlan& plan);

    struct StreamCategorization
    {
        StreamId pendingStreamToStartId{InvalidStreamId};
        bool haveNonPendingActiveStream{false};
    };

    struct ReadPlan
    {
        int outChannels{0};
        int outRate{0};
        size_t requestedSamples{0};
        int processFrames{0};
    };

    struct ReadCycleState
    {
        size_t writeIndex{0};
        bool hasActiveStream{false};
        bool startedPendingStream{false};
        bool dspBuffering{false};
        bool hasTrackedPrimary{false};
        int producedFrames{0};
        int sourceFramesRead{0};
        StreamId splicedPendingStreamId{InvalidStreamId};
        StreamId trackedPrimaryStreamId{InvalidStreamId};
    };

    struct StreamTopUpBudget
    {
        int inputFramesPerRead{1};
        int maxTopUpIters{1};
        int maxBufferingReadsThisCycle{1};
    };

    struct MixerScratchpad
    {
        std::vector<double> outputSamples;
        std::vector<double> mixSamples;
        std::vector<double> streamTemp;
        std::vector<double> perTrackCoalesced;
        std::vector<TimelineRange> trackedTimeline;
        std::vector<TimelineSegment> outputTimeline;
        std::vector<TimedAudioFifo::TimelineChunk> perTrackTimeline;
        std::vector<TimelineSegment> consumedTimeline; // reused by consumeTimeline() callers
        ProcessingBuffer perTrackBuffer;
        ProcessingBufferList perTrackChunks;
    };

    AudioFormat m_format;
    AudioFormat m_outputFormat;
    std::vector<MixerChannel> m_channels;
    MixerScratchpad m_scratch;
    std::vector<TrackProcessorFactory> m_trackProcessorFactories;
    std::unique_ptr<MixerOutput> m_output;
};
} // namespace Fooyin
