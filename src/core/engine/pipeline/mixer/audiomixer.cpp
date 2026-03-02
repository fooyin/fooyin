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

#include "mixeroutput.h"
#include "mixertimeline.h"

#include <core/engine/audioutils.h>
#include <utils/timeconstants.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iterator>
#include <limits>
#include <ranges>
#include <span>

constexpr auto MinTopUpDurationMs            = 25.0;
constexpr auto TopUpDurationMultiplier       = 2.0;
constexpr auto TopUpSafetyPaddingMs          = 25.0;
constexpr auto MaxTopUpDurationMs            = 3000.0;
constexpr auto BufferingReadGrowthFactor     = 2;
constexpr auto BufferingReadMaxDurationMs    = 750;
constexpr auto BufferingReadMaxFramesHardCap = 131072;
constexpr auto MaxTopUpItersHardCap          = 4096;
constexpr auto MaxBufferingReadsPerCycle     = 8;

namespace {
uint64_t saturatingAddU64(uint64_t lhs, uint64_t rhs)
{
    if(lhs > (std::numeric_limits<uint64_t>::max() - rhs)) {
        return std::numeric_limits<uint64_t>::max();
    }
    return lhs + rhs;
}

Fooyin::AudioFormat normalisePrepareFormat(Fooyin::AudioFormat format)
{
    if(format.isValid()) {
        format.setSampleFormat(Fooyin::SampleFormat::F64);
    }
    return format;
}
} // namespace

namespace Fooyin {
void AudioMixer::PerTrackInflightEstimator::resetBoundary()
{
    pendingNs = 0;
    remIn     = 0;
    remOut    = 0;
}

void AudioMixer::PerTrackInflightEstimator::update(uint64_t consumedInputFrames, int inputRate,
                                                   uint64_t producedOutputFrames, int outputRate)
{
    const uint64_t inNs  = accumulateNsWithRemainder(consumedInputFrames, inputRate, remIn);
    const uint64_t outNs = accumulateNsWithRemainder(producedOutputFrames, outputRate, remOut);

    pendingNs = saturatingAddU64(pendingNs, inNs);
    if(outNs > pendingNs) {
        pendingNs = 0;
        return;
    }

    pendingNs -= outNs;
}

uint64_t AudioMixer::PerTrackInflightEstimator::inflightFramesAt(int outputSampleRate) const
{
    if(outputSampleRate <= 0) {
        return 0;
    }

    const auto rate          = static_cast<uint64_t>(outputSampleRate);
    const uint64_t wholeSecs = pendingNs / Time::NsPerSecond;
    const uint64_t remNs     = pendingNs % Time::NsPerSecond;

    if(wholeSecs > (std::numeric_limits<uint64_t>::max() / std::max<uint64_t>(1ULL, rate))) {
        return std::numeric_limits<uint64_t>::max();
    }

    const uint64_t wholeFrames = wholeSecs * rate;
    const uint64_t remNumer    = remNs * rate;
    const uint64_t remFrames   = remNumer == 0 ? 0 : ((remNumer + Time::NsPerSecond - 1ULL) / Time::NsPerSecond);
    return saturatingAddU64(wholeFrames, remFrames);
}

uint64_t AudioMixer::PerTrackInflightEstimator::accumulateNsWithRemainder(uint64_t frames, int sampleRate,
                                                                          uint64_t& remainder)
{
    if(frames == 0 || sampleRate <= 0) {
        return 0;
    }

    const auto rate  = static_cast<uint64_t>(sampleRate);
    const uint64_t q = frames / rate;
    const uint64_t r = frames % rate;

    const uint64_t wholeNs = q > (std::numeric_limits<uint64_t>::max() / Time::NsPerSecond)
                               ? std::numeric_limits<uint64_t>::max()
                               : q * Time::NsPerSecond;

    uint64_t carryNumer = remainder;
    if(r > 0) {
        const uint64_t maxBeforeMul = (std::numeric_limits<uint64_t>::max() - carryNumer) / Time::NsPerSecond;
        carryNumer = r > maxBeforeMul ? std::numeric_limits<uint64_t>::max() : carryNumer + (r * Time::NsPerSecond);
    }

    const uint64_t carryWhole = carryNumer / rate;
    remainder                 = carryNumer % rate;
    return saturatingAddU64(wholeNs, carryWhole);
}

AudioMixer::AudioMixer(const AudioFormat& format)
    : m_format{format}
    , m_output{std::make_unique<MixerOutput>()}
{
    AudioFormat mixFormat = format.isValid() ? format : AudioFormat{SampleFormat::F64, 48000, 2};
    mixFormat.setSampleFormat(SampleFormat::F64);

    const auto bufferSamples = static_cast<size_t>(mixFormat.sampleRate()) * mixFormat.channelCount();
    m_scratch.outputSamples.resize(bufferSamples);
    m_scratch.mixSamples.resize(bufferSamples);
}

AudioMixer::~AudioMixer() = default;

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
    if(Audio::inputFormatsMatch(m_outputFormat, format)) {
        m_outputFormat = format;
        return;
    }

    m_outputFormat = format;

    for(auto& channel : m_channels) {
        resetChannelRuntimeState(channel, ResetMode::OutputFormatChange);
    }

    m_output->clear();

    if(format.isValid()) {
        const auto outputSamples = static_cast<size_t>(format.sampleRate()) * format.channelCount();
        m_scratch.outputSamples.resize(outputSamples);
    }
}

void AudioMixer::resetBuffers()
{
    m_output->clear();

    for(auto& channel : m_channels) {
        resetChannelRuntimeState(channel, ResetMode::RuntimeReset);
    }
}

