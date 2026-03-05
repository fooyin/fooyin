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

#include "audiopipeline.h"

#include "audioanalysisbus.h"
#include "chunktimelineassembler.h"
#include "dsp/dspregistry.h"
#include "enginehelpers.h"

#include <utils/timeconstants.h>

#include <QDebug>
#include <QLoggingCategory>
#include <QString>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

Q_LOGGING_CATEGORY(PIPELINE, "fy.pipeline")

using namespace Qt::StringLiterals;

constexpr auto IdleWaitMs                    = std::chrono::milliseconds{2000};
constexpr auto MaxQueuedCommands             = 4096;
constexpr auto MaxCommandsPerCycle           = 64;
constexpr auto MaxQueuedFadeEvents           = 128;
constexpr auto MinOrphanDrainTimeout         = std::chrono::milliseconds{2000};
constexpr auto MaxOrphanDrainTimeout         = std::chrono::milliseconds{15000};
constexpr auto ProcessChunkTargetMs          = std::chrono::milliseconds{10};
constexpr auto MinProcessChunkFrames         = 4096;
constexpr auto MaxProcessChunkFrames         = 1048576;
constexpr auto MasterTopUpMinPulls           = 8;
constexpr auto MasterTopUpMaxPulls           = 64;
constexpr auto MasterPrerollMinMs            = std::chrono::milliseconds{20};
constexpr auto MasterPrerollMaxMs            = std::chrono::milliseconds{300};
constexpr auto UnderrunConcealMaxMs          = std::chrono::milliseconds{20};
constexpr auto MaxMappedPositionRegressionMs = 250;
constexpr auto SeekPrerollGraceMs            = std::chrono::milliseconds{500};
constexpr auto CrossfadeFadeCurve            = Fooyin::Engine::FadeCurve::Sine;

namespace {
int processFrameChunkLimit(const Fooyin::AudioFormat& outputFormat)
{
    if(!outputFormat.isValid() || outputFormat.sampleRate() <= 0) {
        return MaxProcessChunkFrames;
    }

    const auto desiredFrames
        = (static_cast<uint64_t>(outputFormat.sampleRate()) * static_cast<uint64_t>(ProcessChunkTargetMs.count()))
        / 1000ULL;

    return static_cast<int>(std::clamp<uint64_t>(desiredFrames, static_cast<uint64_t>(MinProcessChunkFrames),
                                                 static_cast<uint64_t>(MaxProcessChunkFrames)));
}

int inputFramesForOutputFrames(const Fooyin::AudioFormat& inputFormat, const Fooyin::AudioFormat& outputFormat,
                               int outputFrames)
{
    if(outputFrames <= 0) {
        return 0;
    }

    if(!inputFormat.isValid() || !outputFormat.isValid()) {
        return outputFrames;
    }

    const int inputRate  = inputFormat.sampleRate();
    const int outputRate = outputFormat.sampleRate();
    if(inputRate <= 0 || outputRate <= 0 || inputRate == outputRate) {
        return outputFrames;
    }

    const uint64_t scaled = ((static_cast<uint64_t>(outputFrames) * static_cast<uint64_t>(inputRate))
                             + static_cast<uint64_t>(outputRate) - 1ULL)
                          / static_cast<uint64_t>(outputRate);

    return static_cast<int>(std::min<uint64_t>(scaled, static_cast<uint64_t>(std::numeric_limits<int>::max())));
}

void applyMasterVolume(Fooyin::ProcessingBufferList& chunks, double volume)
{
    const double gain = std::clamp(volume, 0.0, 1.0);
    if(gain == 1.0) {
        return;
    }

    for(size_t i{0}; i < chunks.count(); ++i) {
        auto* chunk = chunks.item(i);
        if(!chunk) {
            break;
        }

        auto& samples = chunk->samples();

        if(gain == 0.0) {
            std::ranges::fill(samples, 0.0);
        }
        else {
            for(double& sample : samples) {
                sample *= gain;
            }
        }
    }
}

QString describeFormat(const Fooyin::AudioFormat& format)
{
    if(!format.isValid()) {
        return u"invalid"_s;
    }

    return u"%1Hz %2ch %3"_s.arg(format.sampleRate()).arg(format.channelCount()).arg(format.prettyFormat())
         + u" [%1]"_s.arg(format.prettyChannelLayout());
}

uint64_t applySignedOffsetMs(uint64_t value, int64_t offset)
{
    if(offset >= 0) {
        const auto add = static_cast<uint64_t>(offset);

        if(value > std::numeric_limits<uint64_t>::max() - add) {
            return std::numeric_limits<uint64_t>::max();
        }

        return value + add;
    }

    const auto sub = static_cast<uint64_t>(-offset);
    return value > sub ? value - sub : 0;
}

bool shouldLogEveryN(std::atomic_uint32_t& counter, uint32_t interval)
{
    const uint32_t value = counter.fetch_add(1, std::memory_order_relaxed) + 1;
    return value == 1 || (value % interval) == 0;
}

bool sameRateAndChannelCount(const Fooyin::AudioFormat& lhs, const Fooyin::AudioFormat& rhs)
{
    if(!lhs.isValid() || !rhs.isValid()) {
        return false;
    }

    return lhs.sampleRate() == rhs.sampleRate() && lhs.channelCount() == rhs.channelCount();
}
} // namespace

