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
#include "core/engine/pipeline/sampleringbuffer.h"

#include <core/engine/dsp/dspnode.h>
#include <core/engine/dsp/processingbuffer.h>
#include <core/engine/dsp/processingbufferlist.h>

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace Fooyin {
/*!
 * Audio-thread stream stage used by AudioPipeline.
 *
 * AudioMixer owns the set of active `AudioStream`s and produces a single mixed
 * F64 `ProcessingBuffer` block for each audio callback cycle. For each active stream
 * it:
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

    explicit AudioMixer(const AudioFormat& format = {});

    AudioMixer(const AudioMixer&)            = delete;
    AudioMixer& operator=(const AudioMixer&) = delete;

    //! Set expected per-stream input format (pre per-track DSP).
    void setFormat(const AudioFormat& format);
    //! Set post per-track DSP format emitted by read().
    void setOutputFormat(const AudioFormat& format);

    //! Add a stream to be mixed
    void addStream(AudioStreamPtr stream);
    //! Add a stream with per-track DSP nodes
    void addStream(AudioStreamPtr stream, std::vector<DspNodePtr> perTrackNodes);

    //! Remove a specific stream by pointer identity.
    void removeStream(const AudioStreamPtr& stream);
    //! Remove a specific stream by ID.
    void removeStream(StreamId id);

    //! Remove all active streams and per-stream processing state.
    void clear();

    //! Check if a stream is already present
    [[nodiscard]] bool hasStream(StreamId id) const;
    //! Primary stream (first non-fading-out, non-stopped stream).
    [[nodiscard]] AudioStreamPtr primaryStream() const;
    //! Get number of active streams
    [[nodiscard]] size_t streamCount() const;

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

    //! Rebuild per-track DSP chains for all active streams.
    void rebuildAllPerTrackDsps(const std::function<std::vector<DspNodePtr>()>& factory);

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

    /*!
     * True when decode side should be nudged to provide more data.
     *
     * - Returns true if any active stream is below its low-water threshold.
     * - Streams that already reached end-of-input are excluded.
     */
    [[nodiscard]] bool needsMoreData() const;

    //! Get buffered duration of primary stream in milliseconds
    [[nodiscard]] uint64_t bufferedDurationMs() const;

    //! Total per-track DSP/post-processor latency on the current primary stream.
    [[nodiscard]] int primaryStreamDspLatencyFrames() const;
    //! Total per-track DSP/post-processor latency on the current primary stream.
    [[nodiscard]] double primaryStreamDspLatencySeconds() const;

private:
    struct MixerChannel;
    struct StreamCategorization;
    struct ReadPlan;
    struct ReadCycleState;

    struct TimelineSegment
    {
        bool valid{false};
        uint64_t startNs{0};
        uint64_t frameDurationNs{0};
        int frames{0};
    };

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
        std::span<const TimelineSegment> timelineSegments{};
    };

    PerTrackResult processPerTrackDsps(MixerChannel& channel, double* inputData, int inputFrames,
                                       uint64_t inputSourceStartNs);

    struct StreamDspFifo
    {
        SampleRingBuffer buffer;
        std::deque<TimelineSegment> timeline;
        int channels{0};
        bool endFlushed{false};
    };

    static int fifoFrames(const AudioMixer::StreamDspFifo& f) noexcept;
    static void fifoAppend(StreamDspFifo& f, const double* data, int frames, int channels);
    static int fifoConsume(StreamDspFifo& f, double* dst, int frames, int channels);

    static uint64_t frameDurationNs(int sampleRate);
    static void appendTimelineSegment(std::deque<TimelineSegment>& segments, const TimelineSegment& segment);
    static void appendUnknownTimeline(std::deque<TimelineSegment>& segments, int frames);
    static void clearTimeline(StreamDspFifo& f);
    static void discardTimeline(std::deque<TimelineSegment>& segments, int frames);
    static void discardTimeline(StreamDspFifo& f, int frames);
    static void consumeTimeline(StreamDspFifo& f, int frames, std::vector<TimelineSegment>& out);
    static void consumeTimeline(std::deque<TimelineSegment>& segments, int frames, std::vector<TimelineSegment>& out);
    static bool timelineStartNs(const std::vector<TimelineSegment>& segments, uint64_t& startNsOut);
    static bool timelineEndNs(const std::vector<TimelineSegment>& segments, uint64_t& endNsOut);
    static double calculateDspLatencySeconds(const std::vector<DspNodePtr>& nodes, const AudioFormat& inputFormat);

    struct StreamTrackProcessorState
    {
        std::vector<std::unique_ptr<TrackProcessor>> processors;
        Track track;
        uint64_t trackRevision{0};
        bool hasTrack{false};
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
        StreamTrackProcessorState trackProcessors;
    };

    static void endTrackProcessors(StreamTrackProcessorState& state);
    void rebuildStreamTrackProcessors(MixerChannel& channel);
    StreamTrackProcessorState* ensureStreamTrackProcessors(MixerChannel& channel);

    MixerChannel* findChannel(StreamId id);
    const MixerChannel* findChannel(StreamId id) const;

    static bool isMixingState(AudioStream::State state);

    StreamCategorization categorizeStreams() const;
    size_t calculateMaxInputSamples(int outRate, int processFrames) const;
    void prepareMixBuffers(const ReadPlan& plan, size_t maxInputSamples);

    int mixSingleStream(MixerChannel& channel, const ReadPlan& plan, int targetFrames, int outputFrameOffset,
                        bool consumeOutput = true, bool captureTimeline = false,
                        std::vector<TimelineRange>* capturedTimeline = nullptr);
    void warmPendingStreams(const ReadPlan& plan, const StreamCategorization& categorization);
    ReadCycleState mixActiveStreams(const ReadPlan& plan, StreamId pendingStreamToStartId);

    void handleGaplessTransition(const ReadPlan& plan, StreamId pendingStreamToStartId, ReadCycleState& cycleState);
    void pruneStoppedStreams(size_t writeIndex);

    void commitToOutput(const ReadPlan& plan, StreamId pendingStreamToStartId, const ReadCycleState& cycleState);
    int drainOutput(ProcessingBuffer& output, const ReadPlan& plan);

    struct StreamCategorization
    {
        int activeMixingStreams{0};
        StreamId pendingStreamToStartId{InvalidStreamId};
        bool haveNonPendingActiveStream{false};
    };

    struct ReadPlan
    {
        int outChannels{0};
        int outRate{0};
        size_t requestedSamples{0};
        int processFrames{0};
        bool crossfadeActive{false};
    };

    struct ReadCycleState
    {
        size_t writeIndex{0};
        bool hasActiveStream{false};
        bool startedPendingStream{false};
        bool rateConversionWarmup{false};
        bool hasTrackedPrimary{false};
        int producedFrames{0};
        StreamId splicedPendingStreamId{InvalidStreamId};
    };

    struct MixerScratchpad
    {
        std::vector<double> outputSamples;
        std::vector<double> mixSamples;
        std::vector<double> streamTemp;
        std::vector<double> perTrackCoalesced;
        std::vector<TimelineRange> trackedTimeline;
        std::vector<TimelineSegment> perTrackTimeline;
        std::vector<TimelineSegment> consumedTimeline; // reused by consumeTimeline() callers
        ProcessingBuffer perTrackBuffer;
        ProcessingBufferList perTrackChunks;
    };

    AudioFormat m_format;
    AudioFormat m_outputFormat;
    std::vector<MixerChannel> m_channels;
    MixerScratchpad m_scratch;
    std::vector<TrackProcessorFactory> m_trackProcessorFactories;
    SampleRingBuffer m_outputLeftover;
    std::deque<TimelineSegment> m_outputTimeline;
};
} // namespace Fooyin