void AudioMixer::resetStreamForSeek(const StreamId id)
{
    if(id == InvalidStreamId) {
        return;
    }

    m_output->clear();

    for(auto& channel : m_channels) {
        if(!channel.stream || channel.stream->id() != id) {
            continue;
        }

        resetChannelRuntimeState(channel, ResetMode::SeekReset);
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

void AudioMixer::resetChannelRuntimeState(MixerChannel& channel, const ResetMode mode)
{
    switch(mode) {
        case ResetMode::OutputFormatChange:
            channel.dspFifo.stage.clearPendingOutput();
            channel.dspFifo.endFlushed    = false;
            channel.inputFrameAccumulator = 0.0;
            channel.inflightEstimator.resetBoundary();
            break;
        case ResetMode::RuntimeReset:
            channel.dspFifo.stage.clearPendingOutput();
            channel.dspFifo.endFlushed    = false;
            channel.inputFrameAccumulator = 0.0;
            channel.dspLatencySeconds     = 0.0;
            channel.inflightEstimator.resetBoundary();
            break;
        case ResetMode::SeekReset:
            channel.dspFifo.stage.clearPendingOutput();
            channel.dspFifo.endFlushed    = false;
            channel.inputFrameAccumulator = 0.0;
            channel.dspLatencySeconds     = 0.0;
            channel.inflightEstimator.resetBoundary();
            for(auto& node : channel.perTrackDsps) {
                if(node) {
                    (*node).reset();
                }
            }
            break;
        case ResetMode::RebuildPerTrack:
            channel.dspFifo.stage.clearPendingOutput();
            channel.dspFifo.endFlushed    = false;
            channel.inputFrameAccumulator = 0.0;
            channel.perTrackDsps.clear();
            channel.dspLatencySeconds = 0.0;
            channel.inflightEstimator.resetBoundary();
            break;
    }
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
            node->prepare(normalisePrepareFormat(channel.stream->format()));
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
    m_output->clear();

    std::vector<double>{}.swap(m_scratch.outputSamples);
    std::vector<double>{}.swap(m_scratch.mixSamples);
    std::vector<double>{}.swap(m_scratch.streamTemp);
    std::vector<double>{}.swap(m_scratch.perTrackCoalesced);
    std::vector<TimelineRange>{}.swap(m_scratch.trackedTimeline);
    std::vector<TimelineSegment>{}.swap(m_scratch.outputTimeline);
    std::vector<TimedAudioFifo::TimelineChunk>{}.swap(m_scratch.perTrackTimeline);
    std::vector<TimelineSegment>{}.swap(m_scratch.consumedTimeline);
    m_scratch.perTrackBuffer = {};
    m_scratch.perTrackChunks = {};
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

size_t AudioMixer::mixingStreamCount() const
{
    size_t count{0};

    for(const auto& channel : m_channels) {
        const auto& stream = channel.stream;
        if(stream && isMixingState(stream->state())) {
            ++count;
        }
    }

    return count;
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
    m_output->clear();

    newStream->applyCommand(AudioStream::Command::Play);
    addStream(std::move(newStream), std::move(perTrackNodes));
}

void AudioMixer::rebuildAllPerTrackDsps(const std::function<std::vector<DspNodePtr>()>& factory,
                                        const bool preserveBufferedOutput)
{
    for(auto& channel : m_channels) {
        if(preserveBufferedOutput) {
            channel.perTrackDsps.clear();
            channel.dspLatencySeconds = 0.0;
        }
        else {
            resetChannelRuntimeState(channel, ResetMode::RebuildPerTrack);
        }
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
                node->prepare(normalisePrepareFormat(channel.stream->format()));
            }

            channel.dspLatencySeconds = calculateDspLatencySeconds(nodes, channel.stream->format());
            channel.perTrackDsps      = std::move(nodes);
            if(!preserveBufferedOutput) {
                channel.inflightEstimator.resetBoundary();
            }
        }
    }
}

bool AudioMixer::canApplyIncrementalPerTrackPatch(const PerTrackPatchPlan& patchPlan) const
{
    if(!patchPlan.hasChanges) {
        return true;
    }

    for(const auto& channel : m_channels) {
        if(!channel.stream) {
            continue;
        }

        const size_t existingCount = channel.perTrackDsps.size();
        if(patchPlan.prefixKeep > existingCount) {
            return false;
        }

        const size_t removable = existingCount - patchPlan.prefixKeep;
        if(patchPlan.oldMiddleCount > removable) {
            return false;
        }
    }

    return true;
}

bool AudioMixer::applyIncrementalPerTrackPatch(const PerTrackPatchPlan& patchPlan,
                                               const std::function<std::vector<DspNodePtr>()>& middleFactory,
                                               const bool preserveBufferedOutput)
{
    if(!patchPlan.hasChanges) {
        return true;
    }

    if(!middleFactory || !canApplyIncrementalPerTrackPatch(patchPlan)) {
        return false;
    }

    for(auto& channel : m_channels) {
        if(!channel.stream) {
            continue;
        }

        auto replacementNodes = middleFactory();
        if(replacementNodes.size() != patchPlan.newMiddleCount) {
            return false;
        }

        const size_t replaceStart = patchPlan.prefixKeep;
        const auto eraseBegin     = channel.perTrackDsps.begin() + static_cast<ptrdiff_t>(replaceStart);
        const auto eraseEnd       = eraseBegin + static_cast<ptrdiff_t>(patchPlan.oldMiddleCount);
        channel.perTrackDsps.erase(eraseBegin, eraseEnd);

        const auto insertPos = channel.perTrackDsps.begin() + static_cast<ptrdiff_t>(replaceStart);
        channel.perTrackDsps.insert(insertPos, std::make_move_iterator(replacementNodes.begin()),
                                    std::make_move_iterator(replacementNodes.end()));

        AudioFormat current = channel.stream->format();
        for(size_t i{0}; i < replaceStart; ++i) {
            const auto& node = channel.perTrackDsps[i];
            if(node) {
                current = node->outputFormat(current);
            }
        }

        for(size_t i{replaceStart}; i < channel.perTrackDsps.size(); ++i) {
            const auto& node = channel.perTrackDsps[i];
            if(!node) {
                continue;
            }

            node->prepare(normalisePrepareFormat(current));
            current = node->outputFormat(current);
        }

        channel.dspLatencySeconds = calculateDspLatencySeconds(channel.perTrackDsps, channel.stream->format());

        if(!preserveBufferedOutput) {
            channel.inflightEstimator.resetBoundary();
            channel.dspFifo.stage.clearPendingOutput();
            channel.dspFifo.endFlushed    = false;
            channel.inputFrameAccumulator = 0.0;
        }
    }

    return true;
}

void AudioMixer::applyLivePerTrackDspSettings(uint64_t instanceId, const QByteArray& settings, uint64_t /*revision*/)
{
    if(instanceId == 0) {
        return;
    }

    for(auto& channel : m_channels) {
        for(auto& node : channel.perTrackDsps) {
            if(!node || node->instanceId() != instanceId || !node->supportsLiveSettings()) {
                continue;
            }

            node->loadSettings(settings);
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
    return readWithStatus(output, frames).producedFrames;
}

AudioMixer::ReadResult AudioMixer::readWithStatus(ProcessingBuffer& output, int frames)
{
    ReadResult result{};
    m_scratch.consumedTimeline.clear();

    if(frames <= 0 || !output.isValid() || output.format().sampleFormat() != SampleFormat::F64) {
        return result;
    }

    const AudioFormat outFormat = m_outputFormat.isValid() ? m_outputFormat : m_format;
    const int outChannels       = outFormat.channelCount();
    const int outRate           = outFormat.sampleRate();

    if(outChannels <= 0 || outRate <= 0) {
        return result;
    }

    ReadPlan plan;
    plan.outChannels      = outChannels;
    plan.outRate          = outRate;
    plan.requestedSamples = static_cast<size_t>(frames) * static_cast<size_t>(outChannels);

    // If queued output covers this request, drain it.
    if(m_output->queuedFrames() >= frames) {
        result.producedFrames  = drainOutput(output, plan);
        result.primaryTimeline = m_scratch.consumedTimeline;

        for(const auto& segment : result.primaryTimeline) {
            if(segment.streamId != InvalidStreamId) {
                result.primaryStreamId = segment.streamId;
                break;
            }
        }
        return result;
    }

    if(m_channels.empty()) {
        return result;
    }

    const int leftoverFrames = m_output->queuedFrames();
    const int framesNeeded   = std::max(0, frames - leftoverFrames);

    const auto categorization = categorizeStreams();

    plan.processFrames = std::max(1, framesNeeded);
    if(plan.processFrames <= 0) {
        return result;
    }

    const size_t maxInputSamples = calculateMaxInputSamples(outRate, plan.processFrames);

    prepareMixBuffers(plan, maxInputSamples);
    warmPendingStreams(plan, categorization);

    auto cycleState = mixActiveStreams(plan, categorization.pendingStreamToStartId);
    handleGaplessTransition(plan, categorization.pendingStreamToStartId, cycleState);
    commitToOutput(plan, categorization.pendingStreamToStartId, cycleState);

    result.producedFrames  = drainOutput(output, plan);
    result.sourceFrames    = cycleState.sourceFramesRead;
    result.buffering       = cycleState.dspBuffering;
    result.primaryTimeline = m_scratch.consumedTimeline;

    for(const auto& segment : result.primaryTimeline) {
        if(segment.streamId != InvalidStreamId) {
            result.primaryStreamId = segment.streamId;
            break;
        }
    }
    return result;
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

        categorization.haveNonPendingActiveStream = true;
    }

    return categorization;
}

size_t AudioMixer::calculateMaxInputSamples(int outRate, int processFrames) const
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

void AudioMixer::prepareMixBuffers(const ReadPlan& plan, size_t maxInputSamples)
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
    const auto addSourceFrames = [&cycleState](int value) {
        if(value <= 0) {
            return;
        }
        cycleState.sourceFramesRead = saturatingAddFrames(cycleState.sourceFramesRead, value);
    };

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

        const bool trackPrimary = !cycleState.hasTrackedPrimary && state != AudioStream::State::FadingOut
                               && stream->id() != pendingStreamToStartId;

        int streamSourceFramesRead{0};
        bool streamBuffering{false};

        const int gotFrames = mixSingleStream(channel, plan, plan.processFrames, 0, true, trackPrimary,
                                              trackPrimary ? &m_scratch.trackedTimeline : nullptr,
                                              &streamSourceFramesRead, &streamBuffering);

        addSourceFrames(streamSourceFramesRead);

        cycleState.dspBuffering = cycleState.dspBuffering || streamBuffering;

        if(trackPrimary && gotFrames > 0) {
            cycleState.hasTrackedPrimary      = true;
            cycleState.trackedPrimaryStreamId = stream->id();
        }

        if(gotFrames > 0) {
            cycleState.producedFrames = std::max(cycleState.producedFrames, gotFrames);
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

                int tailSourceFramesRead{0};
                bool tailBuffering{false};

                const int tailMixed = mixSingleStream(*pendingChannel, plan, tailFrames, gotFrames, true, trackPending,
                                                      trackPending ? &m_scratch.trackedTimeline : nullptr,
                                                      &tailSourceFramesRead, &tailBuffering);

                addSourceFrames(tailSourceFramesRead);

                cycleState.dspBuffering = cycleState.dspBuffering || tailBuffering;

                if(trackPending && tailMixed > 0) {
                    cycleState.hasTrackedPrimary      = true;
                    cycleState.trackedPrimaryStreamId = pendingStream->id();
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

        int handoffSourceFramesRead{0};
        bool handoffBuffering{false};

        const int handoffFrames = mixSingleStream(*pendingChannel, plan, plan.processFrames, 0, true, trackPending,
                                                  trackPending ? &m_scratch.trackedTimeline : nullptr,
                                                  &handoffSourceFramesRead, &handoffBuffering);

        cycleState.sourceFramesRead = saturatingAddFrames(cycleState.sourceFramesRead, handoffSourceFramesRead);
        cycleState.dspBuffering     = cycleState.dspBuffering || handoffBuffering;

        if(trackPending && handoffFrames > 0) {
            cycleState.hasTrackedPrimary      = true;
            cycleState.trackedPrimaryStreamId = pendingStream->id();
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
    auto& outputTimeline = m_scratch.outputTimeline;
    outputTimeline.clear();

    // Append mixed output only when we actually mixed active stream data
    if(cycleState.producedFrames > 0) {
        outputTimeline.reserve(m_scratch.trackedTimeline.size() + 2U);

        if(m_scratch.trackedTimeline.empty()) {
            Timeline::appendUnknown(outputTimeline, cycleState.producedFrames, cycleState.trackedPrimaryStreamId);
        }
        else {
            const auto byOffset = [](const TimelineRange& lhs, const TimelineRange& rhs) {
                return lhs.outputOffsetFrames < rhs.outputOffsetFrames;
            };

            if(!std::ranges::is_sorted(m_scratch.trackedTimeline, byOffset)) {
                std::ranges::sort(m_scratch.trackedTimeline, byOffset);
            }

            int cursor{0};
            for(const auto& range : m_scratch.trackedTimeline) {
                const int clampedOffset = std::clamp(range.outputOffsetFrames, 0, cycleState.producedFrames);

                if(clampedOffset > cursor) {
                    Timeline::appendUnknown(outputTimeline, clampedOffset - cursor, cycleState.trackedPrimaryStreamId);
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
                Timeline::appendSegment(outputTimeline, segment);
                cursor += segmentFrames;
            }

            if(cursor < cycleState.producedFrames) {
                Timeline::appendUnknown(outputTimeline, cycleState.producedFrames - cursor,
                                        cycleState.trackedPrimaryStreamId);
            }
        }
    }

    const int requestedFrames
        = static_cast<int>(plan.requestedSamples / static_cast<size_t>(std::max(1, plan.outChannels)));
    const size_t producedSamples = static_cast<size_t>(std::max(0, cycleState.producedFrames))
                                 * static_cast<size_t>(std::max(0, plan.outChannels));
    const std::span<const double> mixedSamples{
        m_scratch.outputSamples.data(),
        std::min(producedSamples, m_scratch.outputSamples.size()),
    };

    m_output->commitMixed(
        MixerOutput::CommitPlan{
            .outRate                = plan.outRate,
            .outChannels            = plan.outChannels,
            .requestedFrames        = requestedFrames,
            .pendingStreamToStartId = pendingStreamToStartId,
        },
        MixerOutput::CycleState{
            .startedPendingStream   = cycleState.startedPendingStream,
            .hasActiveStream        = cycleState.hasActiveStream,
            .dspBuffering           = cycleState.dspBuffering,
            .producedFrames         = cycleState.producedFrames,
            .trackedPrimaryStreamId = cycleState.trackedPrimaryStreamId,
        },
        mixedSamples, outputTimeline);
}

int AudioMixer::drainOutput(ProcessingBuffer& output, const ReadPlan& plan)
{
    const int requestedFrames
        = static_cast<int>(plan.requestedSamples / static_cast<size_t>(std::max(1, plan.outChannels)));
    return m_output->drain(output, requestedFrames, plan.outChannels, m_scratch.consumedTimeline);
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

    const int sampleRate = stream->sampleRate();
    if(sampleRate <= 0) {
        return std::max(0.0, channel->dspLatencySeconds);
    }

    const int runtimeFrames = runtimeLatencyFrames(*channel, sampleRate);
    if(runtimeFrames > 0) {
        return static_cast<double>(runtimeFrames) / static_cast<double>(sampleRate);
    }

    return std::max(0.0, channel->dspLatencySeconds);
}

uint64_t AudioMixer::primaryStreamPerTrackDspInflightNs() const
{
    const auto stream = primaryStream();
    if(!stream) {
        return 0;
    }

    const auto* channel = findChannel(stream->id());
    if(!channel || channel->perTrackDsps.empty()) {
        return 0;
    }

    const int sampleRate = stream->sampleRate();
    if(sampleRate <= 0) {
        return 0;
    }

    const int inflightFrames = runtimeLatencyFrames(*channel, sampleRate);
    if(inflightFrames <= 0) {
        return 0;
    }

    const auto frames = static_cast<uint64_t>(inflightFrames);
    const auto rate   = static_cast<uint64_t>(sampleRate);

    if(frames > (std::numeric_limits<uint64_t>::max() / static_cast<uint64_t>(Time::NsPerSecond))) {
        return std::numeric_limits<uint64_t>::max();
    }

    return (frames * static_cast<uint64_t>(Time::NsPerSecond) + rate - 1ULL) / rate;
}

uint64_t AudioMixer::frameDurationNs(int sampleRate)
{
    if(sampleRate <= 0) {
        return 0;
    }

    const double frameNs = static_cast<double>(Time::NsPerSecond) / static_cast<double>(sampleRate);
    const auto roundedNs = std::llround(frameNs);
    return static_cast<uint64_t>(std::max<int64_t>(1, roundedNs));
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

        if(node->isEnabled()) {
            const int latencyFrames = std::max(0, node->latencyFrames());

            if(latencyFrames > 0 && current.sampleRate() > 0) {
                total += static_cast<double>(latencyFrames) / static_cast<double>(current.sampleRate());
            }
            current = node->outputFormat(current);
        }
    }

    return total;
}

int AudioMixer::saturatingAddFrames(int lhs, int rhs) noexcept
{
    const int safeLhs = std::max(0, lhs);
    const int safeRhs = std::max(0, rhs);
    if(safeLhs > std::numeric_limits<int>::max() - safeRhs) {
        return std::numeric_limits<int>::max();
    }
    return safeLhs + safeRhs;
}

int AudioMixer::runtimeLatencyFrames(const MixerChannel& channel, int sampleRate)
{
    if(!channel.stream || channel.perTrackDsps.empty() || sampleRate <= 0) {
        return 0;
    }

    const double liveLatencySeconds = calculateDspLatencySeconds(channel.perTrackDsps, channel.stream->format());
    int liveLatencyFrames{0};
    if(liveLatencySeconds > 0.0) {
        const auto rounded = std::llround(liveLatencySeconds * static_cast<double>(sampleRate));
        liveLatencyFrames  = std::clamp<int64_t>(rounded, 0, std::numeric_limits<int>::max());
    }

    const auto inflightFramesU64 = channel.inflightEstimator.inflightFramesAt(sampleRate);
    const int inflightFrames     = inflightFramesU64 >= static_cast<uint64_t>(std::numeric_limits<int>::max())
                                     ? std::numeric_limits<int>::max()
                                     : static_cast<int>(inflightFramesU64);

    if(liveLatencyFrames <= 0) {
        return inflightFrames;
    }

    const int headroom       = std::max(1, liveLatencyFrames);
    const int boundedCeiling = liveLatencyFrames > std::numeric_limits<int>::max() - headroom
                                 ? std::numeric_limits<int>::max()
                                 : liveLatencyFrames + headroom;
    return std::max(liveLatencyFrames, std::min(inflightFrames, boundedCeiling));
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
        return {.data           = outputData,
                .frames         = outputFrames,
                .channels       = outputChannels,
                .sampleRate     = outputRate,
                .timelineChunks = m_scratch.perTrackTimeline};
    };

    const auto appendPerTrackTimelineChunk = [&](const TimedAudioFifo::TimelineChunk& chunk) {
        if(chunk.frames <= 0) {
            return;
        }

        if(!m_scratch.perTrackTimeline.empty()) {
            auto& prev               = m_scratch.perTrackTimeline.back();
            const bool metadataMatch = prev.valid == chunk.valid && prev.streamId == chunk.streamId
                                    && prev.epoch == chunk.epoch && prev.sampleRate == chunk.sampleRate;
            if(metadataMatch) {
                bool canMerge = !chunk.valid;

                if(chunk.valid && prev.frameDurationNs > 0 && prev.frameDurationNs == chunk.frameDurationNs) {
                    const uint64_t expectedStartNs
                        = prev.startNs + (prev.frameDurationNs * static_cast<uint64_t>(std::max(0, prev.frames)));
                    canMerge = expectedStartNs == chunk.startNs;
                }

                if(canMerge) {
                    prev.frames += chunk.frames;
                    return;
                }
            }
        }

        m_scratch.perTrackTimeline.push_back(chunk);
    };

    if(channel.perTrackDsps.empty()) {
        if(outputFrames > 0 && outputRate > 0) {
            appendPerTrackTimelineChunk(TimedAudioFifo::TimelineChunk{
                .valid           = true,
                .startNs         = inputSourceStartNs,
                .frameDurationNs = frameDurationNs(outputRate),
                .frames          = outputFrames,
                .sampleRate      = outputRate,
                .streamId        = stream->id(),
            });
        }
        else {
            appendPerTrackTimelineChunk(TimedAudioFifo::TimelineChunk{
                .valid           = false,
                .startNs         = 0,
                .frameDurationNs = 0,
                .frames          = outputFrames,
                .sampleRate      = outputRate,
                .streamId        = stream->id(),
            });
        }
        return buildResult();
    }

    AudioFormat dspFormat = stream->format();
    dspFormat.setSampleFormat(SampleFormat::F64);

    const auto inSamples     = static_cast<size_t>(inputFrames) * inChannels;
    m_scratch.perTrackBuffer = ProcessingBuffer({inputData, inSamples}, dspFormat, inputSourceStartNs);
    m_scratch.perTrackBuffer.setSourceFrameDurationNs(frameDurationNs(stream->sampleRate()));
    m_scratch.perTrackChunks.setToSingle(m_scratch.perTrackBuffer);

    for(auto& node : channel.perTrackDsps) {
        if(node && node->isEnabled()) {
            node->process(m_scratch.perTrackChunks);
        }
    }

    const int inRate = stream->sampleRate();
    int perTrackOutputFrames{0};
    int perTrackOutputRate{0};

    for(size_t i{0}; i < m_scratch.perTrackChunks.count(); ++i) {
        if(const auto* chunk = m_scratch.perTrackChunks.item(i); chunk && chunk->isValid() && chunk->frameCount() > 0) {
            if(perTrackOutputRate == 0) {
                perTrackOutputRate = chunk->format().sampleRate();
            }
            perTrackOutputFrames += chunk->frameCount();
        }
    }

    // Account for per-track DSP in-flight delay. Must run before early returns so
    // warmup cycles (no output) are still credited as consumed input.
    const int effectiveOutRate = perTrackOutputRate > 0 ? perTrackOutputRate : inRate;
    if(inRate > 0 && effectiveOutRate > 0) {
        channel.inflightEstimator.update(static_cast<uint64_t>(std::max(0, inputFrames)), inRate,
                                         static_cast<uint64_t>(std::max(0, perTrackOutputFrames)), effectiveOutRate);
    }

    const auto chunkFrameDurationNs = [&](const ProcessingBuffer* chunk, int fallbackRate) -> uint64_t {
        if(!chunk || !chunk->isValid()) {
            return 0;
        }

        if(chunk->sourceFrameDurationNs() > 0) {
            return chunk->sourceFrameDurationNs();
        }

        const int chunkRate = chunk->format().sampleRate();
        return frameDurationNs(chunkRate > 0 ? chunkRate : fallbackRate);
    };
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

        appendPerTrackTimelineChunk(TimedAudioFifo::TimelineChunk{
            .valid           = true,
            .startNs         = chunk->startTimeNs(),
            .frameDurationNs = chunkFrameDurationNs(chunk, outputRate),
            .frames          = outputFrames,
            .sampleRate      = outputRate,
            .streamId        = stream->id(),
        });
    }
    else {
        // Multiple chunks - coalesce into shared scratch vector
        m_scratch.perTrackCoalesced.clear();
        outputChannels = -1;
        outputRate     = -1;
        size_t expectedSamples{0};

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
        }

        if(outputChannels <= 0 || outputRate <= 0 || expectedSamples == 0) {
            return {};
        }

        m_scratch.perTrackCoalesced.resize(expectedSamples);

        size_t writeOffset{0};
        int timelineFrames{0};

        for(size_t i{0}; i < m_scratch.perTrackChunks.count(); ++i) {
            const auto* chunk = m_scratch.perTrackChunks.item(i);
            if(!chunk || !chunk->isValid()) {
                continue;
            }

            const auto data = chunk->constData();
            if(data.empty()) {
                continue;
            }

            if((writeOffset + data.size()) > m_scratch.perTrackCoalesced.size()) {
                return {};
            }

            std::copy_n(data.begin(), static_cast<std::ptrdiff_t>(data.size()),
                        m_scratch.perTrackCoalesced.begin() + static_cast<std::ptrdiff_t>(writeOffset));
            writeOffset += data.size();

            const int chunkFrames = chunk->frameCount();
            const int chunkRate   = chunk->format().sampleRate();
            if(chunkFrames <= 0 || chunkRate <= 0) {
                continue;
            }

            const uint64_t frameNs = chunkFrameDurationNs(chunk, chunkRate);
            appendPerTrackTimelineChunk(TimedAudioFifo::TimelineChunk{
                .valid           = frameNs > 0,
                .startNs         = frameNs > 0 ? chunk->startTimeNs() : 0,
                .frameDurationNs = frameNs,
                .frames          = chunkFrames,
                .sampleRate      = chunkRate,
                .streamId        = stream->id(),
            });
            timelineFrames += chunkFrames;
        }

        if(writeOffset != m_scratch.perTrackCoalesced.size()) {
            m_scratch.perTrackCoalesced.resize(writeOffset);
        }

        if(m_scratch.perTrackCoalesced.empty()) {
            return {};
        }

        outputData   = m_scratch.perTrackCoalesced.data();
        outputFrames = static_cast<int>(m_scratch.perTrackCoalesced.size()) / std::max(1, outputChannels);

        if(timelineFrames < outputFrames) {
            appendPerTrackTimelineChunk(TimedAudioFifo::TimelineChunk{
                .valid           = false,
                .startNs         = 0,
                .frameDurationNs = 0,
                .frames          = outputFrames - timelineFrames,
                .sampleRate      = outputRate,
                .streamId        = stream->id(),
            });
        }
    }

    return buildResult();
}

AudioMixer::StreamTopUpBudget AudioMixer::buildStreamTopUpBudget(MixerChannel& channel, const ReadPlan& plan,
                                                                 int targetFrames)
{
    StreamTopUpBudget budget{};

    auto* stream = channel.stream.get();
    if(!stream || targetFrames <= 0 || plan.outRate <= 0) {
        return budget;
    }

    const int streamRate = stream->sampleRate();
    if(streamRate <= 0) {
        return budget;
    }

    budget.inputFramesPerRead = std::max(1, targetFrames);
    if(streamRate != plan.outRate) {
        const double inputFramesExact
            = (static_cast<double>(targetFrames) * static_cast<double>(streamRate) / static_cast<double>(plan.outRate))
            + channel.inputFrameAccumulator;
        budget.inputFramesPerRead     = std::max(1, static_cast<int>(inputFramesExact));
        channel.inputFrameAccumulator = inputFramesExact - static_cast<double>(budget.inputFramesPerRead);
    }

    const double targetDurationMs
        = (static_cast<double>(std::max(1, targetFrames)) * 1000.0) / static_cast<double>(std::max(1, plan.outRate));
    const double baseTopUpDurationMs   = std::max(MinTopUpDurationMs, targetDurationMs * TopUpDurationMultiplier);
    const double latencyDurationMs     = std::max(0.0, channel.dspLatencySeconds * 1000.0);
    const double topUpBudgetDurationMs = std::clamp(baseTopUpDurationMs + latencyDurationMs + TopUpSafetyPaddingMs,
                                                    MinTopUpDurationMs, MaxTopUpDurationMs);

    const double maxInputFramesExact = (topUpBudgetDurationMs * static_cast<double>(streamRate)) / 1000.0;
    const double clampedInputFrames
        = std::clamp(maxInputFramesExact, 1.0, static_cast<double>(std::numeric_limits<int>::max()));
    const int maxInputFramesBudget
        = std::max(budget.inputFramesPerRead, static_cast<int>(std::ceil(clampedInputFrames)));
    const int lookaheadFrames
        = targetFrames >= std::numeric_limits<int>::max() ? std::numeric_limits<int>::max() : targetFrames + 1;

    budget.inputFramesPerRead = std::max(budget.inputFramesPerRead, std::min(maxInputFramesBudget, lookaheadFrames));
    budget.maxTopUpIters      = std::clamp(
        (maxInputFramesBudget + budget.inputFramesPerRead - 1) / budget.inputFramesPerRead, 1, MaxTopUpItersHardCap);
    budget.maxBufferingReadsThisCycle = std::max(MaxBufferingReadsPerCycle, budget.maxTopUpIters);
    return budget;
}

bool AudioMixer::flushStreamEndTail(MixerChannel& channel, const ReadPlan& plan, const AudioFormat& stageFormat)
{
    auto* stream = channel.stream.get();
    if(!stream) {
        return false;
    }

    if(channel.dspFifo.endFlushed) {
        return true;
    }

    channel.dspFifo.endFlushed = true;
    if(channel.perTrackDsps.empty()) {
        return true;
    }

    ProcessingBufferList tailChunks;

    const size_t nodeCount = channel.perTrackDsps.size();
    for(size_t nodeIndex{0}; nodeIndex < nodeCount; ++nodeIndex) {
        const auto& node = channel.perTrackDsps[nodeIndex];
        if(!node || !node->isEnabled()) {
            continue;
        }

        ProcessingBufferList nodeTailChunks;
        node->flush(nodeTailChunks, DspNode::FlushMode::EndOfTrack);
        if(nodeTailChunks.count() == 0) {
            continue;
        }

        for(size_t downstreamIndex{nodeIndex + 1}; downstreamIndex < nodeCount; ++downstreamIndex) {
            const auto& downstreamNode = channel.perTrackDsps[downstreamIndex];
            if(downstreamNode && downstreamNode->isEnabled()) {
                downstreamNode->process(nodeTailChunks);
            }
        }

        for(size_t i{0}; i < nodeTailChunks.count(); ++i) {
            const auto* chunk = nodeTailChunks.item(i);
            if(chunk && chunk->isValid()) {
                tailChunks.addChunk(*chunk);
            }
        }
    }

    auto* stage = &channel.dspFifo.stage;

    for(size_t i{0}; i < tailChunks.count(); ++i) {
        const auto* chunk = tailChunks.item(i);
        if(!chunk || !chunk->isValid()) {
            continue;
        }

        const int chunkChannels = chunk->format().channelCount();
        if(chunkChannels != plan.outChannels) {
            stream->applyCommand(AudioStream::Command::Stop);
            return false;
        }

        const auto chunkData  = chunk->constData();
        const int chunkFrames = chunk->frameCount();
        if(chunkFrames <= 0
           || chunkData.size() < static_cast<size_t>(chunkFrames) * static_cast<size_t>(chunkChannels)) {
            continue;
        }

        const int chunkRate = chunk->format().sampleRate();
        const uint64_t chunkFrameNs
            = chunk->sourceFrameDurationNs() > 0 ? chunk->sourceFrameDurationNs() : frameDurationNs(chunkRate);

        const TimedAudioFifo::TimelineChunk timelineChunk{
            .valid           = chunkFrameNs > 0,
            .startNs         = chunkFrameNs > 0 ? chunk->startTimeNs() : 0,
            .frameDurationNs = chunkFrameNs,
            .frames          = chunkFrames,
            .sampleRate      = plan.outRate,
            .streamId        = stream->id(),
        };

        const std::array timelineChunks{timelineChunk};
        const AudioBuffer tailBuffer{std::as_bytes(chunkData), stageFormat, 0};
        stage->appendPendingOutputOrReplace(tailBuffer, timelineChunks, stream->id());
    }

    return true;
}

void AudioMixer::fillStreamPendingOutput(MixerChannel& channel, const ReadPlan& plan, int targetFrames,
                                         const StreamTopUpBudget& budget, const AudioFormat& stageFormat,
                                         int& sourceFramesRead, bool& buffering)
{
    auto* stream = channel.stream.get();
    if(!stream || targetFrames <= 0) {
        return;
    }

    const int streamChannels = stream->channelCount();
    if(streamChannels <= 0 || budget.inputFramesPerRead <= 0 || budget.maxTopUpIters <= 0 || plan.outChannels <= 0) {
        return;
    }

    auto* stage = &channel.dspFifo.stage;

    int iters{0};
    int bufferingReadsThisCycle{0};

    int inputFramesPerRead  = budget.inputFramesPerRead;
    const int maxTopUpIters = std::max(budget.maxTopUpIters, budget.maxBufferingReadsThisCycle);

    const int64_t sampleRate64     = std::max<int64_t>(0, stream->sampleRate());
    const int64_t durationFrames64 = (sampleRate64 * static_cast<int64_t>(BufferingReadMaxDurationMs)) / 1000;
    const int durationFrameCap     = static_cast<int>(
        std::clamp<int64_t>(durationFrames64, 1, static_cast<int64_t>(BufferingReadMaxFramesHardCap)));
    const int maxBufferingReadFrames = std::max(budget.inputFramesPerRead, durationFrameCap);

    while(stage->pendingOutputFrames() < targetFrames && iters++ < maxTopUpIters) {
        if(stage->pendingWriteOffsetFrames() > 0) {
            stage->compactPendingOutput();
        }

        const auto inSamples = static_cast<size_t>(inputFramesPerRead) * static_cast<size_t>(streamChannels);
        if(m_scratch.mixSamples.size() < inSamples) {
            m_scratch.mixSamples.resize(inSamples);
        }

        auto* mixData                     = m_scratch.mixSamples.data();
        const uint64_t inputSourceStartNs = stream->positionMs() * Time::NsPerMs;
        const int framesRead              = stream->read(mixData, inputFramesPerRead);

        if(framesRead <= 0) {
            if(stream->isEndOfStream()) {
                if(!flushStreamEndTail(channel, plan, stageFormat)) {
                    break;
                }
                if(stage->pendingOutputFrames() == 0) {
                    stream->applyCommand(AudioStream::Command::Stop);
                }
            }
            break;
        }

        sourceFramesRead           = saturatingAddFrames(sourceFramesRead, framesRead);
        channel.dspFifo.endFlushed = false;

        const auto result = processPerTrackDsps(channel, mixData, framesRead, inputSourceStartNs);
        if(result.data && result.frames > 0) {
            bufferingReadsThisCycle = 0;
            inputFramesPerRead      = budget.inputFramesPerRead;

            if(result.channels != plan.outChannels) {
                stream->applyCommand(AudioStream::Command::Stop);
                stage->clearPendingOutput();
                break;
            }

            const auto outputSampleCount = static_cast<size_t>(result.frames) * static_cast<size_t>(plan.outChannels);
            const AudioBuffer outputBuffer{
                std::as_bytes(std::span<const double>{result.data, outputSampleCount}),
                stageFormat,
                0,
            };
            stage->appendPendingOutputOrReplace(outputBuffer, result.timelineChunks, stream->id());
            continue;
        }

        if(framesRead > 0 && !stream->isEndOfStream()) {
            buffering = true;

            const int grownInputFrames
                = inputFramesPerRead > (std::numeric_limits<int>::max() / BufferingReadGrowthFactor)
                    ? std::numeric_limits<int>::max()
                    : inputFramesPerRead * BufferingReadGrowthFactor;
            inputFramesPerRead = std::clamp(grownInputFrames, budget.inputFramesPerRead, maxBufferingReadFrames);
            ++bufferingReadsThisCycle;

            if(bufferingReadsThisCycle >= budget.maxBufferingReadsThisCycle) {
                break;
            }
        }
    }
}

int AudioMixer::consumeStreamPendingOutput(MixerChannel& channel, const ReadPlan& plan, int targetFrames,
                                           int outputFrameOffset, bool captureTimeline,
                                           std::vector<TimelineRange>* capturedTimeline)
{
    auto* stream = channel.stream.get();
    if(!stream || targetFrames <= 0 || outputFrameOffset < 0 || plan.outChannels <= 0) {
        return 0;
    }

    auto* stage            = &channel.dspFifo.stage;
    const auto tempSamples = static_cast<size_t>(targetFrames) * static_cast<size_t>(plan.outChannels);
    if(m_scratch.streamTemp.size() < tempSamples) {
        m_scratch.streamTemp.resize(tempSamples);
    }

    const auto writeResult = stage->writePendingOutput([&](const AudioBuffer& buffer, int frameOffset) -> int {
        if(!buffer.isValid() || frameOffset < 0) {
            return 0;
        }

        const int availableFrames = std::max(0, buffer.frameCount() - frameOffset);
        const int takeFrames      = std::min(targetFrames, availableFrames);
        if(takeFrames <= 0) {
            return 0;
        }

        const int bytesPerFrame = buffer.format().bytesPerFrame();
        if(bytesPerFrame <= 0) {
            return 0;
        }

        const size_t byteOffset = static_cast<size_t>(frameOffset) * static_cast<size_t>(bytesPerFrame);
        const size_t byteCount  = static_cast<size_t>(takeFrames) * static_cast<size_t>(bytesPerFrame);
        const auto srcBytes     = buffer.constData();
        if(byteOffset + byteCount > srcBytes.size()) {
            return 0;
        }

        auto dstBytes = std::as_writable_bytes(std::span<double>{m_scratch.streamTemp.data(), tempSamples});
        if(byteCount > dstBytes.size()) {
            return 0;
        }

        std::memcpy(dstBytes.data(), srcBytes.data() + byteOffset, byteCount);
        return takeFrames;
    });

    const int gotFrames = writeResult.writtenFrames;
    if(gotFrames <= 0) {
        return 0;
    }

    if(captureTimeline && capturedTimeline) {
        Timeline::consumeRange(stage->pendingTimeline(), writeResult.queueOffsetStartFrames, gotFrames, stream->id(),
                               m_scratch.consumedTimeline);
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

    if(stage->pendingWriteOffsetFrames() > 0
       && (!stage->hasPendingOutput() || stage->pendingWriteOffsetFrames() >= targetFrames)) {
        stage->compactPendingOutput();
    }

    return gotFrames;
}

void AudioMixer::applyStreamFadeToMix(const ReadPlan& plan, AudioStream& stream, int outputFrameOffset, int gotFrames)
{
    if(gotFrames <= 0 || plan.outChannels <= 0) {
        return;
    }

    const auto fadeRamp   = stream.calculateFadeGainRamp(gotFrames, plan.outRate);
    const auto baseOffset = static_cast<size_t>(outputFrameOffset) * static_cast<size_t>(plan.outChannels);
    const auto gotSamples = static_cast<size_t>(gotFrames) * static_cast<size_t>(plan.outChannels);

    if(!fadeRamp.active || gotFrames <= 1 || std::abs(fadeRamp.endGain - fadeRamp.startGain) < 0.000001) {
        for(size_t i{0}; i < gotSamples; ++i) {
            m_scratch.outputSamples[baseOffset + i] += m_scratch.streamTemp[i] * fadeRamp.endGain;
        }
        return;
    }

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

int AudioMixer::mixSingleStream(MixerChannel& channel, const ReadPlan& plan, int targetFrames, int outputFrameOffset,
                                bool consumeOutput, bool captureTimeline, std::vector<TimelineRange>* capturedTimeline,
                                int* sourceFramesReadOut, bool* bufferingOut)
{
    int sourceFramesRead{0};
    bool buffering{false};

    const auto finish = [&](int value) {
        if(sourceFramesReadOut) {
            *sourceFramesReadOut = sourceFramesRead;
        }
        if(bufferingOut) {
            *bufferingOut = buffering;
        }
        return value;
    };

    auto* stream = channel.stream.get();
    if(!stream || targetFrames <= 0 || outputFrameOffset < 0) {
        return finish(0);
    }

    const int streamChannels = stream->channelCount();
    const int streamRate     = stream->sampleRate();
    if(streamChannels <= 0 || streamRate <= 0 || plan.outChannels <= 0 || plan.outRate <= 0) {
        return finish(0);
    }

    const AudioFormat stageFormat{SampleFormat::F64, plan.outRate, plan.outChannels};
    const auto budget = buildStreamTopUpBudget(channel, plan, targetFrames);
    fillStreamPendingOutput(channel, plan, targetFrames, budget, stageFormat, sourceFramesRead, buffering);

    if(!consumeOutput) {
        return finish(0);
    }

    const int gotFrames
        = consumeStreamPendingOutput(channel, plan, targetFrames, outputFrameOffset, captureTimeline, capturedTimeline);
    if(gotFrames <= 0) {
        if(sourceFramesRead > 0 && !stream->isEndOfStream()) {
            buffering = true;
        }
        return finish(0);
    }

    applyStreamFadeToMix(plan, *stream, outputFrameOffset, gotFrames);
    return finish(gotFrames);
}
} // namespace Fooyin