namespace Fooyin {
AudioPipeline::AudioPipeline()
    : m_fadeEvents{MaxQueuedFadeEvents}
    , m_masterVolume{1.0}
    , m_dspRegistry{nullptr}
    , m_fadeEventDropCount{0}
    , m_analysisBus{nullptr}
    , m_playbackState{PipelinePlaybackState::Stopped}
    , m_playing{false}
    , m_renderPhase{RenderPhase::Stopped}
    , m_outputBitdepth{SampleFormat::Unknown}
    , m_ditherEnabled{false}
    , m_dataDemandNotified{false}
    , m_pendingWriteStallLogActive{false}
    , m_outputUnderrunLogActive{false}
    , m_transitionUnderrunLogActive{false}
{ }

AudioPipeline::~AudioPipeline()
{
    stop();
}

void AudioPipeline::setLastInitError(QString error)
{
    if(error.isEmpty()) {
        m_lastInitError.store(std::shared_ptr<const QString>{}, std::memory_order_release);
        return;
    }

    m_lastInitError.store(std::make_shared<const QString>(std::move(error)), std::memory_order_release);
}

void AudioPipeline::clearMappedPositionState(std::optional<uint64_t> positionMs)
{
    m_timelineUnit.clearMappedPositionState(positionMs);
}

void AudioPipeline::resetPlaybackDelayState(bool resetScale)
{
    m_timelineUnit.resetPlaybackDelayState(resetScale);
}

void AudioPipeline::resetPendingOutputState(bool resetMasterRate, bool clearLastOutputFrame)
{
    if(resetMasterRate) {
        resetMasterRateObservation();
    }

    m_outputUnit.resetPendingState(clearLastOutputFrame);
}

void AudioPipeline::clearRenderedSegmentSnapshot()
{
    m_timelineUnit.clearRenderedSegmentSnapshot();
}

void AudioPipeline::beginTimelineEpoch(uint64_t anchorPosMs)
{
    m_timelineUnit.beginTimelineEpoch(anchorPosMs);
}

std::shared_ptr<const AudioPipeline::FormatSnapshot> AudioPipeline::makeSnapshot(const AudioFormat& format)
{
    if(!format.isValid()) {
        return {};
    }

    auto snapshot = std::make_shared<FormatSnapshot>();

    snapshot->sampleRate       = format.sampleRate();
    snapshot->channelCount     = format.channelCount();
    snapshot->sampleFormat     = format.sampleFormat();
    snapshot->hasChannelLayout = format.hasChannelLayout();

    if(snapshot->hasChannelLayout) {
        snapshot->channelLayout.fill(AudioFormat::ChannelPosition::UnknownPosition);

        const auto layout = format.channelLayout();
        const auto limit  = std::min<std::size_t>({static_cast<std::size_t>(snapshot->channelCount),
                                                   static_cast<std::size_t>(AudioFormat::MaxChannels), layout.size()});

        for(size_t i{0}; i < limit; ++i) {
            snapshot->channelLayout[i] = layout[i];
        }
    }

    return snapshot;
}

AudioFormat AudioPipeline::formatFromSnapshot(const std::shared_ptr<const FormatSnapshot>& snapshot)
{
    if(!snapshot || snapshot->sampleRate <= 0 || snapshot->channelCount <= 0
       || snapshot->sampleFormat == SampleFormat::Unknown) {
        return {};
    }

    AudioFormat format{snapshot->sampleFormat, snapshot->sampleRate, snapshot->channelCount};

    if(!snapshot->hasChannelLayout) {
        return format;
    }

    const auto limit = std::min<std::size_t>(static_cast<std::size_t>(snapshot->channelCount),
                                             static_cast<std::size_t>(AudioFormat::MaxChannels));

    const AudioFormat::ChannelLayout layout{snapshot->channelLayout.begin(), snapshot->channelLayout.begin() + limit};

    format.setChannelLayout(layout);

    return format;
}

int AudioPipeline::writeOutputBufferFrames(const AudioBuffer& buffer, int frameOffset)
{
    const bool shouldStartOutput = m_playing.load(std::memory_order_acquire) && (m_renderPhase != RenderPhase::Running);
    const int writtenFrames      = m_outputUnit.writeOutputBufferFrames(buffer, frameOffset, shouldStartOutput);
    if(writtenFrames > 0 && shouldStartOutput) {
        m_renderPhase = RenderPhase::Running;
    }

    return writtenFrames;
}

int AudioPipeline::writeUnderrunConcealFrames(int maxFrames)
{
    const bool shouldStartOutput = m_playing.load(std::memory_order_acquire) && (m_renderPhase != RenderPhase::Running);
    const int writtenFrames
        = m_outputUnit.writeUnderrunConcealFrames(maxFrames, m_renderer.outputFormat(), shouldStartOutput);
    if(writtenFrames > 0 && shouldStartOutput) {
        m_renderPhase = RenderPhase::Running;
    }

    return writtenFrames;
}

bool AudioPipeline::hasPendingOutput() const
{
    return m_outputUnit.hasPendingOutput();
}

int AudioPipeline::pendingOutputFrames() const
{
    return m_outputUnit.pendingOutputFrames();
}

void AudioPipeline::compactPendingOutput()
{
    m_outputUnit.compactPendingOutput();
}

int AudioPipeline::masterPrerollFrames() const
{
    if(!m_renderer.outputFormat().isValid() || m_renderer.outputFormat().sampleRate() <= 0) {
        return 0;
    }

    const int latencyFrames = std::max(0, m_renderer.masterLatencyFrames());
    const auto latencyMs
        = std::chrono::milliseconds{static_cast<int64_t>(m_renderer.outputFormat().durationForFrames(latencyFrames))};
    const auto prerollMs = std::clamp(std::max(MasterPrerollMinMs, latencyMs), MasterPrerollMinMs, MasterPrerollMaxMs);

    return std::max(0, static_cast<int>((prerollMs.count() * m_renderer.outputFormat().sampleRate()) / 1000));
}

void AudioPipeline::appendPendingOutput(const AudioBuffer& buffer)
{
    if(!buffer.isValid()) {
        return;
    }

    const auto appendResult
        = m_outputUnit.appendPendingOutput(buffer, {}, InvalidStreamId, m_timelineUnit.timelineEpoch());
    if(appendResult.formatMismatch) {
        qCWarning(PIPELINE) << "Pending output format changed; dropping appended pending output";
    }
}

void AudioPipeline::clearPendingOutput()
{
    m_outputUnit.clearPendingOutput();
}

void AudioPipeline::resetCycleRenderedPosition()
{
    m_timelineUnit.resetCycleRenderedPosition();
}

void AudioPipeline::resetMasterRateObservation()
{
    m_timelineUnit.resetMasterRateObservation();
}

void AudioPipeline::queueFadeEvent(const FadeEvent& event)
{
    if(m_fadeEvents.writer().write(&event, 1) == 1) {
        markPendingSignal(PendingSignal::FadeEventsReady);
        return;
    }

    ++m_fadeEventDropCount;

    if(m_fadeEventDropCount == 1 || (m_fadeEventDropCount % 16) == 0) {
        qCInfo(PIPELINE) << "Fade event queue full; dropped" << m_fadeEventDropCount << "events";
    }
}

void AudioPipeline::start()
{
    const std::scoped_lock lock{m_lifecycleMutex};

    if(m_threadHost.isRunning()) {
        return;
    }

    m_threadHost.clearAudioThreadIdentity();
    m_threadHost.setShutdownRequested(false);
    m_threadHost.setRunning(true);
    m_threadHost.start([this] { audioThreadFunc(); });
}

void AudioPipeline::stop()
{
    const std::scoped_lock lifecycleLock{m_lifecycleMutex};

    if(!m_threadHost.isRunning()) {
        return;
    }

    m_threadHost.setShutdownRequested(true);
    m_threadHost.wake();

    if(m_threadHost.joinable()) {
        m_threadHost.join();
    }

    m_threadHost.setRunning(false);
    m_threadHost.setShutdownRequested(false);
    m_threadHost.clearAudioThreadIdentity();

    if(m_outputUnit.output() && m_outputUnit.output()->initialised()) {
        m_outputUnit.output()->uninit();
    }

    m_threadHost.clearCommands();

    m_playing.store(false, std::memory_order_release);
    m_playbackState.store(PipelinePlaybackState::Stopped, std::memory_order_release);
    m_renderPhase = RenderPhase::Stopped;
    m_outputUnit.setOutputInitialized(false);
    m_timelineUnit.setBufferUnderrun(false);
    clearMappedPositionState(0);
    resetPlaybackDelayState();
    beginTimelineEpoch(0);

    m_renderer.clearRuntimeState();
    m_outputUnit.setBufferFrames(0);
    clearPendingOutput();

    m_inputSnapshot.store(std::shared_ptr<const FormatSnapshot>{}, std::memory_order_release);
    m_outputSnapshot.store(std::shared_ptr<const FormatSnapshot>{}, std::memory_order_release);
    m_lastInitError.store(std::shared_ptr<const QString>{}, std::memory_order_release);

    m_streamRegistry.clear();
    clearOrphanState();
    resetQueuedNotifications(true);
    m_seekPrerollGraceUntil = {};
}

void AudioPipeline::setOutput(std::unique_ptr<AudioOutput> output)
{
    onAudioThreadAsync([output = std::move(output)](AudioPipeline& pipeline) mutable {
        if(pipeline.m_outputUnit.output() && pipeline.m_outputUnit.output()->initialised()) {
            pipeline.m_outputUnit.output()->uninit();
        }

        pipeline.m_outputUnit.setOutput(std::move(output));
        pipeline.m_outputUnit.setOutputSupportsVolume(!pipeline.m_outputUnit.output()
                                                      || pipeline.m_outputUnit.output()->supportsVolumeControl());

        if(pipeline.m_outputUnit.output() && pipeline.m_outputUnit.outputSupportsVolume()) {
            pipeline.m_outputUnit.output()->setVolume(pipeline.m_masterVolume);
        }

        pipeline.m_outputUnit.setOutputInitialized(false);
        pipeline.m_renderer.setOutputFormat({});
        pipeline.m_outputUnit.setBufferFrames(0);
        pipeline.clearMappedPositionState();
        pipeline.resetPlaybackDelayState();
        pipeline.m_outputSnapshot.store(std::shared_ptr<const FormatSnapshot>{}, std::memory_order_release);
        pipeline.beginTimelineEpoch(pipeline.m_timelineUnit.positionMs());
        pipeline.clearPendingOutput();
    });
}

void AudioPipeline::setDspRegistry(DspRegistry* registry)
{
    m_dspRegistry.store(registry, std::memory_order_release);
}

void AudioPipeline::addTrackProcessorFactory(AudioMixer::TrackProcessorFactory factory)
{
    if(!factory) {
        return;
    }

    onAudioThreadAsync([factory = std::move(factory)](AudioPipeline& pipeline) mutable {
        pipeline.m_renderer.addTrackProcessorFactory(std::move(factory));
    });
}

AudioFormat AudioPipeline::configureInputAndDspForFormat(const AudioFormat& format)
{
    auto* registry                   = m_dspRegistry.load(std::memory_order_acquire);
    const AudioFormat effectiveInput = m_renderer.predictPerTrackOutputFormat(format, registry);
    const AudioFormat configuredOutput
        = m_renderer.configureForInputFormat(format, effectiveInput, m_outputBitdepth, MinProcessChunkFrames);

    m_inputSnapshot.store(makeSnapshot(m_renderer.inputFormat()), std::memory_order_release);
    m_outputSnapshot.store(makeSnapshot(configuredOutput), std::memory_order_release);
    clearPendingOutput();
    resetMasterRateObservation();

    return configuredOutput;
}

bool AudioPipeline::init(const AudioFormat& format)
{
    if(!m_threadHost.isRunning()) {
        setLastInitError(u"Audio pipeline is not running"_s);
        return false;
    }

    return enqueueAndWait([format](AudioPipeline& pipeline) -> bool {
        pipeline.setLastInitError({});
        pipeline.clearMappedPositionState();
        pipeline.resetPlaybackDelayState();
        pipeline.resetPendingOutputState(true);
        pipeline.resetQueuedNotifications(false);

        if(!pipeline.m_outputUnit.output()) {
            pipeline.setLastInitError(u"No audio output backend is available"_s);
            return false;
        }

        pipeline.configureInputAndDspForFormat(format);

        AudioFormat requestedOutput = normaliseChannelLayout(pipeline.m_renderer.outputFormat());
        if(requestedOutput != pipeline.m_renderer.outputFormat()) {
            qCDebug(PIPELINE) << "Normalised output channel layout before backend negotiation:"
                              << describeFormat(requestedOutput);
            pipeline.m_renderer.setOutputFormat(requestedOutput);
            pipeline.m_outputSnapshot.store(makeSnapshot(requestedOutput), std::memory_order_release);
        }

        const AudioFormat negotiatedOutput = pipeline.m_outputUnit.output()->negotiateFormat(requestedOutput);
        if(negotiatedOutput.isValid() && negotiatedOutput.sampleRate() == requestedOutput.sampleRate()
           && negotiatedOutput.channelCount() == requestedOutput.channelCount()
           && negotiatedOutput.sampleFormat() == requestedOutput.sampleFormat()) {
            if(negotiatedOutput.hasChannelLayout()) {
                requestedOutput.setChannelLayout(negotiatedOutput.channelLayout());
            }
            else {
                requestedOutput.clearChannelLayout();
            }

            pipeline.m_renderer.setOutputFormat(requestedOutput);
            pipeline.m_outputSnapshot.store(makeSnapshot(pipeline.m_renderer.outputFormat()),
                                            std::memory_order_release);
        }

        if(pipeline.m_outputUnit.output()->init(pipeline.m_renderer.outputFormat())) {
            const AudioFormat actualFormat = pipeline.m_outputUnit.output()->format();

            const bool incompatibleFormat
                = actualFormat.sampleRate() != pipeline.m_renderer.outputFormat().sampleRate()
               || actualFormat.channelCount() != pipeline.m_renderer.outputFormat().channelCount();

            if(incompatibleFormat) {
                const QString requestedDescription = describeFormat(pipeline.m_renderer.outputFormat());
                const QString actualDescription    = describeFormat(actualFormat);
                const QString error = u"Output format negotiation failed: requested %1, backend initialised %2"_s.arg(
                    requestedDescription, actualDescription);
                qCWarning(PIPELINE) << error;

                pipeline.setLastInitError(error);
                pipeline.m_outputUnit.output()->uninit();
                pipeline.m_outputUnit.setOutputInitialized(false);
                pipeline.m_renderer.setOutputFormat({});
                pipeline.m_outputSnapshot.store(std::shared_ptr<const FormatSnapshot>{}, std::memory_order_release);
                pipeline.m_outputUnit.setBufferFrames(0);
                pipeline.clearMappedPositionState();
                pipeline.resetPlaybackDelayState();
                pipeline.clearPendingOutput();

                return false;
            }

            pipeline.setLastInitError({});
            pipeline.m_outputUnit.setOutputInitialized(true);
            pipeline.m_renderer.setOutputFormat(actualFormat);
            pipeline.m_outputSnapshot.store(makeSnapshot(pipeline.m_renderer.outputFormat()),
                                            std::memory_order_release);
            pipeline.m_outputUnit.setBufferFrames(std::max(1, pipeline.m_outputUnit.output()->bufferSize()));
            pipeline.m_renderPhase = RenderPhase::Stopped;

            if(pipeline.m_outputUnit.outputSupportsVolume()) {
                pipeline.m_outputUnit.output()->setVolume(pipeline.m_masterVolume);
            }

            return true;
        }

        const QString backendError = pipeline.m_outputUnit.output()->error().trimmed();
        if(!backendError.isEmpty()) {
            pipeline.setLastInitError(backendError);
        }
        else {
            pipeline.setLastInitError(u"Audio output backend failed to initialize requested format: %1"_s.arg(
                describeFormat(pipeline.m_renderer.outputFormat())));
        }

        pipeline.m_outputUnit.setOutputInitialized(false);
        pipeline.m_renderer.setOutputFormat({});
        pipeline.m_outputSnapshot.store(std::shared_ptr<const FormatSnapshot>{}, std::memory_order_release);
        pipeline.clearMappedPositionState();
        pipeline.resetPlaybackDelayState();
        pipeline.resetPendingOutputState(true, true);

        return false;
    });
}

AudioFormat AudioPipeline::applyInputFormat(const AudioFormat& format)
{
    return onAudioThread([format](AudioPipeline& pipeline) { return pipeline.configureInputAndDspForFormat(format); });
}

void AudioPipeline::uninit()
{
    if(!m_threadHost.isRunning()) {
        return;
    }

    enqueueAndWait([](AudioPipeline& pipeline) {
        if(pipeline.m_outputUnit.output() && pipeline.m_outputUnit.output()->initialised()) {
            pipeline.m_outputUnit.output()->uninit();
        }

        pipeline.m_outputUnit.setOutputInitialized(false);
        pipeline.m_playbackState.store(PipelinePlaybackState::Stopped, std::memory_order_release);
        pipeline.m_renderPhase = RenderPhase::Stopped;
        pipeline.m_renderer.setOutputFormat({});
        pipeline.m_outputUnit.setBufferFrames(0);
        pipeline.clearMappedPositionState();
        pipeline.resetPlaybackDelayState();
        pipeline.m_outputSnapshot.store(std::shared_ptr<const FormatSnapshot>{}, std::memory_order_release);
        pipeline.beginTimelineEpoch(0);
        pipeline.resetPendingOutputState(true, true);
        pipeline.resetQueuedNotifications(false);
    });
}

StreamId AudioPipeline::registerStream(AudioStreamPtr stream)
{
    return m_streamRegistry.registerStream(std::move(stream));
}

void AudioPipeline::unregisterStream(StreamId id)
{
    m_streamRegistry.unregisterStream(id);
}

void AudioPipeline::play()
{
    enqueueAsync([](AudioPipeline& pipeline) {
        const auto previousState = pipeline.m_playbackState.load(std::memory_order_acquire);
        pipeline.m_playing.store(true, std::memory_order_release);
        pipeline.m_playbackState.store(PipelinePlaybackState::Playing, std::memory_order_release);
        pipeline.m_renderPhase = RenderPhase::Preroll;

        if(pipeline.m_outputUnit.output() && pipeline.m_outputUnit.output()->initialised()) {
            pipeline.m_outputUnit.output()->setPaused(false);

            if(previousState != PipelinePlaybackState::Paused) {
                pipeline.clearPendingOutput();
            }
        }

        pipeline.m_renderer.resumeAll();
    });
}

void AudioPipeline::pause()
{
    enqueueAsync([](AudioPipeline& pipeline) {
        pipeline.m_playing.store(false, std::memory_order_release);
        pipeline.m_playbackState.store(PipelinePlaybackState::Paused, std::memory_order_release);
        pipeline.m_renderPhase = RenderPhase::Stopped;

        if(pipeline.m_outputUnit.output() && pipeline.m_outputUnit.output()->initialised()) {
            pipeline.m_outputUnit.output()->setPaused(true);
        }

        pipeline.m_renderer.pauseAll();
    });
}

void AudioPipeline::stopPlayback()
{
    enqueueAsync([](AudioPipeline& pipeline) {
        pipeline.m_playing.store(false, std::memory_order_release);
        pipeline.m_playbackState.store(PipelinePlaybackState::Stopped, std::memory_order_release);
        pipeline.m_renderPhase = RenderPhase::Stopped;

        if(pipeline.m_outputUnit.output() && pipeline.m_outputUnit.output()->initialised()) {
            (*pipeline.m_outputUnit.output()).reset();
        }

        {
            ProcessingBufferList chunks;
            pipeline.m_renderer.flushMaster(chunks, DspNode::FlushMode::Flush);
        }

        pipeline.m_renderer.clearAll();

        if(pipeline.m_orphanTracker.hasOrphan()) {
            pipeline.m_streamRegistry.unregisterStream(pipeline.m_orphanTracker.streamId());
        }

        pipeline.clearOrphanState();
        pipeline.clearMappedPositionState(0);
        pipeline.resetPlaybackDelayState();
        pipeline.beginTimelineEpoch(0);
        pipeline.resetPendingOutputState(true, true);
        pipeline.resetQueuedNotifications(false);
    });
}

void AudioPipeline::resetOutput()
{
    enqueueAsync([](AudioPipeline& pipeline) {
        if(pipeline.m_outputUnit.output() && pipeline.m_outputUnit.output()->initialised()) {
            (*pipeline.m_outputUnit.output()).reset();
        }

        pipeline.m_renderPhase = RenderPhase::Stopped;
        pipeline.resetPendingOutputState(true, true);
    });
}

void AudioPipeline::flushDsp(DspNode::FlushMode mode)
{
    enqueueAsync([mode](AudioPipeline& pipeline) {
        if(!pipeline.m_outputUnit.isOutputInitialized() || !pipeline.m_outputUnit.output()
           || !pipeline.m_outputUnit.output()->initialised()) {
            return;
        }

        ProcessingBufferList chunks;
        pipeline.m_renderer.flushMaster(chunks, mode);

        if(!pipeline.m_outputUnit.outputSupportsVolume()) {
            applyMasterVolume(chunks, pipeline.m_masterVolume);
        }

        if(Timeline::coalesceChunks(chunks, pipeline.m_renderer.outputFormat(),
                                    pipeline.m_outputUnit.coalescedOutputBuffer(), pipeline.m_ditherEnabled)) {
            auto& combinedBuffer = pipeline.m_outputUnit.coalescedOutputBuffer();

            if(pipeline.hasPendingOutput()) {
                pipeline.appendPendingOutput(combinedBuffer);
                return;
            }

            const int writtenFrames = pipeline.writeOutputBufferFrames(combinedBuffer, 0);

            if(writtenFrames < combinedBuffer.frameCount()) {
                pipeline.m_outputUnit.setPendingOutput(combinedBuffer, InvalidStreamId,
                                                       pipeline.m_timelineUnit.timelineEpoch());
                pipeline.m_outputUnit.dropPendingFrontFrames(std::max(0, writtenFrames));
            }
        }
    });
}

void AudioPipeline::setOutputVolume(double volume)
{
    const double clampedVolume = std::clamp(volume, 0.0, 1.0);

    onAudioThreadAsync([clampedVolume](AudioPipeline& pipeline) {
        pipeline.m_masterVolume = clampedVolume;

        if(pipeline.m_outputUnit.output() && pipeline.m_outputUnit.outputSupportsVolume()) {
            pipeline.m_outputUnit.output()->setVolume(clampedVolume);
        }
    });
}

void AudioPipeline::setOutputDevice(const QString& device)
{
    onAudioThreadAsync([device](AudioPipeline& pipeline) {
        if(pipeline.m_outputUnit.output()) {
            pipeline.m_outputUnit.output()->setDevice(device);
        }
    });
}

AudioFormat AudioPipeline::setOutputBitdepth(const SampleFormat bitdepth)
{
    return onAudioThread([bitdepth](AudioPipeline& pipeline) {
        pipeline.m_outputBitdepth = bitdepth;

        if(pipeline.m_outputBitdepth != SampleFormat::Unknown) {
            pipeline.m_renderer.setOutputBitdepth(pipeline.m_outputBitdepth);
        }

        pipeline.m_outputSnapshot.store(makeSnapshot(pipeline.m_renderer.outputFormat()), std::memory_order_release);

        return pipeline.m_renderer.outputFormat();
    });
}

void AudioPipeline::setDither(bool enabled)
{
    onAudioThreadAsync([enabled](AudioPipeline& pipeline) { pipeline.m_ditherEnabled = enabled; });
}

AudioFormat AudioPipeline::setDspChain(std::vector<DspNodePtr> masterNodes, const Engine::DspChain& perTrackDefs,
                                       const AudioFormat& inputFormat)
{
    return onAudioThread([nodes = std::move(masterNodes), perTrackDefs, inputFormat](AudioPipeline& pipeline) mutable {
        auto* registry = pipeline.m_dspRegistry.load(std::memory_order_acquire);

        const bool hadInput          = pipeline.m_renderer.inputFormat().isValid();
        const AudioFormat oldOutput  = pipeline.m_renderer.outputFormat();
        const AudioFormat oldProcess = pipeline.m_renderer.processFormat();

        const auto perTrackPatchPlan = pipeline.m_renderer.buildPerTrackChainPatchPlan(perTrackDefs);
        const bool perTrackChanged   = perTrackPatchPlan.hasChanges;

        if(perTrackChanged) {
            pipeline.m_renderer.setPerTrackChainDefs(perTrackDefs);
        }

        const auto masterPatchPlan = pipeline.m_renderer.buildMasterChainPatchPlan(nodes);
        const bool masterChanged   = masterPatchPlan.hasChanges;

        uint64_t anchorPosMs = pipeline.m_timelineUnit.positionMs();
        StreamId primaryId{InvalidStreamId};

        if(const auto primary = pipeline.m_renderer.primaryStream()) {
            anchorPosMs = primary->positionMs();
            primaryId   = primary->id();
        }

        pipeline.m_renderer.resetLiveSettings();

        const bool canApplyIncrementalMasterPatch
            = !perTrackChanged && pipeline.m_renderer.canApplyIncrementalMasterPatch(masterPatchPlan, hadInput);
        if(masterChanged) {
            if(canApplyIncrementalMasterPatch) {
                pipeline.m_renderer.applyIncrementalMasterPatch(masterPatchPlan, std::move(nodes));
            }
            else {
                pipeline.m_renderer.replaceMasterNodes(std::move(nodes));
            }
        }

        const AudioFormat effectiveInput = pipeline.m_renderer.predictPerTrackOutputFormat(inputFormat, registry);
        pipeline.m_renderer.reconfigureForChainChange(effectiveInput, pipeline.m_outputBitdepth, hadInput,
                                                      MinProcessChunkFrames);
        const AudioFormat newOutput  = pipeline.m_renderer.outputFormat();
        const AudioFormat newProcess = pipeline.m_renderer.processFormat();

        pipeline.m_outputSnapshot.store(makeSnapshot(newOutput), std::memory_order_release);

        const bool singleStreamNoTransition
            = !pipeline.m_orphanTracker.hasOrphan() && pipeline.m_renderer.mixingStreamCount() <= 1;
        const bool softChainUpdate = singleStreamNoTransition && sameRateAndChannelCount(oldOutput, newOutput)
                                  && sameRateAndChannelCount(oldProcess, newProcess);

        if(!softChainUpdate) {
            pipeline.clearPendingOutput();
        }

        if(perTrackChanged) {
            pipeline.m_renderer.applyPerTrackChainPatch(perTrackPatchPlan, registry, softChainUpdate);
        }

        if(!softChainUpdate) {
            // Live chain topology/rate changes are discontinuities for source mapping.
            // Reset mapped snapshots and re-anchor timeline mapping to the active stream.
            const std::optional<uint64_t> publishedPosition
                = (primaryId != InvalidStreamId) ? std::optional<uint64_t>{anchorPosMs} : std::nullopt;
            pipeline.clearMappedPositionState(publishedPosition);
            pipeline.beginTimelineEpoch(anchorPosMs);
            pipeline.resetMasterRateObservation();
        }

        return pipeline.m_renderer.predictMasterOutputFormat(effectiveInput);
    });
}

void AudioPipeline::updateLiveDspSettings(const Engine::LiveDspSettingsUpdate& update)
{
    onAudioThreadAsync([update](AudioPipeline& pipeline) { pipeline.m_renderer.applyLiveSettingsUpdate(update); });
}

AudioFormat AudioPipeline::predictPerTrackFormat(const AudioFormat& inputFormat) const
{
    return onAudioThread([inputFormat](const AudioPipeline& pipeline) {
        auto* registry = pipeline.m_dspRegistry.load(std::memory_order_acquire);
        return pipeline.m_renderer.predictPerTrackOutputFormat(inputFormat, registry);
    });
}

AudioFormat AudioPipeline::predictOutputFormat(const AudioFormat& inputFormat) const
{
    return onAudioThread([inputFormat](const AudioPipeline& pipeline) {
        auto* registry             = pipeline.m_dspRegistry.load(std::memory_order_acquire);
        AudioFormat effectiveInput = pipeline.m_renderer.predictPerTrackOutputFormat(inputFormat, registry);

        if(pipeline.m_outputBitdepth != SampleFormat::Unknown) {
            effectiveInput.setSampleFormat(pipeline.m_outputBitdepth);
        }

        return pipeline.m_renderer.predictMasterOutputFormat(effectiveInput);
    });
}

void AudioPipeline::sendStreamCommand(StreamId id, AudioStream::Command command, int param)
{
    onAudioThread([id, command, param](AudioPipeline& pipeline) {
        if(auto stream = pipeline.m_streamRegistry.getStream(id)) {
            stream->applyCommand(command, param);
        }
    });
}

void AudioPipeline::resetStreamForSeek(StreamId id, uint64_t anchorPosMs)
{
    onAudioThread([id, anchorPosMs](AudioPipeline& pipeline) {
        uint64_t timelineAnchorPosMs = anchorPosMs;
        if(auto stream = pipeline.m_streamRegistry.getStream(id)) {
            stream->applyCommand(AudioStream::Command::ResetForSeek);

            if(timelineAnchorPosMs == 0) {
                timelineAnchorPosMs = stream->positionMs();
            }
        }

        pipeline.m_renderer.resetStreamForSeek(id);
        pipeline.m_renderer.resetMaster();

        if(pipeline.m_outputUnit.output() && pipeline.m_outputUnit.output()->initialised()) {
            (*pipeline.m_outputUnit.output()).reset();
        }

        pipeline.resetPendingOutputState(true);
        pipeline.clearMappedPositionState();
        pipeline.resetPlaybackDelayState(false);
        pipeline.beginTimelineEpoch(timelineAnchorPosMs);
        // Allow a short preroll window so post-seek decode refill is not treated as an underrun
        pipeline.m_seekPrerollGraceUntil = std::chrono::steady_clock::now() + SeekPrerollGraceMs;
    });
}

void AudioPipeline::setFaderCurve(Engine::FadeCurve curve)
{
    enqueueAsync([curve](AudioPipeline& pipeline) { pipeline.m_outputFader.setFadeCurve(curve); });
}

void AudioPipeline::faderFadeIn(int durationMs, double targetVolume, uint64_t token)
{
    enqueueAsync([durationMs, targetVolume, token](AudioPipeline& pipeline) {
        const int sampleRate = pipeline.m_renderer.outputFormat().isValid()
                                 ? pipeline.m_renderer.outputFormat().sampleRate()
                                 : pipeline.m_renderer.inputFormat().sampleRate();
        pipeline.m_outputFader.fadeIn(durationMs, targetVolume, sampleRate, token);
    });
}

void AudioPipeline::faderFadeOut(int durationMs, double currentVolume, uint64_t token)
{
    enqueueAsync([durationMs, currentVolume, token](AudioPipeline& pipeline) {
        const int sampleRate = pipeline.m_renderer.outputFormat().isValid()
                                 ? pipeline.m_renderer.outputFormat().sampleRate()
                                 : pipeline.m_renderer.inputFormat().sampleRate();
        pipeline.m_outputFader.fadeOut(durationMs, currentVolume, sampleRate, token);
    });
}

void AudioPipeline::faderStop()
{
    enqueueAndWait([](AudioPipeline& pipeline) { pipeline.m_outputFader.stopFade(); });
}

void AudioPipeline::faderReset()
{
    enqueueAndWait([](AudioPipeline& pipeline) { pipeline.m_outputFader.reset(); });
}

bool AudioPipeline::faderIsFading() const
{
    return enqueueAndWait([](const AudioPipeline& pipeline) -> bool { return pipeline.m_outputFader.isFading(); });
}

void AudioPipeline::setSignalWakeTarget(QObject* target, int wakeEventType)
{
    m_signalMailbox.setWakeTarget(target, wakeEventType);
}

void AudioPipeline::setAnalysisBus(AudioAnalysisBus* analysisBus)
{
    m_analysisBus.store(analysisBus, std::memory_order_release);
}

AudioPipeline::PendingSignals AudioPipeline::drainPendingSignals()
{
    PendingSignals pendingSignals;

    const uint32_t signalBits        = m_signalMailbox.drain();
    pendingSignals.needsData         = (signalBits & pendingSignalMask(PendingSignal::NeedsData)) != 0;
    pendingSignals.fadeEventsPending = (signalBits & pendingSignalMask(PendingSignal::FadeEventsReady)) != 0;

    return pendingSignals;
}

void AudioPipeline::clearPendingSignals(uint32_t signalMask)
{
    m_signalMailbox.clear(signalMask);
}

void AudioPipeline::resetQueuedNotifications(bool resetWakeState)
{
    m_fadeEvents.requestReset();
    m_fadeEventDropCount = 0;

    notifyDataDemand(false);
    clearPendingSignals(pendingSignalMask(PendingSignal::FadeEventsReady));

    if(resetWakeState) {
        m_signalMailbox.reset();
    }
}

void AudioPipeline::markPendingSignal(const PendingSignal signal)
{
    m_signalMailbox.mark(pendingSignalMask(signal));
}

void AudioPipeline::notifyDataDemand(bool required)
{
    if(!required) {
        m_dataDemandNotified = false;
        clearPendingSignals(pendingSignalMask(PendingSignal::NeedsData));
        return;
    }

    if(m_dataDemandNotified) {
        return;
    }

    m_dataDemandNotified = true;
    markPendingSignal(PendingSignal::NeedsData);
}

void AudioPipeline::clearUnderrunWarningLatches()
{
    m_outputUnderrunLogActive     = false;
    m_transitionUnderrunLogActive = false;
}

void AudioPipeline::addStream(StreamId id)
{
    enqueueAsync([id](AudioPipeline& pipeline) {
        auto* registry = pipeline.m_dspRegistry.load(std::memory_order_acquire);
        auto stream    = pipeline.m_streamRegistry.getStream(id);

        if(!stream) {
            qCWarning(PIPELINE) << "addStream: stream not found in registry" << id;
            return;
        }

        pipeline.m_renderer.addStream(std::move(stream), pipeline.m_renderer.buildPerTrackNodes(registry));
    });
}

void AudioPipeline::removeStream(StreamId id)
{
    enqueueAsync([id](AudioPipeline& pipeline) { pipeline.m_renderer.removeStream(id); });
}

void AudioPipeline::switchToStream(StreamId id)
{
    enqueueAsync([id](AudioPipeline& pipeline) {
        auto* registry = pipeline.m_dspRegistry.load(std::memory_order_acquire);
        auto stream    = pipeline.m_streamRegistry.getStream(id);

        if(!stream) {
            qCWarning(PIPELINE) << "switchToStream: stream not found in registry" << id;
            return;
        }

        if(pipeline.m_orphanTracker.hasOrphan() && pipeline.m_orphanTracker.streamId() != id) {
            pipeline.m_streamRegistry.unregisterStream(pipeline.m_orphanTracker.streamId());
        }

        pipeline.clearOrphanState();

        const auto streamPosMs = stream->positionMs();

        pipeline.m_renderer.switchTo(std::move(stream), pipeline.m_renderer.buildPerTrackNodes(registry));
        pipeline.m_renderer.resetMaster();

        if(pipeline.m_outputUnit.output() && pipeline.m_outputUnit.output()->initialised()) {
            (*pipeline.m_outputUnit.output()).reset();
        }

        pipeline.m_renderPhase
            = pipeline.m_playing.load(std::memory_order_acquire) ? RenderPhase::Preroll : RenderPhase::Stopped;
        pipeline.clearPendingOutput();
        pipeline.clearMappedPositionState(streamPosMs);
        pipeline.resetPlaybackDelayState();
        pipeline.resetMasterRateObservation();
        pipeline.beginTimelineEpoch(streamPosMs);
    });
}

AudioPipeline::TransitionResult AudioPipeline::executeTransition(const TransitionRequest& request)
{
    if(!request.stream) {
        return {};
    }

    const StreamId newStreamId = registerStream(request.stream);
    if(newStreamId == InvalidStreamId) {
        return {};
    }

    const bool transitionApplied = onAudioThread([request, newStreamId](AudioPipeline& pipeline) -> bool {
        auto* registry = pipeline.m_dspRegistry.load(std::memory_order_acquire);

        if(pipeline.m_orphanTracker.hasOrphan()) {
            const StreamId orphanId = pipeline.m_orphanTracker.streamId();
            pipeline.m_renderer.removeStream(orphanId);
            pipeline.m_streamRegistry.unregisterStream(orphanId);
            pipeline.clearOrphanState();
        }

        auto targetStream = pipeline.m_streamRegistry.getStream(newStreamId);
        if(!targetStream) {
            qCWarning(PIPELINE) << "executeTransition: stream not found in registry" << newStreamId;
            return false;
        }

        switch(request.type) {
            case TransitionType::Crossfade:
            case TransitionType::SeekCrossfade: {
                auto currentPrimary = pipeline.m_renderer.primaryStream();

                targetStream->setFadeCurve(CrossfadeFadeCurve);

                if(!pipeline.m_renderer.hasStream(newStreamId)) {
                    pipeline.m_renderer.addStream(targetStream, pipeline.m_renderer.buildPerTrackNodes(registry));
                }

                if(currentPrimary && currentPrimary->state() != AudioStream::State::Stopped) {
                    if(!request.skipFadeOutStart) {
                        currentPrimary->setFadeCurve(CrossfadeFadeCurve);
                        currentPrimary->applyCommand(AudioStream::Command::StartFadeOut,
                                                     std::max(0, request.fadeOutMs));
                    }
                    pipeline.setOrphanState(currentPrimary->id(), pipeline.playbackDelayMs());
                    targetStream->applyCommand(AudioStream::Command::StartFadeIn, std::max(0, request.fadeInMs));
                }
                else {
                    pipeline.clearOrphanState();
                    targetStream->applyCommand(AudioStream::Command::Play);
                }

                if(request.reanchorTimeline) {
                    const uint64_t targetPosMs = targetStream->positionMs();
                    pipeline.clearMappedPositionState(targetPosMs);
                    pipeline.beginTimelineEpoch(targetPosMs);
                    pipeline.resetMasterRateObservation();
                }

                return true;
            }
            case TransitionType::Gapless: {
                if(!pipeline.m_renderer.hasStream(newStreamId)) {
                    pipeline.m_renderer.addStream(targetStream, pipeline.m_renderer.buildPerTrackNodes(registry));
                }

                auto currentPrimary = pipeline.m_renderer.primaryStream();
                if(!currentPrimary || currentPrimary->id() == newStreamId) {
                    targetStream->applyCommand(AudioStream::Command::Play);
                }

                uint64_t targetPosMs          = targetStream->positionMs();
                const bool liveGaplessHandoff = pipeline.m_playing.load(std::memory_order_acquire) && currentPrimary
                                             && currentPrimary->id() != newStreamId;
                if(liveGaplessHandoff && request.signalCurrentEndOfInput
                   && currentPrimary->state() != AudioStream::State::Stopped) {
                    // Decoder ownership has moved to the next track stream, so end this one
                    currentPrimary->setEndOfInput();
                }

                const uint64_t bufferedMs = targetStream->bufferedDurationMs();
                if(liveGaplessHandoff) {
                    const uint64_t transitionDelayMs = pipeline.m_timelineUnit.transitionPlaybackDelayMs();
                    if(bufferedMs < transitionDelayMs) {
                        qCWarning(PIPELINE) << "Gapless handoff prebuffer below transition delay:"
                                            << "newStreamId=" << newStreamId << "bufferedMs=" << bufferedMs
                                            << "transitionDelayMs=" << transitionDelayMs
                                            << "streamCount=" << pipeline.m_renderer.streamCount();
                    }
                }
                if(liveGaplessHandoff) {
                    targetPosMs = targetPosMs > bufferedMs ? (targetPosMs - bufferedMs) : 0;
                }

                if(request.reanchorTimeline) {
                    pipeline.clearMappedPositionState(targetPosMs);
                    pipeline.beginTimelineEpoch(targetPosMs);
                    pipeline.resetMasterRateObservation();
                }

                return true;
            }
        }

        return false;
    });

    if(!transitionApplied) {
        unregisterStream(newStreamId);
        return {};
    }

    return TransitionResult{.streamId = newStreamId, .success = true};
}

bool AudioPipeline::hasOrphanStream() const
{
    return onAudioThread([](const AudioPipeline& pipeline) { return pipeline.m_orphanTracker.hasOrphan(); });
}

void AudioPipeline::cleanupOrphanImmediate()
{
    const auto cleanup = [](AudioPipeline& pipeline) {
        if(!pipeline.m_orphanTracker.hasOrphan()) {
            return;
        }

        const StreamId orphanId = pipeline.m_orphanTracker.streamId();
        pipeline.m_renderer.removeStream(orphanId);
        pipeline.m_streamRegistry.unregisterStream(orphanId);
        pipeline.clearOrphanState();
    };

    onAudioThread(cleanup);
}

PipelineStatus AudioPipeline::currentStatus() const
{
    PipelineStatus status;
    const auto renderedSegment = m_timelineUnit.renderedSegment();

    status.position                      = m_timelineUnit.positionMs();
    status.timelineEpoch                 = m_timelineUnit.timelineEpoch();
    status.positionIsMapped              = m_timelineUnit.positionIsMapped();
    status.renderedSegment.valid         = renderedSegment.valid;
    status.renderedSegment.streamId      = renderedSegment.streamId;
    status.renderedSegment.sourceStartMs = renderedSegment.startMs;
    status.renderedSegment.sourceEndMs   = renderedSegment.endMs;
    status.renderedSegment.outputFrames  = renderedSegment.outputFrames;
    status.state                         = m_playbackState.load(std::memory_order_acquire);
    status.bufferUnderrun                = m_timelineUnit.bufferUnderrun();

    return status;
}

AudioPipeline::OutputQueueSnapshot AudioPipeline::outputQueueSnapshot() const
{
    return onAudioThread([](const AudioPipeline& pipeline) {
        OutputQueueSnapshot snapshot;

        if(!pipeline.m_outputUnit.isOutputInitialized() || !pipeline.m_outputUnit.output()
           || !pipeline.m_outputUnit.output()->initialised()) {
            return snapshot;
        }

        snapshot.state        = pipeline.m_outputUnit.output()->currentState();
        snapshot.bufferFrames = std::max(0, pipeline.m_outputUnit.bufferFrames());
        snapshot.valid        = true;

        return snapshot;
    });
}

uint64_t AudioPipeline::playbackDelayMs() const
{
    return m_timelineUnit.playbackDelayMs();
}

uint64_t AudioPipeline::transitionPlaybackDelayMs() const
{
    return m_timelineUnit.transitionPlaybackDelayMs();
}

double AudioPipeline::playbackDelayToTrackScale() const
{
    return m_timelineUnit.playbackDelayToTrackScale();
}

bool AudioPipeline::isRunning() const
{
    return m_threadHost.isRunning();
}

bool AudioPipeline::hasOutput() const
{
    return m_outputUnit.isOutputInitialized();
}

AudioFormat AudioPipeline::inputFormat() const
{
    const auto snapshot = m_inputSnapshot.load(std::memory_order_acquire);
    return formatFromSnapshot(snapshot);
}

AudioFormat AudioPipeline::outputFormat() const
{
    const auto snapshot = m_outputSnapshot.load(std::memory_order_acquire);
    return formatFromSnapshot(snapshot);
}

QString AudioPipeline::lastInitError() const
{
    const auto error = m_lastInitError.load(std::memory_order_acquire);
    return error ? *error : QString{};
}

size_t AudioPipeline::drainFadeEvents(FadeEvent* outEvents, size_t capacity)
{
    if(!outEvents || capacity == 0) {
        return 0;
    }

    return m_fadeEvents.reader().read(outEvents, capacity);
}

void AudioPipeline::audioThreadFunc()
{
    m_threadHost.markAudioThreadReady(currentThreadIdHash());

    while(true) {
        const bool hasMoreCommands = processCommands();

        if(m_threadHost.isShutdownRequested()) {
            break;
        }

        if(m_playing.load(std::memory_order_acquire) && m_outputUnit.isOutputInitialized() && m_outputUnit.output()
           && m_outputUnit.output()->initialised()) {
            processAudio();
        }
        else if(hasMoreCommands) {
            continue;
        }
        else {
            m_threadHost.waitFor(IdleWaitMs);
        }
    }

    m_threadHost.clearAudioThreadIdentity();
}

bool AudioPipeline::processCommands()
{
    std::deque<PipelineCommand> commands;
    const bool hasMoreCommands = m_threadHost.dequeueCommands(commands, MaxCommandsPerCycle);

    for(auto& command : commands) {
        if(command) {
            command(this);
        }
    }

    return hasMoreCommands;
}

bool AudioPipeline::enqueueCommand(PipelineCommand cmd) const
{
    if(!m_threadHost.isRunning() || m_threadHost.isShutdownRequested()) {
        return false;
    }

    if(!m_threadHost.enqueueCommand(std::move(cmd), MaxQueuedCommands)) {
        qWarning() << "AudioPipeline command queue full; dropping command";
        return false;
    }

    return true;
}

uint64_t AudioPipeline::currentThreadIdHash()
{
    return PipelineThreadHost::currentThreadIdHash();
}

bool AudioPipeline::isAudioThread() const
{
    return m_threadHost.isAudioThread();
}

void AudioPipeline::notifyResponse(const std::shared_ptr<CommandResponseVoid>& response)
{
    if(!response) {
        return;
    }

    {
        const std::scoped_lock lock{response->mutex};
        response->done = true;
    }

    response->cv.notify_one();
}

void AudioPipeline::processAudio()
{
    cleanupOrphanIfDone();

    if(!m_outputUnit.isOutputInitialized() || !m_outputUnit.output() || !m_outputUnit.output()->initialised()) {
        clearUnderrunWarningLatches();
        m_pendingWriteStallLogActive = false;
        return;
    }

    const auto state = m_outputUnit.output()->currentState();
    int freeFrames   = state.freeFrames;
    int framesWrittenThisCycle{0};
    resetCycleRenderedPosition();

    if(handleNoFreeFrames(state, freeFrames, framesWrittenThisCycle)) {
        clearUnderrunWarningLatches();
        m_pendingWriteStallLogActive = false;
        return;
    }

    const AudioFormat inputFormat = m_renderer.processFormat();
    const int channels            = inputFormat.channelCount();
    if(channels <= 0) {
        clearUnderrunWarningLatches();
        m_pendingWriteStallLogActive = false;
        notifyDataDemand(false);
        return;
    }

    const int prerollFrames         = masterPrerollFrames();
    const int desiredBufferedFrames = std::max({0, freeFrames, prerollFrames});
    const int processChunkFrames    = processFrameChunkLimit(m_renderer.outputFormat());
    const int initialQueuedFrames   = pendingOutputFrames();
    const int initialDeficitFrames  = std::max(1, desiredBufferedFrames - initialQueuedFrames);
    const int topUpPullBudget
        = std::max(1, (initialDeficitFrames + processChunkFrames - 1) / std::max(1, processChunkFrames));
    const int maxTopUpPulls = std::clamp(topUpPullBudget, MasterTopUpMinPulls, MasterTopUpMaxPulls);

    bool sawMixerUnderrun{false};
    bool sawMixerReadStarved{false};
    bool sawMasterChainStarved{false};

    for(int pull{0}; pull < maxTopUpPulls; ++pull) {
        const int queuedFrames = pendingOutputFrames();
        if(queuedFrames >= desiredBufferedFrames) {
            break;
        }

        const int outputFramesTarget = std::min(
            {std::max(1, desiredBufferedFrames - queuedFrames), m_outputUnit.bufferFrames(), processChunkFrames});
        int framesToProcess = inputFramesForOutputFrames(inputFormat, m_renderer.outputFormat(), outputFramesTarget);

        if(framesToProcess > MaxProcessChunkFrames) {
            static std::atomic_uint32_t cappedInputChunkCount{0};
            if(shouldLogEveryN(cappedInputChunkCount, 256U)) {
                qCDebug(PIPELINE) << "Input processing chunk capped below output demand:"
                                  << "requestedInputFrames=" << framesToProcess
                                  << "cappedInputFrames=" << MaxProcessChunkFrames
                                  << "targetOutputFrames=" << outputFramesTarget
                                  << "inputRate=" << inputFormat.sampleRate()
                                  << "outputRate=" << m_renderer.outputFormat().sampleRate();
            }
            framesToProcess = MaxProcessChunkFrames;
        }

        if(framesToProcess <= 0) {
            break;
        }

        const auto pullResult
            = m_renderer.render(framesToProcess, m_outputFader, m_outputUnit.outputSupportsVolume(), m_masterVolume,
                                m_analysisBus.load(std::memory_order_acquire), m_timelineUnit.playbackDelayMs());
        const int framesRead = pullResult.framesRead;

        if(framesRead <= 0) {
            if(pullResult.mixerBuffering) {
                // Per-track/master DSP may need additional warmup reads before
                // first output appears after a handoff.
                sawMixerUnderrun      = true;
                sawMasterChainStarved = true;

                static std::atomic_uint32_t bufferingNoOutputCount{0};
                if(shouldLogEveryN(bufferingNoOutputCount, 64U)) {
                    qCWarning(PIPELINE) << "DSP buffering produced no output frames during render pull:"
                                        << "pull=" << pull << "maxTopUpPulls=" << maxTopUpPulls
                                        << "framesToProcess=" << framesToProcess
                                        << "queuedPendingFrames=" << queuedFrames
                                        << "desiredBufferedFrames=" << desiredBufferedFrames
                                        << "streamCount=" << m_renderer.streamCount()
                                        << "transitionDelayMs=" << m_timelineUnit.transitionPlaybackDelayMs();
                }
                continue;
            }

            sawMixerUnderrun    = true;
            sawMixerReadStarved = true;
            break;
        }

        m_timelineUnit.setBufferUnderrun(false);
        m_timelineUnit.setCycleInputPrimaryStreamId(pullResult.primaryStreamId);
        if(pullResult.fadeCompletion) {
            queueFadeEvent(
                FadeEvent{.type = pullResult.fadeCompletion->type, .token = pullResult.fadeCompletion->token});
        }

        const int queuedNow = queueProcessedOutput();
        if(queuedNow > 0) {
            // Clear seek preroll grace only once audio has actually reached the
            // pending master output queue
            m_seekPrerollGraceUntil = {};
        }
        else if(framesRead > 0) {
            sawMixerUnderrun      = true;
            sawMasterChainStarved = true;
        }
    }

    if(!hasPendingOutput()) {
        if(sawMixerUnderrun) {
            const bool seekPrerollActive = std::chrono::steady_clock::now() < m_seekPrerollGraceUntil;

            if(seekPrerollActive && (sawMixerReadStarved || sawMasterChainStarved)) {
                const auto outputStateWithWrites = stateWithWrites(state, framesWrittenThisCycle);
                updatePlaybackDelay(outputStateWithWrites, PositionBasis::DecodeHead);
                notifyDataDemand(true);
                const auto waitDuration = m_outputUnit.writeBackoff(m_renderer.outputFormat(), outputStateWithWrites);
                m_threadHost.waitFor(waitDuration);
            }
            else {
                m_seekPrerollGraceUntil = {};
                handleMixerUnderrun(state, framesWrittenThisCycle, sawMixerReadStarved, sawMasterChainStarved);
            }
        }
        else {
            m_seekPrerollGraceUntil = {};
            clearUnderrunWarningLatches();
            m_pendingWriteStallLogActive     = false;
            const auto outputStateWithWrites = stateWithWrites(state, framesWrittenThisCycle);
            const auto basis
                = m_timelineUnit.positionIsMapped() ? PositionBasis::RenderedSource : PositionBasis::DecodeHead;
            updatePlaybackDelay(outputStateWithWrites, basis);
            notifyDataDemand(m_renderer.needsMoreData());
        }
        return;
    }

    if(drainPendingOutput(state, freeFrames, framesWrittenThisCycle, prerollFrames)) {
        m_seekPrerollGraceUntil = {};
        clearUnderrunWarningLatches();
        return;
    }

    if(framesWrittenThisCycle > 0) {
        m_seekPrerollGraceUntil = {};
        clearUnderrunWarningLatches();
        updatePlaybackState(state, framesWrittenThisCycle);
    }
    else {
        m_seekPrerollGraceUntil = {};
        clearUnderrunWarningLatches();
        const auto outputStateWithWrites = stateWithWrites(state, framesWrittenThisCycle);
        const auto basis
            = m_timelineUnit.positionIsMapped() ? PositionBasis::RenderedSource : PositionBasis::DecodeHead;
        updatePlaybackDelay(outputStateWithWrites, basis);
        notifyDataDemand(m_renderer.needsMoreData());
    }

    cleanupOrphanIfDone();
}

OutputState AudioPipeline::stateWithWrites(const OutputState& state, int framesWrittenThisCycle) const
{
    OutputState adjusted{state};

    const int queuedFromState
        = std::clamp(std::max(0, state.queuedFrames) + framesWrittenThisCycle, 0, m_outputUnit.bufferFrames());
    const int inferredQueuedBeforeWrite
        = std::clamp(m_outputUnit.bufferFrames() - std::max(0, state.freeFrames), 0, m_outputUnit.bufferFrames());
    const int queuedFromFreeFrames
        = std::clamp(inferredQueuedBeforeWrite + framesWrittenThisCycle, 0, m_outputUnit.bufferFrames());
    adjusted.queuedFrames = std::max(queuedFromState, queuedFromFreeFrames);

    if(m_renderer.outputFormat().sampleRate() > 0) {
        const double queuedDelay
            = static_cast<double>(adjusted.queuedFrames) / static_cast<double>(m_renderer.outputFormat().sampleRate());
        adjusted.delay = std::max(adjusted.delay, queuedDelay);
    }

    return adjusted;
}

void AudioPipeline::updatePlaybackDelay(const OutputState& outputState, const PositionBasis basis)
{
    uint64_t outputDelayMs{0};
    uint64_t transitionDspDelayMs{0};
    uint64_t dspDelayMs{0};
    double delayToTrackScale{1.0};

    if(m_renderer.outputFormat().isValid()) {
        const int backendQueuedFrames = std::max(0, outputState.queuedFrames);
        const int pendingQueuedFrames = std::max(0, pendingOutputFrames());
        const int totalQueuedFrames
            = std::clamp(backendQueuedFrames + pendingQueuedFrames, 0, std::numeric_limits<int>::max());
        const auto queuedMs = m_renderer.outputFormat().durationForFrames(totalQueuedFrames);
        const uint64_t deviceDelayMs
            = outputState.delay > 0.0 ? static_cast<uint64_t>(std::llround(outputState.delay * 1000.0)) : 0;

        // Some outputs provide explicit device delay, others only queued frames.
        outputDelayMs = std::max(queuedMs, deviceDelayMs);

        const double totalLatencySeconds = m_renderer.totalLatencySeconds();
        transitionDspDelayMs             = static_cast<uint64_t>(std::llround(totalLatencySeconds * 1000.0));

        if(basis == PositionBasis::DecodeHead) {
            dspDelayMs = transitionDspDelayMs;
        }

        if(m_timelineUnit.hasMasterRateObservation()) {
            delayToTrackScale = std::clamp(m_timelineUnit.observedMasterRateScale(), 0.05, 8.0);
        }
    }

    uint64_t totalDelayMs{outputDelayMs};

    if(totalDelayMs > std::numeric_limits<uint64_t>::max() - dspDelayMs) {
        totalDelayMs = std::numeric_limits<uint64_t>::max();
    }
    else {
        totalDelayMs += dspDelayMs;
    }

    m_timelineUnit.setPlaybackDelayMs(totalDelayMs);

    uint64_t transitionDelayMs{outputDelayMs};
    if(transitionDelayMs > std::numeric_limits<uint64_t>::max() - transitionDspDelayMs) {
        transitionDelayMs = std::numeric_limits<uint64_t>::max();
    }
    else {
        transitionDelayMs += transitionDspDelayMs;
    }

    m_timelineUnit.setTransitionPlaybackDelayMs(transitionDelayMs);
    m_timelineUnit.setPlaybackDelayToTrackScale(delayToTrackScale);
}

bool AudioPipeline::drainPendingOutput(const OutputState& state, int& freeFrames, int& framesWrittenThisCycle,
                                       int minBufferedFramesToDrain)
{
    if(!hasPendingOutput()) {
        return false;
    }

    const int queuedPendingFrames    = pendingOutputFrames();
    const bool belowPrerollThreshold = OutputPump::shouldHoldForPreroll(queuedPendingFrames, minBufferedFramesToDrain);
    if(m_renderPhase == RenderPhase::Preroll && belowPrerollThreshold) {
        const auto outputStateWithWrites = stateWithWrites(state, framesWrittenThisCycle);
        const auto basis
            = m_timelineUnit.positionIsMapped() ? PositionBasis::RenderedSource : PositionBasis::DecodeHead;
        updatePlaybackDelay(outputStateWithWrites, basis);
        notifyDataDemand(m_renderer.needsMoreData());

        const auto waitDuration = m_outputUnit.writeBackoff(m_renderer.outputFormat(), outputStateWithWrites);
        m_threadHost.waitFor(waitDuration);
        return true;
    }

    const int pendingOffsetBeforeWrite = m_outputUnit.pendingWriteOffsetFrames();
    bool publishedMappedUpdate{false};
    const auto writeResult = m_outputUnit.drainPending(
        [this](const AudioBuffer& buffer, int frameOffset) { return writeOutputBufferFrames(buffer, frameOffset); });
    const int pendingWritten = writeResult.writtenFrames;
    if(pendingWritten > 0) {
        m_pendingWriteStallLogActive = false;
        framesWrittenThisCycle += pendingWritten;
        freeFrames = std::max(0, freeFrames - pendingWritten);

        const int pendingStartOffset = writeResult.queueOffsetStartFrames;
        const int pendingEndOffset   = writeResult.queueOffsetEndFrames;
        const double masterScale     = std::clamp(m_timelineUnit.observedMasterRateScale(), 0.05, 8.0);
        const auto& pendingTimeline  = m_outputUnit.pendingTimeline();
        const auto startOffsetInfo
            = sourceTimelineOffsetInfoForFrameOffset(pendingTimeline, pendingStartOffset, masterScale,
                                                     m_outputUnit.pendingStreamId(), m_outputUnit.pendingEpoch());
        const auto segmentEndMs = sourceTimelinePositionForFrameOffset(pendingTimeline, pendingEndOffset, masterScale);

        if(startOffsetInfo && segmentEndMs) {
            m_timelineUnit.setCycleMappedPosition(*segmentEndMs);
            m_timelineUnit.setCycleRenderedSegment({
                .valid        = true,
                .streamId     = startOffsetInfo->streamId,
                .epoch        = startOffsetInfo->epoch,
                .startMs      = startOffsetInfo->positionMs,
                .endMs        = *segmentEndMs,
                .outputFrames = pendingWritten,
            });

            updatePlaybackState(state, framesWrittenThisCycle);
            publishedMappedUpdate = true;
        }
    }
    else {
        const bool pendingWriteStalled = freeFrames > 0 && queuedPendingFrames > 0;
        if(pendingWriteStalled && !m_pendingWriteStallLogActive) {
            m_pendingWriteStallLogActive = true;
            qCWarning(PIPELINE) << "Master pending write stalled with free output frames:"
                                << "queuedPendingFrames=" << queuedPendingFrames
                                << "pendingOffset=" << pendingOffsetBeforeWrite << "freeFrames=" << freeFrames
                                << "renderPhase=" << static_cast<int>(m_renderPhase);
        }
        else if(!pendingWriteStalled) {
            m_pendingWriteStallLogActive = false;
        }
    }

    if(!hasPendingOutput()) {
        return false;
    }

    if(pendingWritten <= 0 || !publishedMappedUpdate) {
        const auto outputStateWithWrites = stateWithWrites(state, framesWrittenThisCycle);
        const auto basis
            = m_timelineUnit.positionIsMapped() ? PositionBasis::RenderedSource : PositionBasis::DecodeHead;
        updatePlaybackDelay(outputStateWithWrites, basis);
        notifyDataDemand(m_renderer.needsMoreData());
    }

    if(OutputPump::shouldWaitAfterDrain(freeFrames, pendingWritten)) {
        const auto outputStateWithWrites = stateWithWrites(state, framesWrittenThisCycle);
        const auto waitDuration          = m_outputUnit.writeBackoff(m_renderer.outputFormat(), outputStateWithWrites);
        m_threadHost.waitFor(waitDuration);
    }

    return true;
}

bool AudioPipeline::handleNoFreeFrames(const OutputState& state, int freeFrames, int framesWrittenThisCycle)
{
    if(freeFrames > 0) {
        return false;
    }

    const auto outputStateWithWrites = stateWithWrites(state, framesWrittenThisCycle);
    const auto basis = m_timelineUnit.positionIsMapped() ? PositionBasis::RenderedSource : PositionBasis::DecodeHead;
    updatePlaybackDelay(outputStateWithWrites, basis);

    const auto waitDuration = m_outputUnit.writeBackoff(m_renderer.outputFormat(), outputStateWithWrites);

    m_threadHost.waitFor(waitDuration);

    notifyDataDemand(false);
    return true;
}

bool AudioPipeline::handleMixerUnderrun(const OutputState& state, int framesWrittenThisCycle, bool mixerReadStarved,
                                        bool masterChainStarved)
{
    m_pendingWriteStallLogActive = false;

    auto outputStateWithWrites = stateWithWrites(state, framesWrittenThisCycle);
    updatePlaybackDelay(outputStateWithWrites, PositionBasis::DecodeHead);

    if(m_renderer.streamCount() > 0 && m_renderPhase == RenderPhase::Preroll) {
        m_timelineUnit.setBufferUnderrun(false);
        clearUnderrunWarningLatches();
        notifyDataDemand(true);

        const auto waitDuration = m_outputUnit.writeBackoff(m_renderer.outputFormat(), outputStateWithWrites);
        m_threadHost.waitFor(waitDuration);
        return true;
    }

    if(m_renderer.streamCount() > 0) {
        m_timelineUnit.setBufferUnderrun(true);
        notifyDataDemand(true);

        if(!m_outputUnderrunLogActive) {
            m_outputUnderrunLogActive = true;
            qCWarning(PIPELINE) << "Output underrun:"
                                << "mixerReadStarved=" << mixerReadStarved
                                << "masterChainStarved=" << masterChainStarved
                                << "streamCount=" << m_renderer.streamCount()
                                << "queuedFrames=" << outputStateWithWrites.queuedFrames
                                << "freeFrames=" << outputStateWithWrites.freeFrames
                                << "delayMs=" << std::llround(outputStateWithWrites.delay * 1000.0)
                                << "pendingFrames=" << pendingOutputFrames();
        }

        if(m_orphanTracker.hasOrphan()) {
            if(!m_transitionUnderrunLogActive) {
                m_transitionUnderrunLogActive = true;
                qCWarning(PIPELINE) << "Output underrun during active transition:" << "orphanId="
                                    << m_orphanTracker.streamId()
                                    << "queuedFrames=" << outputStateWithWrites.queuedFrames
                                    << "freeFrames=" << outputStateWithWrites.freeFrames
                                    << "delayMs=" << std::llround(outputStateWithWrites.delay * 1000.0);
            }
        }
        else {
            m_transitionUnderrunLogActive = false;
        }

        const bool shouldConcealUnderrun
            = (mixerReadStarved || masterChainStarved) && outputStateWithWrites.freeFrames > 0 && !hasPendingOutput();
        if(shouldConcealUnderrun) {
            const int concealFrameBudget
                = std::max(1, static_cast<int>((static_cast<int64_t>(m_renderer.outputFormat().sampleRate())
                                                * UnderrunConcealMaxMs.count())
                                               / 1000));
            const int concealFrames  = std::min(outputStateWithWrites.freeFrames, concealFrameBudget);
            const int writtenConceal = writeUnderrunConcealFrames(concealFrames);

            if(writtenConceal > 0) {
                m_timelineUnit.setPositionIsMapped(false);
                clearRenderedSegmentSnapshot();
                outputStateWithWrites = stateWithWrites(state, framesWrittenThisCycle + writtenConceal);
                updatePlaybackDelay(outputStateWithWrites, PositionBasis::DecodeHead);
            }
        }
    }
    else {
        clearUnderrunWarningLatches();
        notifyDataDemand(false);
    }

    const auto waitDuration = m_outputUnit.writeBackoff(m_renderer.outputFormat(), outputStateWithWrites);

    m_threadHost.waitFor(waitDuration);

    return true;
}

int AudioPipeline::queueProcessedOutput()
{
    const auto queueResult = m_renderer.queueProcessedOutput(
        m_outputUnit, m_ditherEnabled, m_timelineUnit.cycleInputPrimaryStreamId(), m_timelineUnit.timelineEpoch());
    if(queueResult.coalesceFailed) {
        // Expected for active DSPs with buffering
        return 0;
    }
    if(queueResult.zeroFrameCoalesce) {
        static std::atomic_uint32_t zeroFrameCoalesceCount{0};
        if(shouldLogEveryN(zeroFrameCoalesceCount, 512U)) {
            qCDebug(PIPELINE) << "Master coalesced buffer had zero frames";
        }
        return 0;
    }
    if(queueResult.formatReset) {
        static std::atomic_uint32_t formatResetCount{0};
        if(shouldLogEveryN(formatResetCount, 32U)) {
            qCWarning(PIPELINE) << "Pending output format changed; resetting master pending queue";
        }
    }

    return queueResult.appendedFrames;
}

std::expected<uint64_t, AudioPipeline::TimelineOffsetLookupError>
AudioPipeline::sourceTimelinePositionForFrameOffset(const std::vector<TimelineChunk>& timelineChunks, int frameOffset,
                                                    double fallbackScale)
{
    const auto info
        = sourceTimelineOffsetInfoForFrameOffset(timelineChunks, frameOffset, fallbackScale, InvalidStreamId, 0);
    if(!info) {
        return std::unexpected(info.error());
    }

    return info->positionMs;
}

std::expected<AudioPipeline::TimelineOffsetInfo, AudioPipeline::TimelineOffsetLookupError>
AudioPipeline::sourceTimelineOffsetInfoForFrameOffset(const std::vector<TimelineChunk>& timelineChunks, int frameOffset,
                                                      double fallbackScale, StreamId fallbackStreamId,
                                                      uint64_t fallbackEpoch)
{
    if(frameOffset < 0 || timelineChunks.empty()) {
        return std::unexpected(TimelineOffsetLookupError::InvalidInput);
    }

    const double scale = std::clamp(fallbackScale, 0.01, 8.0);
    int remainingFrames{frameOffset};

    auto timelineInfoFor = [](uint64_t positionMs, StreamId streamId, uint64_t epoch) -> TimelineOffsetInfo {
        return {
            .positionMs = positionMs,
            .streamId   = streamId,
            .epoch      = epoch,
        };
    };

    auto fallbackFrameNs = [scale](const TimelineChunk& c) {
        const auto rounded = std::llround((Time::NsPerSecond * scale) / static_cast<double>(std::max(1, c.sampleRate)));
        return static_cast<uint64_t>(std::max<int64_t>(1, rounded));
    };

    const TimelineChunk* tailChunk{nullptr};
    uint64_t lastMappedBoundaryNs{0};
    bool hasMappedBoundary{false};
    StreamId lastMappedBoundaryStreamId{InvalidStreamId};
    StreamId lastStreamId{fallbackStreamId};
    uint64_t lastEpoch{fallbackEpoch};

    for(size_t idx{0}; idx < timelineChunks.size(); ++idx) {
        const auto& chunk = timelineChunks[idx];
        if(chunk.frames <= 0) {
            continue;
        }

        tailChunk = &chunk;

        const StreamId chunkStreamId = (chunk.streamId != InvalidStreamId) ? chunk.streamId : fallbackStreamId;
        const uint64_t chunkEpoch    = (chunk.epoch != 0) ? chunk.epoch : fallbackEpoch;

        if(hasMappedBoundary && chunk.streamId != InvalidStreamId && lastMappedBoundaryStreamId != InvalidStreamId
           && chunk.streamId != lastMappedBoundaryStreamId) {
            hasMappedBoundary          = false;
            lastMappedBoundaryNs       = 0;
            lastMappedBoundaryStreamId = InvalidStreamId;
        }

        const int take          = std::min(remainingFrames, chunk.frames);
        const bool lastChunk    = (idx + 1) == timelineChunks.size();
        const bool landsInChunk = (take < chunk.frames) || (lastChunk && remainingFrames <= chunk.frames);

        if(chunk.valid) {
            const uint64_t frameNs  = (chunk.frameDurationNs != 0) ? chunk.frameDurationNs : fallbackFrameNs(chunk);
            const uint64_t offsetNs = frameNs * static_cast<uint64_t>(std::max(0, take));

            if(landsInChunk) {
                return timelineInfoFor((chunk.startNs + offsetNs) / Time::NsPerMs, chunkStreamId, chunkEpoch);
            }

            remainingFrames -= take;
            lastStreamId = chunkStreamId;
            lastEpoch    = chunkEpoch;

            if(remainingFrames <= 0) {
                return timelineInfoFor((chunk.startNs + (frameNs * static_cast<uint64_t>(std::max(0, chunk.frames))))
                                           / Time::NsPerMs,
                                       lastStreamId, lastEpoch);
            }

            lastMappedBoundaryNs = chunk.startNs + (frameNs * static_cast<uint64_t>(std::max(0, chunk.frames)));
            hasMappedBoundary    = true;
            lastMappedBoundaryStreamId
                = (chunk.streamId != InvalidStreamId) ? chunk.streamId : lastMappedBoundaryStreamId;
        }
        else {
            if(landsInChunk) {
                if(!hasMappedBoundary) {
                    return std::unexpected(TimelineOffsetLookupError::NoMappedBoundary);
                }

                if(chunk.streamId != InvalidStreamId && lastMappedBoundaryStreamId != InvalidStreamId
                   && chunk.streamId != lastMappedBoundaryStreamId) {
                    return std::unexpected(TimelineOffsetLookupError::StreamBoundaryMismatch);
                }

                return timelineInfoFor(lastMappedBoundaryNs / Time::NsPerMs, chunkStreamId, chunkEpoch);
            }

            remainingFrames -= take;
            lastStreamId = chunkStreamId;
            lastEpoch    = chunkEpoch;
            if(remainingFrames <= 0) {
                if(!hasMappedBoundary) {
                    return std::unexpected(TimelineOffsetLookupError::NoMappedBoundary);
                }

                return timelineInfoFor(lastMappedBoundaryNs / Time::NsPerMs, lastStreamId, lastEpoch);
            }
        }
    }

    if(!tailChunk || tailChunk->frames <= 0) {
        if(!hasMappedBoundary) {
            return std::unexpected(TimelineOffsetLookupError::NoMappedBoundary);
        }
        return timelineInfoFor(lastMappedBoundaryNs / Time::NsPerMs, lastStreamId, lastEpoch);
    }

    const StreamId tailStreamId = (tailChunk->streamId != InvalidStreamId) ? tailChunk->streamId : fallbackStreamId;
    const uint64_t tailEpoch    = (tailChunk->epoch != 0) ? tailChunk->epoch : fallbackEpoch;

    if(!tailChunk->valid) {
        if(!hasMappedBoundary) {
            return std::unexpected(TimelineOffsetLookupError::NoMappedBoundary);
        }

        if(tailChunk->streamId != InvalidStreamId && lastMappedBoundaryStreamId != InvalidStreamId
           && tailChunk->streamId != lastMappedBoundaryStreamId) {
            return std::unexpected(TimelineOffsetLookupError::StreamBoundaryMismatch);
        }

        return timelineInfoFor(lastMappedBoundaryNs / Time::NsPerMs, tailStreamId, tailEpoch);
    }

    const uint64_t tailFrameNs
        = (tailChunk->frameDurationNs != 0) ? tailChunk->frameDurationNs : fallbackFrameNs(*tailChunk);
    const uint64_t tailEndNs
        = tailChunk->startNs + (tailFrameNs * static_cast<uint64_t>(std::max(0, tailChunk->frames)));

    return timelineInfoFor((tailEndNs + (tailFrameNs * static_cast<uint64_t>(std::max(0, remainingFrames))))
                               / Time::NsPerMs,
                           tailStreamId, tailEpoch);
}

void AudioPipeline::updatePlaybackState(const OutputState& state, int framesWrittenThisCycle)
{
    const auto cycleSegment{m_timelineUnit.cycleRenderedSegment()};
    bool acceptCycleMapped{m_timelineUnit.cycleMappedPositionValid()};
    uint64_t primaryPosMs{0};
    StreamId primaryId{InvalidStreamId};

    if(const auto primary = m_renderer.primaryStream()) {
        primaryId    = primary->id();
        primaryPosMs = primary->positionMs();
    }

    if(acceptCycleMapped && m_timelineUnit.positionIsMapped()) {
        const uint64_t currentPos = m_timelineUnit.positionMs();

        if(m_timelineUnit.cycleMappedPositionMs() + MaxMappedPositionRegressionMs < currentPos) {
            bool allowBackwardJump{false};

            if(const auto stream = m_renderer.primaryStream()) {
                const uint64_t streamPos = stream->positionMs();
                allowBackwardJump        = streamPos + MaxMappedPositionRegressionMs < currentPos;
            }

            if(!allowBackwardJump) {
                acceptCycleMapped = false;
            }
        }
    }

    if(acceptCycleMapped && cycleSegment.valid) {
        const bool epochMismatch  = cycleSegment.epoch != 0 && cycleSegment.epoch != m_timelineUnit.timelineEpoch();
        const bool streamMismatch = cycleSegment.streamId != InvalidStreamId && primaryId != InvalidStreamId
                                 && cycleSegment.streamId != primaryId;

        if(epochMismatch || streamMismatch) {
            acceptCycleMapped = false;
        }
    }

    if(acceptCycleMapped && cycleSegment.valid) {
        if(!m_timelineUnit.timelineEpochOffsetResolved()) {
            int64_t computedOffsetMs{0};

            if(m_timelineUnit.timelineEpochAnchorPosMs() >= cycleSegment.startMs) {
                const uint64_t delta = m_timelineUnit.timelineEpochAnchorPosMs() - cycleSegment.startMs;
                computedOffsetMs     = delta > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())
                                         ? std::numeric_limits<int64_t>::max()
                                         : static_cast<int64_t>(delta);
            }
            else {
                const uint64_t delta = cycleSegment.startMs - m_timelineUnit.timelineEpochAnchorPosMs();
                computedOffsetMs     = delta > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())
                                         ? std::numeric_limits<int64_t>::min()
                                         : -static_cast<int64_t>(delta);
            }

            m_timelineUnit.setTimelineEpochOffsetMs(computedOffsetMs, true);
        }

