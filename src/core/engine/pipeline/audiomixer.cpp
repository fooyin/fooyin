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

#include "audiomixer.h"

#include <utils/timeconstants.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ranges>

constexpr auto MinProcessFrames = 256;
constexpr auto MaxTopUpIters    = 16;

namespace Fooyin {
AudioMixer::AudioMixer(const AudioFormat& format)
    : m_format{format}
{
    AudioFormat mixFormat = format.isValid() ? format : AudioFormat{SampleFormat::F64, 48000, 2};
    mixFormat.setSampleFormat(SampleFormat::F64);

    const auto bufferSamples = static_cast<size_t>(mixFormat.sampleRate()) * mixFormat.channelCount();
    m_scratch.outputSamples.resize(bufferSamples);
    m_scratch.mixSamples.resize(bufferSamples);
}

void AudioMixer::setFormat(const AudioFormat& format)
{
    m_format = format;

    if(!m_outputFormat.isValid()) {
        m_outputFormat = format;
    }

    if(format.isValid()) {
        AudioFormat mixFormat = format;
        mixFormat.setSampleFormat(SampleFormat::F64);
        const auto bufferSamples = static_cast<size_t>(mixFormat.sampleRate()) * mixFormat.channelCount();
        m_scratch.mixSamples.resize(bufferSamples);

        const auto& outFmt       = m_outputFormat.isValid() ? m_outputFormat : format;
        const auto outputSamples = static_cast<size_t>(outFmt.sampleRate()) * outFmt.channelCount();
        m_scratch.outputSamples.resize(outputSamples);
    }
}

void AudioMixer::setOutputFormat(const AudioFormat& format)
{
    m_outputFormat = format;

    for(auto& channel : m_channels) {
        channel.inputFrameAccumulator = 0.0;
    }

    m_outputLeftover.clear();
    m_outputTimeline.clear();

    if(format.isValid()) {
        const auto outputSamples = static_cast<size_t>(format.sampleRate()) * format.channelCount();
        m_scratch.outputSamples.resize(outputSamples);
    }
}

void AudioMixer::resetBuffers()
{
    m_outputLeftover.clear();
    m_outputTimeline.clear();

    for(auto& channel : m_channels) {
        channel.dspFifo.buffer.clear();
        clearTimeline(channel.dspFifo);
        channel.inputFrameAccumulator = 0.0;
    }
}

void AudioMixer::resetStreamForSeek(const StreamId id)
{
    if(id == InvalidStreamId) {
        return;
    }

    m_outputLeftover.clear();
    m_outputTimeline.clear();

    for(auto& channel : m_channels) {
        if(!channel.stream || channel.stream->id() != id) {
            continue;
        }

        channel.dspFifo.buffer.clear();
        clearTimeline(channel.dspFifo);
        channel.dspFifo.endFlushed    = false;
        channel.inputFrameAccumulator = 0.0;

        for(auto& node : channel.perTrackDsps) {
            if(node) {
                (*node).reset();
            }
        }
    }
}

void AudioMixer::addTrackProcessorFactory(TrackProcessorFactory factory)
{
    if(!factory) {
        return;
    }

    m_trackProcessorFactories.push_back(std::move(factory));

    for(auto& channel : m_channels) {
        rebuildStreamTrackProcessors(channel);
    }
}

void AudioMixer::endTrackProcessors(StreamTrackProcessorState& state)
{
    for(auto& processor : state.processors) {
        if(processor) {
            processor->endOfTrack();
        }
    }

    state.track         = {};
    state.trackRevision = 0;
    state.hasTrack      = false;
}

void AudioMixer::rebuildStreamTrackProcessors(MixerChannel& channel)
{
    auto& stream = channel.stream;
    if(!stream) {
        return;
    }

    endTrackProcessors(channel.trackProcessors);
    channel.trackProcessors.processors.clear();

    if(m_trackProcessorFactories.empty()) {
        return;
    }

    const Track track = stream->track();
    for(const auto& factory : m_trackProcessorFactories) {
        if(!factory) {
            continue;
        }

        auto processor = factory();
        if(!processor) {
            continue;
        }

        processor->init(track, stream->format());
        channel.trackProcessors.processors.push_back(std::move(processor));
    }

    if(channel.trackProcessors.processors.empty()) {
        return;
    }

    channel.trackProcessors.track         = track;
    channel.trackProcessors.trackRevision = stream->trackRevision();
    channel.trackProcessors.hasTrack      = true;
}

AudioMixer::StreamTrackProcessorState* AudioMixer::ensureStreamTrackProcessors(MixerChannel& channel)
{
    auto& stream = channel.stream;
    if(!stream || m_trackProcessorFactories.empty()) {
        return nullptr;
    }

    auto& state = channel.trackProcessors;
    if(state.processors.empty()) {
        rebuildStreamTrackProcessors(channel);
        if(state.processors.empty()) {
            return nullptr;
        }
    }

    const uint64_t revision = stream->trackRevision();

    if(!state.hasTrack || state.trackRevision != revision) {
        const Track streamTrack = stream->track();
        endTrackProcessors(state);
        for(auto& processor : state.processors) {
            if(processor) {
                processor->init(streamTrack, stream->format());
            }
        }

        state.track         = streamTrack;
        state.trackRevision = revision;
        state.hasTrack      = true;
    }

    return &state;
}

AudioMixer::MixerChannel* AudioMixer::findChannel(StreamId id)
{
    if(id == InvalidStreamId) {
        return nullptr;
    }

    const auto it = std::ranges::find_if(
        m_channels, [id](const MixerChannel& channel) { return channel.stream && channel.stream->id() == id; });
    return (it != m_channels.end()) ? &(*it) : nullptr;
}

const AudioMixer::MixerChannel* AudioMixer::findChannel(StreamId id) const
{
    if(id == InvalidStreamId) {
        return nullptr;
    }

    const auto it = std::ranges::find_if(
        m_channels, [id](const MixerChannel& channel) { return channel.stream && channel.stream->id() == id; });
    return (it != m_channels.end()) ? &(*it) : nullptr;
}

void AudioMixer::addStream(AudioStreamPtr stream)
{
    addStream(std::move(stream), {});
}

void AudioMixer::addStream(AudioStreamPtr stream, std::vector<DspNodePtr> perTrackNodes)
{
    if(!stream) {
        return;
    }

    MixerChannel channel;
    channel.stream = std::move(stream);

    if(!perTrackNodes.empty()) {
        for(auto& node : perTrackNodes) {
            node->prepare(channel.stream->format());
        }
        channel.dspLatencySeconds = calculateDspLatencySeconds(perTrackNodes, channel.stream->format());
        channel.perTrackDsps      = std::move(perTrackNodes);
    }

    rebuildStreamTrackProcessors(channel);
    m_channels.push_back(std::move(channel));
}

void AudioMixer::removeStream(const AudioStreamPtr& stream)
{
    if(!stream) {
        return;
    }

    removeStream(stream->id());
}

void AudioMixer::removeStream(StreamId id)
{
    if(id == InvalidStreamId) {
        return;
    }

    std::erase_if(m_channels, [id](MixerChannel& channel) {
        if(!channel.stream || channel.stream->id() != id) {
            return false;
        }
        endTrackProcessors(channel.trackProcessors);
        return true;
    });
}

void AudioMixer::clear()
{
    for(auto& channel : m_channels) {
        endTrackProcessors(channel.trackProcessors);
    }
    m_channels.clear();
    m_outputLeftover.clear();
    m_outputTimeline.clear();
}

bool AudioMixer::hasStream(StreamId id) const
{
    return findChannel(id) != nullptr;
}

AudioStreamPtr AudioMixer::primaryStream() const
{
    for(const auto& channel : m_channels) {
        const auto& stream = channel.stream;
        if(!stream) {
            continue;
        }

        const auto state = stream->state();

        if(state != AudioStream::State::FadingOut && state != AudioStream::State::Stopped) {
            return stream;
        }
    }

    // Fall back to first stream if all are fading out
    for(const auto& channel : m_channels) {
        const auto& stream = channel.stream;
        if(stream) {
            return stream;
        }
    }

    return nullptr;
}

size_t AudioMixer::streamCount() const
{
    return m_channels.size();
}

void AudioMixer::switchTo(AudioStreamPtr newStream)
{
    switchTo(std::move(newStream), {});
}

void AudioMixer::switchTo(AudioStreamPtr newStream, std::vector<DspNodePtr> perTrackNodes)
{
    if(!newStream) {
        return;
    }

    for(auto& channel : m_channels) {
        auto& stream = channel.stream;
        if(stream) {
            stream->applyCommand(AudioStream::Command::Stop);
        }
    }

    for(auto& channel : m_channels) {
        endTrackProcessors(channel.trackProcessors);
    }
    m_channels.clear();
    m_outputLeftover.clear();
    m_outputTimeline.clear();

    newStream->applyCommand(AudioStream::Command::Play);
    addStream(std::move(newStream), std::move(perTrackNodes));
}

void AudioMixer::rebuildAllPerTrackDsps(const std::function<std::vector<DspNodePtr>()>& factory)
{
    for(auto& channel : m_channels) {
        channel.perTrackDsps.clear();
        channel.dspLatencySeconds = 0.0;
    }

    if(!factory) {
        return;
    }

    for(auto& channel : m_channels) {
        if(!channel.stream) {
            continue;
        }

        auto nodes = factory();

        if(!nodes.empty()) {
            for(auto& node : nodes) {
                node->prepare(channel.stream->format());
            }

            channel.dspLatencySeconds = calculateDspLatencySeconds(nodes, channel.stream->format());
            channel.perTrackDsps      = std::move(nodes);
        }
    }
}

void AudioMixer::pauseAll()
{
    for(auto& channel : m_channels) {
        if(channel.stream && channel.stream->state() == AudioStream::State::Playing) {
            channel.stream->applyCommand(AudioStream::Command::Pause);
        }
    }
}

void AudioMixer::resumeAll()
{
    for(auto& channel : m_channels) {
        if(channel.stream && channel.stream->state() == AudioStream::State::Paused) {
            channel.stream->applyCommand(AudioStream::Command::Play);
        }
    }
}

int AudioMixer::read(ProcessingBuffer& output, int frames)
{
    if(frames <= 0 || !output.isValid() || output.format().sampleFormat() != SampleFormat::F64) {
        return 0;
    }

    const AudioFormat outFormat = m_outputFormat.isValid() ? m_outputFormat : m_format;
    const int outChannels       = outFormat.channelCount();
    const int outRate           = outFormat.sampleRate();

    if(outChannels <= 0 || outRate <= 0) {
        return 0;
    }

    ReadPlan plan;
    plan.outChannels      = outChannels;
    plan.outRate          = outRate;
    plan.requestedSamples = static_cast<size_t>(frames) * static_cast<size_t>(outChannels);

    // If leftover covers this request, drain it
    if(m_outputLeftover.size() >= plan.requestedSamples) {
        return drainOutput(output, plan);
    }

    if(m_channels.empty()) {
        return 0;
    }

    const int leftoverFrames = static_cast<int>(m_outputLeftover.size()) / outChannels;
    const int framesNeeded   = std::max(0, frames - leftoverFrames);

    const auto categorization = categorizeStreams();
    plan.crossfadeActive      = categorization.activeMixingStreams > 1;
    plan.processFrames        = std::max(framesNeeded, MinProcessFrames);
    if(plan.processFrames <= 0) {
        return 0;
    }

    const size_t maxInputSamples = calculateMaxInputSamples(outRate, plan.processFrames);

    prepareMixBuffers(plan, maxInputSamples);
    warmPendingStreams(plan, categorization);

    auto cycleState = mixActiveStreams(plan, categorization.pendingStreamToStartId);
    handleGaplessTransition(plan, categorization.pendingStreamToStartId, cycleState);
    commitToOutput(plan, categorization.pendingStreamToStartId, cycleState);

    return drainOutput(output, plan);
}

bool AudioMixer::isMixingState(AudioStream::State state)
{
    return state != AudioStream::State::Stopped && state != AudioStream::State::Pending
        && state != AudioStream::State::Paused;
}

AudioMixer::StreamCategorization AudioMixer::categorizeStreams() const
{
    StreamCategorization categorization;

    for(const auto& channel : m_channels) {
        const auto& stream = channel.stream;
        if(!stream) {
            continue;
        }

        const auto state = stream->state();
        if(state == AudioStream::State::Pending) {
            categorization.pendingStreamToStartId = stream->id();
        }

        if(!isMixingState(state)) {
            continue;
        }

        ++categorization.activeMixingStreams;
    }

    for(const auto& channel : m_channels) {
        const auto& stream = channel.stream;
        if(!stream || stream->id() == categorization.pendingStreamToStartId) {
            continue;
        }

        if(isMixingState(stream->state())) {
            categorization.haveNonPendingActiveStream = true;
            break;
        }
    }

    return categorization;
}

size_t AudioMixer::calculateMaxInputSamples(const int outRate, const int processFrames) const
{
    size_t maxInputSamples{0};

    for(const auto& channel : m_channels) {
        const auto& stream = channel.stream;
        if(!stream || !isMixingState(stream->state())) {
            continue;
        }

        const int streamChannels = stream->channelCount();
        const int streamRate     = stream->sampleRate();
        if(streamChannels <= 0 || streamRate <= 0) {
            continue;
        }

        int inputFramesForRead = std::max(1, processFrames);
        if(streamRate != outRate) {
            const double scaled
                = (static_cast<double>(processFrames) * static_cast<double>(streamRate)) / static_cast<double>(outRate);
            inputFramesForRead = std::max(1, static_cast<int>(std::ceil(scaled)) + 1);
        }

        const size_t inputSamples = static_cast<size_t>(inputFramesForRead) * static_cast<size_t>(streamChannels);
        maxInputSamples           = std::max(maxInputSamples, inputSamples);
    }

    return maxInputSamples;
}

void AudioMixer::prepareMixBuffers(const ReadPlan& plan, const size_t maxInputSamples)
{
    // Ensure output mix buffer is sized for processFrames
    const auto mixOutSamples = static_cast<size_t>(plan.processFrames) * static_cast<size_t>(plan.outChannels);
    if(m_scratch.outputSamples.size() < mixOutSamples) {
        m_scratch.outputSamples.resize(mixOutSamples);
    }
    std::fill_n(m_scratch.outputSamples.data(), mixOutSamples, 0.0);

    // Temp per-stream buffer (post-DSP, outChannels).
    if(m_scratch.streamTemp.size() < mixOutSamples) {
        m_scratch.streamTemp.resize(mixOutSamples);
    }

    if(maxInputSamples > 0 && m_scratch.mixSamples.size() < maxInputSamples) {
        m_scratch.mixSamples.resize(maxInputSamples);
    }
}

int AudioMixer::mixSingleStream(MixerChannel& channel, const ReadPlan& plan, const int targetFrames,
                                const int outputFrameOffset, const bool consumeOutput, const bool captureTimeline,
                                std::vector<TimelineRange>* capturedTimeline)
{
    auto& stream = channel.stream;
    if(!stream || targetFrames <= 0 || outputFrameOffset < 0) {
        return 0;
    }

    const int streamChannels = stream->channelCount();
    const int streamRate     = stream->sampleRate();
    if(streamChannels <= 0 || streamRate <= 0) {
        return 0;
    }

    auto& fifo = channel.dspFifo;
    if(fifo.channels != 0 && fifo.channels != plan.outChannels) {
        fifo.buffer.clear();
        clearTimeline(fifo);
        fifo.channels = plan.outChannels;
    }
    if(fifo.channels == 0) {
        fifo.channels = plan.outChannels;
    }

    double& inputFrameAccumulator = channel.inputFrameAccumulator;
    int inputFramesPerRead        = std::max(1, targetFrames);

    if(streamRate != plan.outRate) {
        const double inputFramesExact
            = (static_cast<double>(targetFrames) * static_cast<double>(streamRate) / static_cast<double>(plan.outRate))
            + inputFrameAccumulator;
        inputFramesPerRead    = std::max(1, static_cast<int>(inputFramesExact));
        inputFrameAccumulator = inputFramesExact - static_cast<double>(inputFramesPerRead);
    }

    int maxTopUpIters{MaxTopUpIters};
    if(channel.dspLatencySeconds > 0.0 && inputFramesPerRead > 0) {
        const int latencyFrames = static_cast<int>(std::ceil(channel.dspLatencySeconds * streamRate));
        const int extraIters    = ((latencyFrames + inputFramesPerRead - 1) / inputFramesPerRead) + 1;
        maxTopUpIters += extraIters;
    }

    int iters{0};
    while(fifoFrames(fifo) < targetFrames && iters++ < maxTopUpIters) {
        const auto inSamples = static_cast<size_t>(inputFramesPerRead) * static_cast<size_t>(streamChannels);
        if(m_scratch.mixSamples.size() < inSamples) {
            m_scratch.mixSamples.resize(inSamples);
        }

        auto* mixData                     = m_scratch.mixSamples.data();
        const uint64_t inputSourceStartNs = stream->positionMs() * Time::NsPerMs;
        const int framesRead              = stream->read(mixData, inputFramesPerRead);

        if(framesRead <= 0) {
            if(stream->isEndOfStream()) {
                if(!fifo.endFlushed) {
                    fifo.endFlushed = true;

                    if(!channel.perTrackDsps.empty()) {
                        ProcessingBufferList tailChunks;
                        for(auto& node : channel.perTrackDsps) {
                            if(node) {
                                node->flush(tailChunks, DspNode::FlushMode::EndOfTrack);
                            }
                        }

                        if(tailChunks.count() > 0) {
                            for(auto& node : channel.perTrackDsps) {
                                if(node) {
                                    node->process(tailChunks);
                                }
                            }
                        }

                        bool tailIncompatible{false};
                        for(size_t i{0}; i < tailChunks.count(); ++i) {
                            const auto* chunk = tailChunks.item(i);
                            if(!chunk || !chunk->isValid()) {
                                continue;
                            }

                            const int chunkChannels = chunk->format().channelCount();
                            if(chunkChannels != plan.outChannels) {
                                tailIncompatible = true;
                                break;
                            }

                            const auto chunkData  = chunk->constData();
                            const int chunkFrames = chunk->frameCount();
                            if(chunkFrames <= 0
                               || chunkData.size()
                                      < static_cast<size_t>(chunkFrames) * static_cast<size_t>(chunkChannels)) {
                                continue;
                            }

                            fifoAppend(fifo, chunkData.data(), chunkFrames, chunkChannels);

                            const int chunkRate = chunk->format().sampleRate();
                            if(chunkRate > 0) {
                                appendTimelineSegment(fifo.timeline,
                                                      TimelineSegment{.valid           = true,
                                                                      .startNs         = chunk->startTimeNs(),
                                                                      .frameDurationNs = frameDurationNs(chunkRate),
                                                                      .frames          = chunkFrames});
                            }
                            else {
                                appendUnknownTimeline(fifo.timeline, chunkFrames);
                            }
                        }

                        if(tailIncompatible) {
                            stream->applyCommand(AudioStream::Command::Stop);
                            break;
                        }
                    }
                }

                if(fifoFrames(fifo) == 0) {
                    stream->applyCommand(AudioStream::Command::Stop);
                }
            }
            break;
        }

        fifo.endFlushed = false;

        const auto result = processPerTrackDsps(channel, mixData, framesRead, inputSourceStartNs);
        if(result.data && result.frames > 0) {
            if(result.channels != plan.outChannels) {
                stream->applyCommand(AudioStream::Command::Stop);
                fifo.buffer.clear();
                clearTimeline(fifo);
                break;
            }
            fifoAppend(fifo, result.data, result.frames, plan.outChannels);

            int timelineFrames{0};
            for(const auto& segment : result.timelineSegments) {
                appendTimelineSegment(fifo.timeline, segment);
                timelineFrames += segment.frames;
            }

            if(timelineFrames < result.frames) {
                appendUnknownTimeline(fifo.timeline, result.frames - timelineFrames);
            }
        }
    }

    if(!consumeOutput) {
        return 0;
    }

    const int gotFrames = fifoConsume(fifo, m_scratch.streamTemp.data(), targetFrames, plan.outChannels);
    if(gotFrames <= 0) {
        return 0;
    }

    if(captureTimeline && capturedTimeline) {
        consumeTimeline(fifo, gotFrames, m_scratch.consumedTimeline);
        int timelineOffset{0};

        for(const auto& segment : m_scratch.consumedTimeline) {
            if(segment.frames <= 0) {
                continue;
            }

            capturedTimeline->push_back(TimelineRange{
                .outputOffsetFrames = outputFrameOffset + timelineOffset,
                .segment            = segment,
            });
            timelineOffset += segment.frames;
        }
    }
    else {
        discardTimeline(fifo, gotFrames);
    }

    const auto fadeRamp     = stream->calculateFadeGainRamp(gotFrames, plan.outRate);
    const size_t baseOffset = static_cast<size_t>(outputFrameOffset) * static_cast<size_t>(plan.outChannels);
    const size_t gotSamples = static_cast<size_t>(gotFrames) * static_cast<size_t>(plan.outChannels);

    if(!fadeRamp.active || gotFrames <= 1 || std::abs(fadeRamp.endGain - fadeRamp.startGain) < 0.000001) {
        for(size_t i{0}; i < gotSamples; ++i) {
            m_scratch.outputSamples[baseOffset + i] += m_scratch.streamTemp[i] * fadeRamp.endGain;
        }
    }
    else {
        const double gainStep = (fadeRamp.endGain - fadeRamp.startGain) / static_cast<double>(gotFrames - 1);

        for(int frame{0}; frame < gotFrames; ++frame) {
            const double frameGain = std::clamp(fadeRamp.startGain + (gainStep * static_cast<double>(frame)), 0.0, 1.0);
            const size_t base      = baseOffset + (static_cast<size_t>(frame) * static_cast<size_t>(plan.outChannels));

            for(int ch{0}; ch < plan.outChannels; ++ch) {
                m_scratch.outputSamples[base + static_cast<size_t>(ch)]
                    += m_scratch.streamTemp[(static_cast<size_t>(frame) * static_cast<size_t>(plan.outChannels))
                                            + static_cast<size_t>(ch)]
                     * frameGain;
            }
        }
    }

    return gotFrames;
}

void AudioMixer::warmPendingStreams(const ReadPlan& plan, const StreamCategorization& categorization)
{
    // Warm pending stream DSP/output FIFO while current stream is still active
    // This attempts to avoid startup underruns from DSPs with internal delay
    if(categorization.pendingStreamToStartId == InvalidStreamId || !categorization.haveNonPendingActiveStream) {
        return;
    }

    if(auto* pendingChannel = findChannel(categorization.pendingStreamToStartId)) {
        mixSingleStream(*pendingChannel, plan, plan.processFrames, 0, false);
    }
}

AudioMixer::ReadCycleState AudioMixer::mixActiveStreams(const ReadPlan& plan, StreamId pendingStreamToStartId)
{
    ReadCycleState cycleState;
    m_scratch.trackedTimeline.clear();

    for(size_t i{0}; i < m_channels.size(); ++i) {
        auto& channel = m_channels[i];
        auto& stream  = channel.stream;

        if(!stream) {
            continue;
        }

        if(cycleState.splicedPendingStreamId != InvalidStreamId && stream->id() == cycleState.splicedPendingStreamId) {
            if(cycleState.writeIndex != i) {
                m_channels[cycleState.writeIndex] = std::move(channel);
            }
            ++cycleState.writeIndex;
            continue;
        }

        const auto state = stream->state();
        if(state == AudioStream::State::Stopped) {
            endTrackProcessors(channel.trackProcessors);
            continue;
        }

        if(state == AudioStream::State::Pending || state == AudioStream::State::Paused) {
            if(cycleState.writeIndex != i) {
                m_channels[cycleState.writeIndex] = std::move(channel);
            }
            ++cycleState.writeIndex;
            continue;
        }

        cycleState.hasActiveStream = true;

        const int streamRateForCycle = stream->sampleRate();
        const bool trackPrimary      = !cycleState.hasTrackedPrimary && state != AudioStream::State::FadingOut
                               && stream->id() != pendingStreamToStartId;
        const int gotFrames = mixSingleStream(channel, plan, plan.processFrames, 0, true, trackPrimary,
                                              trackPrimary ? &m_scratch.trackedTimeline : nullptr);

        if(trackPrimary && gotFrames > 0) {
            cycleState.hasTrackedPrimary = true;
        }

        if(gotFrames > 0) {
            cycleState.producedFrames = std::max(cycleState.producedFrames, gotFrames);
        }
        else if(!channel.perTrackDsps.empty() && streamRateForCycle != plan.outRate && !stream->isEndOfStream()) {
            cycleState.rateConversionWarmup = true;
        }

        if(gotFrames < plan.processFrames && pendingStreamToStartId != InvalidStreamId && stream->isEndOfStream()) {
            if(auto* pendingChannel = findChannel(pendingStreamToStartId);
               pendingChannel && pendingChannel->stream && pendingChannel->stream != stream) {
                auto& pendingStream = pendingChannel->stream;

                if(pendingStream->state() == AudioStream::State::Pending) {
                    pendingStream->applyCommand(AudioStream::Command::Play);
                    cycleState.startedPendingStream = true;
                }

                const int tailFrames    = plan.processFrames - gotFrames;
                const bool trackPending = !cycleState.hasTrackedPrimary;
                const int tailMixed = mixSingleStream(*pendingChannel, plan, tailFrames, gotFrames, true, trackPending,
                                                      trackPending ? &m_scratch.trackedTimeline : nullptr);

                if(trackPending && tailMixed > 0) {
                    cycleState.hasTrackedPrimary = true;
                }

                if(tailMixed > 0) {
                    cycleState.producedFrames         = std::max(cycleState.producedFrames, gotFrames + tailMixed);
                    cycleState.splicedPendingStreamId = pendingStream->id();
                }
            }
        }

        if(stream->state() != AudioStream::State::Stopped) {
            if(cycleState.writeIndex != i) {
                m_channels[cycleState.writeIndex] = std::move(channel);
            }
            ++cycleState.writeIndex;
        }
        else {
            endTrackProcessors(channel.trackProcessors);
        }
    }

    pruneStoppedStreams(cycleState.writeIndex);
    return cycleState;
}

void AudioMixer::handleGaplessTransition(const ReadPlan& plan, StreamId pendingStreamToStartId,
                                         ReadCycleState& cycleState)
{
    // Gapless: if the active stream ended and only pending streams remain,
    // promote the next pending stream immediately on the audio thread
    if(cycleState.hasActiveStream || pendingStreamToStartId == InvalidStreamId) {
        return;
    }

    if(auto* pendingChannel = findChannel(pendingStreamToStartId); pendingChannel && pendingChannel->stream) {
        auto& pendingStream = pendingChannel->stream;
        pendingStream->applyCommand(AudioStream::Command::Play);
        cycleState.startedPendingStream = true;

        // If the previous stream ended exactly on a block boundary, ensure this
        // cycle still returns audio by mixing the promoted stream
        const bool trackPending = !cycleState.hasTrackedPrimary;
        const int handoffFrames = mixSingleStream(*pendingChannel, plan, plan.processFrames, 0, true, trackPending,
                                                  trackPending ? &m_scratch.trackedTimeline : nullptr);
        if(trackPending && handoffFrames > 0) {
            cycleState.hasTrackedPrimary = true;
        }
        if(handoffFrames > 0) {
            cycleState.producedFrames = std::max(cycleState.producedFrames, handoffFrames);
        }
    }
}

void AudioMixer::pruneStoppedStreams(size_t writeIndex)
{
    m_channels.resize(writeIndex);
}

void AudioMixer::commitToOutput(const ReadPlan& plan, StreamId pendingStreamToStartId, const ReadCycleState& cycleState)
{
    // Append mixed output only when we actually mixed active stream data
    if(cycleState.producedFrames > 0) {
        const size_t producedSamples
            = static_cast<size_t>(cycleState.producedFrames) * static_cast<size_t>(plan.outChannels);
        m_outputLeftover.push(m_scratch.outputSamples.data(), producedSamples);

        if(m_scratch.trackedTimeline.empty()) {
            appendUnknownTimeline(m_outputTimeline, cycleState.producedFrames);
        }
        else {
            std::ranges::sort(m_scratch.trackedTimeline, [](const TimelineRange& lhs, const TimelineRange& rhs) {
                return lhs.outputOffsetFrames < rhs.outputOffsetFrames;
            });

            int cursor{0};
            for(const auto& range : m_scratch.trackedTimeline) {
                const int clampedOffset = std::clamp(range.outputOffsetFrames, 0, cycleState.producedFrames);
                if(clampedOffset > cursor) {
                    appendUnknownTimeline(m_outputTimeline, clampedOffset - cursor);
                    cursor = clampedOffset;
                }

                if(cursor >= cycleState.producedFrames || range.segment.frames <= 0) {
                    continue;
                }

                const int maxSegmentFrames = cycleState.producedFrames - cursor;
                const int segmentFrames    = std::min(range.segment.frames, maxSegmentFrames);
                if(segmentFrames <= 0) {
                    continue;
                }

                TimelineSegment segment = range.segment;
                segment.frames          = segmentFrames;
                appendTimelineSegment(m_outputTimeline, segment);
                cursor += segmentFrames;
            }

            if(cursor < cycleState.producedFrames) {
                appendUnknownTimeline(m_outputTimeline, cycleState.producedFrames - cursor);
            }
        }
    }

    // If active streams exist but we still can't satisfy request, pad with silence
    if(!cycleState.startedPendingStream && pendingStreamToStartId == InvalidStreamId && cycleState.hasActiveStream
       && cycleState.producedFrames > 0 && m_outputLeftover.size() < plan.requestedSamples
       && !cycleState.rateConversionWarmup) {
        const size_t padSamples = plan.requestedSamples - m_outputLeftover.size();
        const int padFrames     = static_cast<int>(padSamples / static_cast<size_t>(std::max(1, plan.outChannels)));
        m_outputLeftover.pushZeros(padSamples);
        appendUnknownTimeline(m_outputTimeline, padFrames);
    }
}

int AudioMixer::drainOutput(ProcessingBuffer& output, const ReadPlan& plan)
{
    const size_t availableSamples = std::min(m_outputLeftover.size(), plan.requestedSamples);
    const int returnedFrames      = static_cast<int>(availableSamples / static_cast<size_t>(plan.outChannels));
    const size_t returnedSamples  = static_cast<size_t>(returnedFrames) * static_cast<size_t>(plan.outChannels);

    output.resizeSamples(returnedSamples);
    auto outSpan = output.data();
    m_outputLeftover.pop(outSpan.data(), returnedSamples);

    if(returnedFrames > 0) {
        // Consume timeline metadata and stamp the output buffer with the source start time.
        // The pipeline reads this startTime off the coalesced output chunks to track position.
        consumeTimeline(m_outputTimeline, returnedFrames, m_scratch.consumedTimeline);
        uint64_t startNs{0};
        if(timelineStartNs(m_scratch.consumedTimeline, startNs)) {
            output.setStartTimeNs(startNs);
        }
        else {
            output.setStartTimeNs(0);
        }

        m_scratch.consumedTimeline.clear();
    }

    return returnedFrames;
}

bool AudioMixer::needsMoreData() const
{
    return std::ranges::any_of(m_channels, [](const MixerChannel& channel) {
        return channel.stream && channel.stream->isBufferLow() && !channel.stream->endOfInput();
    });
}

uint64_t AudioMixer::bufferedDurationMs() const
{
    auto stream = primaryStream();
    if(!stream) {
        return 0;
    }
    return stream->bufferedDurationMs();
}

int AudioMixer::primaryStreamDspLatencyFrames() const
{
    const auto stream = primaryStream();
    if(!stream || stream->sampleRate() <= 0) {
        return 0;
    }

    return static_cast<int>(std::llround(primaryStreamDspLatencySeconds() * static_cast<double>(stream->sampleRate())));
}

double AudioMixer::primaryStreamDspLatencySeconds() const
{
    const auto stream = primaryStream();
    if(!stream) {
        return 0.0;
    }

    const auto* channel = findChannel(stream->id());
    if(!channel) {
        return 0.0;
    }

    return std::max(0.0, channel->dspLatencySeconds);
}

int AudioMixer::fifoFrames(const StreamDspFifo& f) noexcept
{
    return (f.channels > 0) ? static_cast<int>(f.buffer.size() / static_cast<size_t>(f.channels)) : 0;
}

uint64_t AudioMixer::frameDurationNs(const int sampleRate)
{
    if(sampleRate <= 0) {
        return 0;
    }

    const double frameNs = static_cast<double>(Time::NsPerSecond) / static_cast<double>(sampleRate);
    const auto roundedNs = std::llround(frameNs);
    return static_cast<uint64_t>(std::max<int64_t>(1, roundedNs));
}

void AudioMixer::appendTimelineSegment(std::deque<TimelineSegment>& segments, const TimelineSegment& segment)
{
    if(segment.frames <= 0) {
        return;
    }

    if(segments.empty()) {
        segments.push_back(segment);
        return;
    }

    auto& tail = segments.back();
    if(!tail.valid && !segment.valid) {
        tail.frames += segment.frames;
        return;
    }

    if(tail.valid && segment.valid && tail.frameDurationNs == segment.frameDurationNs) {
        const uint64_t expectedStartNs
            = tail.startNs + (tail.frameDurationNs * static_cast<uint64_t>(std::max(0, tail.frames)));
        if(expectedStartNs == segment.startNs) {
            tail.frames += segment.frames;
            return;
        }
    }

    segments.push_back(segment);
}

void AudioMixer::appendUnknownTimeline(std::deque<TimelineSegment>& segments, const int frames)
{
    appendTimelineSegment(segments,
                          TimelineSegment{.valid = false, .startNs = 0, .frameDurationNs = 0, .frames = frames});
}

void AudioMixer::clearTimeline(StreamDspFifo& f)
{
    f.timeline.clear();
}

void AudioMixer::discardTimeline(std::deque<TimelineSegment>& segments, int frames)
{
    if(frames <= 0) {
        return;
    }

    while(frames > 0 && !segments.empty()) {
        auto& head     = segments.front();
        const int take = std::min(head.frames, frames);
        if(take <= 0) {
            segments.pop_front();
            continue;
        }

        if(head.valid && head.frameDurationNs > 0) {
            head.startNs += head.frameDurationNs * static_cast<uint64_t>(take);
        }
        head.frames -= take;
        frames -= take;
        if(head.frames <= 0) {
            segments.pop_front();
        }
    }
}

void AudioMixer::discardTimeline(StreamDspFifo& f, const int frames)
{
    discardTimeline(f.timeline, frames);
}

void AudioMixer::consumeTimeline(std::deque<TimelineSegment>& segments, int frames, std::vector<TimelineSegment>& out)
{
    out.clear();
    if(frames <= 0) {
        return;
    }

    while(frames > 0 && !segments.empty()) {
        auto& head     = segments.front();
        const int take = std::min(head.frames, frames);
        if(take <= 0) {
            segments.pop_front();
            continue;
        }

        TimelineSegment piece = head;
        piece.frames          = take;
        out.push_back(piece);

        if(head.valid && head.frameDurationNs > 0) {
            head.startNs += head.frameDurationNs * static_cast<uint64_t>(take);
        }
        head.frames -= take;
        frames -= take;

        if(head.frames <= 0) {
            segments.pop_front();
        }
    }

    if(frames > 0) {
        out.push_back(TimelineSegment{.valid = false, .startNs = 0, .frameDurationNs = 0, .frames = frames});
    }
}

void AudioMixer::consumeTimeline(StreamDspFifo& f, const int frames, std::vector<TimelineSegment>& out)
{
    consumeTimeline(f.timeline, frames, out);
}

bool AudioMixer::timelineStartNs(const std::vector<TimelineSegment>& segments, uint64_t& startNsOut)
{
    for(const auto& segment : segments) {
        if(!segment.valid) {
            continue;
        }
        startNsOut = segment.startNs;
        return true;
    }

    return false;
}

bool AudioMixer::timelineEndNs(const std::vector<TimelineSegment>& segments, uint64_t& endNsOut)
{
    for(const auto& segment : std::ranges::reverse_view(segments)) {
        if(!segment.valid || segment.frameDurationNs == 0 || segment.frames <= 0) {
            continue;
        }

        const uint64_t endNs = segment.startNs + (segment.frameDurationNs * static_cast<uint64_t>(segment.frames));
        endNsOut             = endNs;
        return true;
    }

    return false;
}

void AudioMixer::fifoAppend(StreamDspFifo& f, const double* data, int frames, int channels)
{
    if(!data || frames <= 0 || channels <= 0) {
        return;
    }

    if(f.channels == 0) {
        f.channels = channels;
    }
    else if(f.channels != channels) {
        return;
    }

    const size_t n = static_cast<size_t>(frames) * static_cast<size_t>(channels);
    f.buffer.push(data, n);
}

int AudioMixer::fifoConsume(StreamDspFifo& f, double* dst, int frames, int channels)
{
    if(frames <= 0 || channels <= 0) {
        return 0;
    }

    if(f.channels != channels) {
        return 0;
    }

    const size_t need = static_cast<size_t>(frames) * static_cast<size_t>(channels);
    const size_t take = f.buffer.pop(dst, need);
    return static_cast<int>(take / static_cast<size_t>(channels));
}

double AudioMixer::calculateDspLatencySeconds(const std::vector<DspNodePtr>& nodes, const AudioFormat& inputFormat)
{
    if(!inputFormat.isValid()) {
        return 0.0;
    }

    AudioFormat current{inputFormat};
    double total{0.0};

    for(const auto& node : nodes) {
        if(!node) {
            continue;
        }

        const int latencyFrames = std::max(0, node->latencyFrames());

        if(latencyFrames > 0 && current.sampleRate() > 0) {
            total += static_cast<double>(latencyFrames) / static_cast<double>(current.sampleRate());
        }
        current = node->outputFormat(current);
    }

    return total;
}

AudioMixer::PerTrackResult AudioMixer::processPerTrackDsps(MixerChannel& channel, double* inputData, int inputFrames,
                                                           uint64_t inputSourceStartNs)
{
    auto& stream = channel.stream;
    if(!stream || !inputData || inputFrames <= 0) {
        return {};
    }

    const int inChannels = stream->channelCount();
    if(inChannels <= 0) {
        return {};
    }

    const double* outputData = inputData;
    int outputFrames         = inputFrames;
    int outputChannels       = inChannels;
    int outputRate           = stream->sampleRate();
    m_scratch.perTrackTimeline.clear();

    // Run per-stream processors on raw stream samples before per-track DSPs.
    if(auto* state = ensureStreamTrackProcessors(channel)) {
        const auto sampleCount = static_cast<size_t>(inputFrames) * static_cast<size_t>(inChannels);
        for(auto& processor : state->processors) {
            if(processor) {
                processor->process(inputData, sampleCount);
            }
        }
    }

    const auto buildResult = [&]() -> PerTrackResult {
        return {.data             = outputData,
                .frames           = outputFrames,
                .channels         = outputChannels,
                .sampleRate       = outputRate,
                .timelineSegments = m_scratch.perTrackTimeline};
    };

    if(channel.perTrackDsps.empty()) {
        if(outputFrames > 0 && outputRate > 0) {
            m_scratch.perTrackTimeline.push_back(TimelineSegment{.valid           = true,
                                                                 .startNs         = inputSourceStartNs,
                                                                 .frameDurationNs = frameDurationNs(outputRate),
                                                                 .frames          = outputFrames});
        }
        else {
            m_scratch.perTrackTimeline.push_back(
                TimelineSegment{.valid = false, .startNs = 0, .frameDurationNs = 0, .frames = outputFrames});
        }
        return buildResult();
    }

    AudioFormat dspFormat = stream->format();
    dspFormat.setSampleFormat(SampleFormat::F64);

    const auto inSamples     = static_cast<size_t>(inputFrames) * inChannels;
    m_scratch.perTrackBuffer = ProcessingBuffer({inputData, inSamples}, dspFormat, inputSourceStartNs);
    m_scratch.perTrackChunks.setToSingle(m_scratch.perTrackBuffer);

    for(auto& node : channel.perTrackDsps) {
        node->process(m_scratch.perTrackChunks);
    }

    if(m_scratch.perTrackChunks.count() == 0) {
        // A DSP may output no chunk for a given input block while buffering
        // internal state. Falling back to raw input bypasses that DSP and can
        // produce invalid output
        return {};
    }

    if(m_scratch.perTrackChunks.count() == 1) {
        auto* chunk = m_scratch.perTrackChunks.item(0);

        if(!chunk || !chunk->isValid()) {
            return {};
        }

        auto processed = chunk->data();
        outputData     = processed.data();
        outputChannels = chunk->format().channelCount();
        outputRate     = chunk->format().sampleRate();
        outputFrames   = static_cast<int>(processed.size()) / std::max(1, outputChannels);
        if(outputChannels <= 0 || outputRate <= 0 || outputFrames <= 0) {
            return {};
        }

        m_scratch.perTrackTimeline.push_back(TimelineSegment{.valid           = true,
                                                             .startNs         = chunk->startTimeNs(),
                                                             .frameDurationNs = frameDurationNs(outputRate),
                                                             .frames          = outputFrames});
    }
    else {
        // Multiple chunks - coalesce into shared scratch vector
        m_scratch.perTrackCoalesced.clear();
        outputChannels = -1;
        outputRate     = -1;
        size_t expectedSamples{0};
        int timelineFrames{0};

        for(size_t i{0}; i < m_scratch.perTrackChunks.count(); ++i) {
            const auto* chunk = m_scratch.perTrackChunks.item(i);

            if(!chunk || !chunk->isValid()) {
                continue;
            }

            const int chunkChannels = chunk->format().channelCount();
            const int chunkRate     = chunk->format().sampleRate();
            if(chunkChannels <= 0) {
                continue;
            }

            if(outputChannels < 0) {
                outputChannels = chunkChannels;
                outputRate     = chunkRate;
            }
            else if(chunkChannels != outputChannels || chunkRate != outputRate) {
                // Mixed output formats across chunks cannot be
                // represented as a single interleaved stream.
                stream->applyCommand(AudioStream::Command::Stop);
                return {};
            }

            const auto data = chunk->constData();
            expectedSamples += data.size();

            const int chunkFrames = chunk->frameCount();
            if(chunkFrames <= 0) {
                continue;
            }

            m_scratch.perTrackTimeline.push_back(TimelineSegment{.valid           = true,
                                                                 .startNs         = chunk->startTimeNs(),
                                                                 .frameDurationNs = frameDurationNs(chunkRate),
                                                                 .frames          = chunkFrames});
            timelineFrames += chunkFrames;
        }

        if(outputChannels <= 0 || outputRate <= 0 || expectedSamples == 0) {
            return {};
        }

        m_scratch.perTrackCoalesced.reserve(expectedSamples);

        for(size_t i{0}; i < m_scratch.perTrackChunks.count(); ++i) {
            const auto* chunk = m_scratch.perTrackChunks.item(i);
            if(!chunk || !chunk->isValid()) {
                continue;
            }
            const auto data = chunk->constData();
            m_scratch.perTrackCoalesced.insert(m_scratch.perTrackCoalesced.end(), data.begin(), data.end());
        }

        if(m_scratch.perTrackCoalesced.empty()) {
            return {};
        }

        outputData   = m_scratch.perTrackCoalesced.data();
        outputFrames = static_cast<int>(m_scratch.perTrackCoalesced.size()) / std::max(1, outputChannels);

        if(timelineFrames < outputFrames) {
            m_scratch.perTrackTimeline.push_back(TimelineSegment{
                .valid           = false,
                .startNs         = 0,
                .frameDurationNs = 0,
                .frames          = outputFrames - timelineFrames,
            });
        }
    }

    return buildResult();
}
} // namespace Fooyin