        const int64_t epochOffsetMs = m_timelineUnit.timelineEpochOffsetMs();
        const uint64_t adjustedMappedPositionMs
            = applySignedOffsetMs(m_timelineUnit.cycleMappedPositionMs(), epochOffsetMs);
        const uint64_t adjustedSegmentStartMs = applySignedOffsetMs(cycleSegment.startMs, epochOffsetMs);
        const uint64_t adjustedSegmentEndMs   = applySignedOffsetMs(cycleSegment.endMs, epochOffsetMs);
        const StreamId mappedStreamId = (cycleSegment.streamId != InvalidStreamId) ? cycleSegment.streamId : primaryId;

        m_timelineUnit.setPositionMs(adjustedMappedPositionMs);
        m_timelineUnit.setPositionIsMapped(true);
        m_timelineUnit.setRenderedSegment({
            .valid        = true,
            .streamId     = mappedStreamId,
            .startMs      = adjustedSegmentStartMs,
            .endMs        = adjustedSegmentEndMs,
            .outputFrames = cycleSegment.outputFrames,
        });
    }
    else {
        m_timelineUnit.setPositionIsMapped(false);
        clearRenderedSegmentSnapshot();
        if(primaryId != InvalidStreamId) {
            m_timelineUnit.setPositionMs(primaryPosMs);
        }
    }

    // Derive source-time-per-output-time scale from mapped master output.
    if(cycleSegment.valid && cycleSegment.outputFrames > 0 && cycleSegment.endMs >= cycleSegment.startMs
       && m_renderer.outputFormat().sampleRate() > 0) {
        const int outputRate = m_renderer.outputFormat().sampleRate();
        const auto currentOutputDurMs
            = (static_cast<double>(cycleSegment.outputFrames) * 1000.0) / static_cast<double>(outputRate);
        const auto currentSourceDurMs = static_cast<double>(cycleSegment.endMs - cycleSegment.startMs);

        double observedScale{0.0};
        bool hasObservedScale{false};

        if(m_timelineUnit.hasMasterRateObservation()) {
            const uint64_t prevStartMs = m_timelineUnit.prevMasterOutputStartMs();
            const int prevFrames       = m_timelineUnit.prevMasterOutputFrames();

            if(prevFrames > 0 && cycleSegment.startMs >= prevStartMs) {
                const auto outputDeltaMs = (static_cast<double>(prevFrames) * 1000.0) / static_cast<double>(outputRate);
                const auto sourceDeltaMs = static_cast<double>(cycleSegment.startMs - prevStartMs);

                if(outputDeltaMs > 0.0 && sourceDeltaMs > 0.0) {
                    observedScale    = sourceDeltaMs / outputDeltaMs;
                    hasObservedScale = true;
                }
            }
        }

        if(!hasObservedScale && currentOutputDurMs > 0.0 && currentSourceDurMs > 0.0) {
            observedScale    = currentSourceDurMs / currentOutputDurMs;
            hasObservedScale = true;
        }

        if(hasObservedScale && std::isfinite(observedScale)) {
            observedScale = std::clamp(observedScale, 0.01, 8.0);

            if(m_timelineUnit.hasMasterRateObservation()) {
                const double previousScale = std::clamp(m_timelineUnit.observedMasterRateScale(), 0.01, 8.0);
                observedScale              = std::clamp((previousScale * 0.85) + (observedScale * 0.15), 0.01, 8.0);
            }

            m_timelineUnit.setObservedMasterRateScale(observedScale);
            m_timelineUnit.setHasMasterRateObservation(true);
            m_timelineUnit.setPrevMasterOutputStartMs(cycleSegment.startMs);
            m_timelineUnit.setPrevMasterOutputFrames(cycleSegment.outputFrames);
        }
    }

    const bool mappedNow = m_timelineUnit.positionIsMapped();
    const auto basis     = mappedNow ? PositionBasis::RenderedSource : PositionBasis::DecodeHead;
    updatePlaybackDelay(stateWithWrites(state, framesWrittenThisCycle), basis);
    notifyDataDemand(m_renderer.needsMoreData());
}

void AudioPipeline::setOrphanState(StreamId streamId, uint64_t playbackDelayMs)
{
    if(streamId == InvalidStreamId) {
        clearOrphanState();
        return;
    }

    const StreamId replacedOrphanId = m_orphanTracker.replace(streamId);
    if(replacedOrphanId != InvalidStreamId) {
        m_renderer.removeStream(replacedOrphanId);
        m_streamRegistry.unregisterStream(replacedOrphanId);
    }

    const auto orphanStream = m_streamRegistry.getStream(streamId);
    if(!orphanStream) {
        clearOrphanState();
        return;
    }

    m_orphanTracker.armDrainDeadline(orphanStream->bufferedDurationMs(), playbackDelayMs, MinOrphanDrainTimeout,
                                     MaxOrphanDrainTimeout);
}

void AudioPipeline::clearOrphanState()
{
    m_orphanTracker.clear();
}

void AudioPipeline::cleanupOrphanIfDone()
{
    if(!m_orphanTracker.hasOrphan()) {
        return;
    }

    const StreamId orphanId = m_orphanTracker.streamId();
    const auto orphanStream = m_streamRegistry.getStream(orphanId);
    const auto eval         = m_orphanTracker.evaluate(orphanStream.get());

    if(!eval.shouldCleanup) {
        return;
    }

    if(eval.timedOut) {
        qCWarning(PIPELINE) << "Orphan stream drain timed out; forcing cleanup";
    }

    m_renderer.removeStream(orphanId);
    m_streamRegistry.unregisterStream(orphanId);
    clearOrphanState();
}
} // namespace Fooyin
