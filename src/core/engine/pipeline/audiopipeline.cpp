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
#include "core/engine/audioconverter.h"
#include "dsp/dspregistry.h"
#include <utils/timeconstants.h>

#include <QDebug>
#include <QLoggingCategory>
#include <QObject>
#include <QString>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <thread>

Q_LOGGING_CATEGORY(PIPELINE, "fy.pipeline")

using namespace Qt::StringLiterals;

constexpr auto MinOrphanDrainTimeout             = std::chrono::milliseconds{2000};
constexpr auto MaxOrphanDrainTimeout             = std::chrono::milliseconds{15000};
constexpr auto ProcessChunkTargetMs              = 10;
constexpr auto MinProcessChunkFrames             = 4096;
constexpr auto MaxProcessChunkFrames             = 1048576;
constexpr uint64_t MaxMappedPositionRegressionMs = 250;
constexpr auto CrossfadeFadeInCurve              = Fooyin::Engine::FadeCurve::Sine;
constexpr auto CrossfadeFadeOutCurve             = Fooyin::Engine::FadeCurve::Sine;

namespace {
std::chrono::milliseconds outputWriteBackoff(const Fooyin::AudioFormat& outputFormat,
                                             const Fooyin::OutputState& outputState)
{
    static constexpr int MinWaitMs{1};
    static constexpr int MaxWaitMs{20};

    if(!outputFormat.isValid() || outputFormat.sampleRate() <= 0) {
        return std::chrono::milliseconds{MinWaitMs};
    }

    // Wake when a small fraction of queued frames should have drained
    const int queuedFrames = std::max(1, outputState.queuedFrames);
    const int drainFrames  = std::max(1, queuedFrames / 4);
    const int waitMs       = static_cast<int>(
        std::llround(static_cast<double>(drainFrames) * 1000.0 / static_cast<double>(outputFormat.sampleRate())));

    return std::chrono::milliseconds{std::clamp(waitMs, MinWaitMs, MaxWaitMs)};
}

int processFrameChunkLimit(const Fooyin::AudioFormat& outputFormat)
{
    if(!outputFormat.isValid() || outputFormat.sampleRate() <= 0) {
        return MaxProcessChunkFrames;
    }

    const uint64_t desiredFrames
        = (static_cast<uint64_t>(outputFormat.sampleRate()) * static_cast<uint64_t>(ProcessChunkTargetMs)) / 1000ULL;
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

bool coalesceChunks(const Fooyin::ProcessingBufferList& chunks, const Fooyin::AudioFormat& outputFormat,
                    Fooyin::AudioBuffer& combined, const bool dither)
{
    if(!outputFormat.isValid()) {
        return false;
    }

    size_t totalBytes{0};
    uint64_t startTime{0};
    bool hasStartTime{false};

    for(size_t i{0}; i < chunks.count(); ++i) {
        const auto* chunk = chunks.item(i);
        if(!chunk || !chunk->isValid()) {
            continue;
        }

        const int frames = chunk->frameCount();
        if(frames <= 0) {
            continue;
        }

        const uint64_t bytes = outputFormat.bytesForFrames(frames);
        if(bytes == 0) {
            continue;
        }

        if(!hasStartTime) {
            startTime    = chunk->startTimeNs() / Fooyin::Time::NsPerMs;
            hasStartTime = true;
        }

        if(bytes > std::numeric_limits<size_t>::max() - totalBytes) {
            return false;
        }
        totalBytes += static_cast<size_t>(bytes);
    }

    if(totalBytes == 0 || !hasStartTime) {
        return false;
    }

    if(!combined.isValid() || combined.format() != outputFormat) {
        combined = Fooyin::AudioBuffer{outputFormat, startTime};
    }
    else {
        combined.setStartTime(startTime);
    }

    if(std::cmp_less(combined.byteCount(), totalBytes)) {
        combined.reserve(totalBytes);
    }

    combined.resize(totalBytes);

    size_t writeOffset{0};
    for(size_t i{0}; i < chunks.count(); ++i) {
        const auto* chunk = chunks.item(i);
        if(!chunk || !chunk->isValid()) {
            continue;
        }

        const int frames = chunk->frameCount();
        if(frames <= 0) {
            continue;
        }

        const uint64_t bytes = outputFormat.bytesForFrames(frames);
        if(bytes <= 0) {
            continue;
        }

        auto* outputData = combined.data() + writeOffset;

        if(chunk->format() == outputFormat) {
            const auto inputData  = chunk->constData();
            const auto inputBytes = std::as_bytes(inputData);
            if(inputBytes.size() < static_cast<size_t>(bytes)) {
                return false;
            }
            std::memcpy(outputData, inputBytes.data(), static_cast<size_t>(bytes));
        }
        else {
            const auto inputData  = chunk->constData();
            const auto inputBytes = std::as_bytes(inputData);
            if(!Fooyin::Audio::convert(chunk->format(), inputBytes.data(), outputFormat, outputData, frames, dither)) {
                return false;
            }
        }

        writeOffset += static_cast<size_t>(bytes);
    }

    return true;
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

} // namespace

namespace Fooyin {
AudioPipeline::AudioPipeline()
    : m_fadeEvents{MaxQueuedFadeEvents}
    , m_audioThreadIdHash{0}
    , m_masterVolume{1.0}
    , m_dspRegistry{nullptr}
    , m_position{0}
    , m_positionIsMapped{false}
    , m_playbackDelayMs{0}
    , m_playbackDelayToTrackScale{1.0}
    , m_fadeEventDropCount{0}
    , m_analysisBus{nullptr}
    , m_bufferFrames{0}
    , m_pendingOutputFrameOffset{0}
    , m_playbackState{PipelinePlaybackState::Stopped}
    , m_audioThreadReady{false}
    , m_running{false}
    , m_shutdownRequested{false}
    , m_playing{false}
    , m_outputInitialized{false}
    , m_outputSupportsVolume{true}
    , m_outputBitdepth{SampleFormat::Unknown}
    , m_ditherEnabled{false}
    , m_dataDemandNotified{false}
    , m_bufferUnderrun{false}
    , m_cycleMappedPositionValid{false}
    , m_cycleMappedPositionMs{0}
    , m_observedMasterRateScale{1.0}
    , m_prevMasterOutputStartMs{0}
    , m_prevMasterOutputFrames{0}
    , m_hasMasterRateObservation{false}
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

AudioBuffer AudioPipeline::sliceBufferFrames(const AudioBuffer& buffer, int frameOffset, int frameCount)
{
    if(!buffer.isValid() || frameOffset < 0 || frameCount <= 0) {
        return {};
    }

    const auto format        = buffer.format();
    const int bytesPerFrame  = format.bytesPerFrame();
    const int totalFrames    = buffer.frameCount();
    const int clampedOffset  = std::min(frameOffset, totalFrames);
    const int available      = std::max(0, totalFrames - clampedOffset);
    const int clampedFrames  = std::min(frameCount, available);
    const int startByte      = clampedOffset * bytesPerFrame;
    const int subspanBytes   = clampedFrames * bytesPerFrame;
    const uint64_t startTime = buffer.startTime() + format.durationForFrames(clampedOffset);

    if(bytesPerFrame <= 0 || subspanBytes <= 0) {
        return {};
    }

    const auto sourceData = buffer.constData();
    if(startByte < 0 || (static_cast<size_t>(startByte) + subspanBytes) > sourceData.size()) {
        return {};
    }

    return {sourceData.subspan(static_cast<size_t>(startByte), static_cast<size_t>(subspanBytes)), format, startTime};
}

int AudioPipeline::writeOutputBufferFrames(const AudioBuffer& buffer, int frameOffset)
{
    if(!buffer.isValid() || frameOffset < 0 || !m_output || !m_output->initialised()) {
        return 0;
    }

    const int totalFrames   = buffer.frameCount();
    const int bytesPerFrame = buffer.format().bytesPerFrame();
    if(totalFrames <= 0 || bytesPerFrame <= 0 || frameOffset >= totalFrames) {
        return 0;
    }

    AudioBuffer writeBuffer;
    const int remainingFrames = totalFrames - frameOffset;

    if(frameOffset == 0 && remainingFrames == totalFrames) {
        writeBuffer = buffer;
    }
    else {
        writeBuffer = sliceBufferFrames(buffer, frameOffset, remainingFrames);
    }

    if(!writeBuffer.isValid()) {
        return 0;
    }

    const int writtenFrames = std::clamp(m_output->write(writeBuffer), 0, remainingFrames);
    if(writtenFrames <= 0) {
        return 0;
    }

    return writtenFrames;
}

bool AudioPipeline::hasPendingOutput() const
{
    return m_pendingOutputBuffer.isValid() && m_pendingOutputFrameOffset < m_pendingOutputBuffer.frameCount();
}

void AudioPipeline::appendPendingOutput(const AudioBuffer& buffer)
{
    if(!buffer.isValid()) {
        return;
    }

    if(!hasPendingOutput()) {
        m_pendingOutputBuffer      = buffer;
        m_pendingOutputFrameOffset = 0;
        return;
    }

    if(m_pendingOutputBuffer.format() == buffer.format()) {
        m_pendingOutputBuffer.append(buffer.constData());
        return;
    }

    qCWarning(PIPELINE) << "Pending output format changed; dropping appended pending output";
}

void AudioPipeline::clearPendingOutput()
{
    m_pendingOutputBuffer      = {};
    m_pendingOutputFrameOffset = 0;
}

void AudioPipeline::resetMasterRateObservation()
{
    m_observedMasterRateScale  = 1.0;
    m_prevMasterOutputStartMs  = 0;
    m_prevMasterOutputFrames   = 0;
    m_hasMasterRateObservation = false;
}

void AudioPipeline::queueFadeEvent(const FadeEvent& event)
{
    if(m_fadeEvents.writer().write(&event, 1) == 1) {
        markPendingSignal(PendingSignal::FadeEventsReady);
        return;
    }

    ++m_fadeEventDropCount;

    if(m_fadeEventDropCount == 1 || (m_fadeEventDropCount % 16U) == 0U) {
        qCInfo(PIPELINE) << "Fade event queue full; dropped" << m_fadeEventDropCount << "events";
    }
}

void AudioPipeline::start()
{
    const std::scoped_lock lock{m_lifecycleMutex};

    if(m_running.load(std::memory_order_acquire)) {
        return;
    }

    m_audioThreadReady.store(false, std::memory_order_release);
    m_audioThreadIdHash.store(0, std::memory_order_release);
    m_shutdownRequested.store(false, std::memory_order_release);
    m_running.store(true, std::memory_order_release);
    m_thread = std::thread(&AudioPipeline::audioThreadFunc, this);
}

void AudioPipeline::stop()
{
    const std::scoped_lock lifecycleLock{m_lifecycleMutex};

    if(!m_running.load(std::memory_order_acquire)) {
        return;
    }

    m_shutdownRequested.store(true, std::memory_order_release);
    m_wakeCondition.notify_one();

    if(m_thread.joinable()) {
        m_thread.join();
    }

    m_running.store(false, std::memory_order_release);
    m_shutdownRequested.store(false, std::memory_order_release);
    m_audioThreadReady.store(false, std::memory_order_release);
    m_audioThreadIdHash.store(0, std::memory_order_release);

    if(m_output && m_output->initialised()) {
        m_output->uninit();
    }

    {
        const std::scoped_lock lock{m_commandMutex};
        m_commandQueue.clear();
    }

    m_playing.store(false, std::memory_order_release);
    m_playbackState.store(PipelinePlaybackState::Stopped, std::memory_order_release);
    m_outputInitialized.store(false, std::memory_order_release);
    m_bufferUnderrun.store(false, std::memory_order_release);
    m_position.store(0, std::memory_order_release);
    m_positionIsMapped.store(false, std::memory_order_release);
    m_playbackDelayMs.store(0, std::memory_order_release);
    m_playbackDelayToTrackScale.store(1.0, std::memory_order_release);
    m_cycleMappedPositionValid = false;
    m_cycleMappedPositionMs    = 0;

    m_inputFormat  = {};
    m_outputFormat = {};
    m_bufferFrames = 0;
    m_processBuffer.reset();
    m_processChunks.clear();
    clearPendingOutput();

    m_inputSnapshot.store(std::shared_ptr<const FormatSnapshot>{}, std::memory_order_release);
    m_outputSnapshot.store(std::shared_ptr<const FormatSnapshot>{}, std::memory_order_release);
    m_lastInitError.store(std::shared_ptr<const QString>{}, std::memory_order_release);

    m_streamRegistry.clear();
    clearOrphanState();
    resetQueuedNotifications(true);
}

void AudioPipeline::setOutput(std::unique_ptr<AudioOutput> output)
{
    onAudioThreadAsync([output = std::move(output)](AudioPipeline& pipeline) mutable {
        if(pipeline.m_output && pipeline.m_output->initialised()) {
            pipeline.m_output->uninit();
        }

        pipeline.m_output               = std::move(output);
        pipeline.m_outputSupportsVolume = !pipeline.m_output || pipeline.m_output->supportsVolumeControl();

        if(pipeline.m_output && pipeline.m_outputSupportsVolume) {
            pipeline.m_output->setVolume(pipeline.m_masterVolume);
        }

        pipeline.m_outputInitialized.store(false, std::memory_order_release);
        pipeline.m_outputFormat = {};
        pipeline.m_bufferFrames = 0;
        pipeline.m_positionIsMapped.store(false, std::memory_order_release);
        pipeline.m_playbackDelayMs.store(0, std::memory_order_release);
        pipeline.m_playbackDelayToTrackScale.store(1.0, std::memory_order_release);
        pipeline.m_outputSnapshot.store(std::shared_ptr<const FormatSnapshot>{}, std::memory_order_release);
        pipeline.m_cycleMappedPositionValid = false;
        pipeline.m_cycleMappedPositionMs    = 0;
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
        pipeline.m_mixer.addTrackProcessorFactory(std::move(factory));
    });
}

AudioFormat AudioPipeline::configureInputAndDspForFormat(const AudioFormat& format)
{
    m_inputFormat = format;

    const AudioFormat effectiveInput = perTrackOutputFormat(m_inputFormat);

    m_dspChain.prepare(effectiveInput);
    m_outputFormat = m_dspChain.outputFormat();
    if(!m_outputFormat.isValid()) {
        m_outputFormat = effectiveInput;
    }

    if(m_outputBitdepth != SampleFormat::Unknown) {
        m_outputFormat.setSampleFormat(m_outputBitdepth);
    }

    m_inputSnapshot.store(makeSnapshot(m_inputFormat), std::memory_order_release);
    m_outputSnapshot.store(makeSnapshot(m_outputFormat), std::memory_order_release);

    m_mixer.setFormat(m_inputFormat);
    m_mixer.setOutputFormat(effectiveInput);

    AudioFormat mixFormat = effectiveInput;
    mixFormat.setSampleFormat(SampleFormat::F64);
    m_processBuffer = ProcessingBuffer{mixFormat, 0};
    m_analysisScratch.clear();
    if(mixFormat.channelCount() > 0) {
        const size_t reserveSamples
            = static_cast<size_t>(mixFormat.channelCount()) * static_cast<size_t>(MinProcessChunkFrames);
        m_analysisScratch.reserve(reserveSamples);
    }
    clearPendingOutput();
    resetMasterRateObservation();

    return m_outputFormat;
}

bool AudioPipeline::init(const AudioFormat& format)
{
    if(!m_running.load(std::memory_order_acquire)) {
        setLastInitError(u"Audio pipeline is not running"_s);
        return false;
    }

    return enqueueAndWait([format](AudioPipeline& pipeline) -> bool {
        pipeline.setLastInitError({});
        pipeline.m_positionIsMapped.store(false, std::memory_order_release);
        pipeline.m_playbackDelayMs.store(0, std::memory_order_release);
        pipeline.m_playbackDelayToTrackScale.store(1.0, std::memory_order_release);
        pipeline.m_cycleMappedPositionValid = false;
        pipeline.m_cycleMappedPositionMs    = 0;
        pipeline.resetMasterRateObservation();
        pipeline.clearPendingOutput();
        pipeline.resetQueuedNotifications(false);

        if(!pipeline.m_output) {
            pipeline.setLastInitError(u"No audio output backend is available"_s);
            return false;
        }

        pipeline.configureInputAndDspForFormat(format);

        AudioFormat requestedOutput        = pipeline.m_outputFormat;
        const AudioFormat negotiatedOutput = pipeline.m_output->negotiateFormat(requestedOutput);
        if(negotiatedOutput.isValid() && negotiatedOutput.sampleRate() == requestedOutput.sampleRate()
           && negotiatedOutput.channelCount() == requestedOutput.channelCount()
           && negotiatedOutput.sampleFormat() == requestedOutput.sampleFormat()) {
            if(negotiatedOutput.hasChannelLayout()) {
                requestedOutput.setChannelLayout(negotiatedOutput.channelLayout());
            }
            else {
                requestedOutput.clearChannelLayout();
            }
            pipeline.m_outputFormat = requestedOutput;
            pipeline.m_outputSnapshot.store(makeSnapshot(pipeline.m_outputFormat), std::memory_order_release);
        }

        if(pipeline.m_output->init(pipeline.m_outputFormat)) {
            const AudioFormat actualFormat = pipeline.m_output->format();

            const bool incompatibleFormat = actualFormat.sampleRate() != pipeline.m_outputFormat.sampleRate()
                                         || actualFormat.channelCount() != pipeline.m_outputFormat.channelCount();

            if(incompatibleFormat) {
                const QString requestedDescription = describeFormat(pipeline.m_outputFormat);
                const QString actualDescription    = describeFormat(actualFormat);
                const QString error = u"Output format negotiation failed: requested %1, backend initialised %2"_s.arg(
                    requestedDescription, actualDescription);
                qCWarning(PIPELINE) << error;

                pipeline.setLastInitError(error);
                pipeline.m_output->uninit();
                pipeline.m_outputInitialized.store(false, std::memory_order_release);
                pipeline.m_outputFormat = {};
                pipeline.m_outputSnapshot.store(std::shared_ptr<const FormatSnapshot>{}, std::memory_order_release);
                pipeline.m_bufferFrames = 0;
                pipeline.m_positionIsMapped.store(false, std::memory_order_release);
                pipeline.m_playbackDelayMs.store(0, std::memory_order_release);
                pipeline.m_playbackDelayToTrackScale.store(1.0, std::memory_order_release);
                pipeline.m_cycleMappedPositionValid = false;
                pipeline.m_cycleMappedPositionMs    = 0;
                pipeline.clearPendingOutput();

                return false;
            }

            pipeline.setLastInitError({});
            pipeline.m_outputInitialized.store(true, std::memory_order_release);
            pipeline.m_outputFormat = actualFormat;
            pipeline.m_outputSnapshot.store(makeSnapshot(pipeline.m_outputFormat), std::memory_order_release);
            pipeline.m_bufferFrames = std::max(1, pipeline.m_output->bufferSize());

            if(pipeline.m_outputSupportsVolume) {
                pipeline.m_output->setVolume(pipeline.m_masterVolume);
            }

            return true;
        }

        const QString backendError = pipeline.m_output->error().trimmed();
        if(!backendError.isEmpty()) {
            pipeline.setLastInitError(backendError);
        }
        else {
            pipeline.setLastInitError(u"Audio output backend failed to initialize requested format: %1"_s.arg(
                describeFormat(pipeline.m_outputFormat)));
        }

        pipeline.m_outputInitialized.store(false, std::memory_order_release);
        pipeline.m_outputFormat = {};
        pipeline.m_outputSnapshot.store(std::shared_ptr<const FormatSnapshot>{}, std::memory_order_release);
        pipeline.m_positionIsMapped.store(false, std::memory_order_release);
        pipeline.m_playbackDelayMs.store(0, std::memory_order_release);
        pipeline.m_playbackDelayToTrackScale.store(1.0, std::memory_order_release);
        pipeline.m_cycleMappedPositionValid = false;
        pipeline.m_cycleMappedPositionMs    = 0;
        pipeline.resetMasterRateObservation();
        pipeline.clearPendingOutput();

        return false;
    });
}

AudioFormat AudioPipeline::applyInputFormat(const AudioFormat& format)
{
    return onAudioThread([format](AudioPipeline& pipeline) { return pipeline.configureInputAndDspForFormat(format); });
}

void AudioPipeline::uninit()
{
    if(!m_running.load(std::memory_order_acquire)) {
        return;
    }

    enqueueAndWait([](AudioPipeline& pipeline) {
        if(pipeline.m_output && pipeline.m_output->initialised()) {
            pipeline.m_output->uninit();
        }

        pipeline.m_outputInitialized.store(false, std::memory_order_release);
        pipeline.m_playbackState.store(PipelinePlaybackState::Stopped, std::memory_order_release);
        pipeline.m_outputFormat = {};
        pipeline.m_bufferFrames = 0;
        pipeline.m_positionIsMapped.store(false, std::memory_order_release);
        pipeline.m_playbackDelayMs.store(0, std::memory_order_release);
        pipeline.m_playbackDelayToTrackScale.store(1.0, std::memory_order_release);
        pipeline.m_outputSnapshot.store(std::shared_ptr<const FormatSnapshot>{}, std::memory_order_release);
        pipeline.m_cycleMappedPositionValid = false;
        pipeline.m_cycleMappedPositionMs    = 0;
        pipeline.resetMasterRateObservation();
        pipeline.clearPendingOutput();
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
        pipeline.m_playing.store(true, std::memory_order_release);
        pipeline.m_playbackState.store(PipelinePlaybackState::Playing, std::memory_order_release);

        if(pipeline.m_output && pipeline.m_output->initialised()) {
            pipeline.m_output->setPaused(false);
            pipeline.m_output->start();
        }

        pipeline.m_mixer.resumeAll();
    });
}

void AudioPipeline::pause()
{
    enqueueAsync([](AudioPipeline& pipeline) {
        pipeline.m_playing.store(false, std::memory_order_release);
        pipeline.m_playbackState.store(PipelinePlaybackState::Paused, std::memory_order_release);

        if(pipeline.m_output && pipeline.m_output->initialised()) {
            pipeline.m_output->setPaused(true);
        }

        pipeline.m_mixer.pauseAll();
    });
}

void AudioPipeline::stopPlayback()
{
    enqueueAsync([](AudioPipeline& pipeline) {
        pipeline.m_playing.store(false, std::memory_order_release);
        pipeline.m_playbackState.store(PipelinePlaybackState::Stopped, std::memory_order_release);

        if(pipeline.m_output && pipeline.m_output->initialised()) {
            (*pipeline.m_output).reset();
        }

        {
            ProcessingBufferList chunks;
            pipeline.m_dspChain.flush(chunks, DspNode::FlushMode::Flush);
        }

        pipeline.m_mixer.clear();

        if(pipeline.m_orphanTracker.hasOrphan()) {
            pipeline.m_streamRegistry.unregisterStream(pipeline.m_orphanTracker.streamId());
        }

        pipeline.clearOrphanState();
        pipeline.m_position.store(0, std::memory_order_release);
        pipeline.m_positionIsMapped.store(false, std::memory_order_release);
        pipeline.m_playbackDelayMs.store(0, std::memory_order_release);
        pipeline.m_playbackDelayToTrackScale.store(1.0, std::memory_order_release);
        pipeline.m_cycleMappedPositionValid = false;
        pipeline.m_cycleMappedPositionMs    = 0;
        pipeline.resetMasterRateObservation();
        pipeline.clearPendingOutput();
        pipeline.resetQueuedNotifications(false);
    });
}

void AudioPipeline::resetOutput()
{
    enqueueAsync([](AudioPipeline& pipeline) {
        if(pipeline.m_output && pipeline.m_output->initialised()) {
            (*pipeline.m_output).reset();
        }

        pipeline.resetMasterRateObservation();
        pipeline.clearPendingOutput();
    });
}

void AudioPipeline::flushDsp(DspNode::FlushMode mode)
{
    enqueueAsync([mode](AudioPipeline& pipeline) {
        if(!pipeline.m_outputInitialized.load(std::memory_order_acquire) || !pipeline.m_output
           || !pipeline.m_output->initialised()) {
            return;
        }

        ProcessingBufferList chunks;
        pipeline.m_dspChain.flush(chunks, mode);

        if(!pipeline.m_outputSupportsVolume) {
            applyMasterVolume(chunks, pipeline.m_masterVolume);
        }

        if(coalesceChunks(chunks, pipeline.m_outputFormat, pipeline.m_coalescedOutputBuffer,
                          pipeline.m_ditherEnabled)) {
            const auto& combinedBuffer = pipeline.m_coalescedOutputBuffer;
            if(pipeline.hasPendingOutput()) {
                pipeline.appendPendingOutput(combinedBuffer);
                return;
            }

            const int writtenFrames = pipeline.writeOutputBufferFrames(combinedBuffer, 0);

            if(writtenFrames < combinedBuffer.frameCount()) {
                pipeline.m_pendingOutputBuffer = AudioPipeline::sliceBufferFrames(
                    combinedBuffer, writtenFrames, combinedBuffer.frameCount() - writtenFrames);
                pipeline.m_pendingOutputFrameOffset = 0;
            }
        }
    });
}

void AudioPipeline::setOutputVolume(double volume)
{
    const double clampedVolume = std::clamp(volume, 0.0, 1.0);

    onAudioThreadAsync([clampedVolume](AudioPipeline& pipeline) {
        pipeline.m_masterVolume = clampedVolume;

        if(pipeline.m_output && pipeline.m_outputSupportsVolume) {
            pipeline.m_output->setVolume(clampedVolume);
        }
    });
}

void AudioPipeline::setOutputDevice(const QString& device)
{
    onAudioThreadAsync([device](AudioPipeline& pipeline) {
        if(pipeline.m_output) {
            pipeline.m_output->setDevice(device);
        }
    });
}

AudioFormat AudioPipeline::setOutputBitdepth(const SampleFormat bitdepth)
{
    return onAudioThread([bitdepth](AudioPipeline& pipeline) {
        pipeline.m_outputBitdepth = bitdepth;

        if(pipeline.m_outputBitdepth != SampleFormat::Unknown) {
            pipeline.m_outputFormat.setSampleFormat(pipeline.m_outputBitdepth);
        }

        pipeline.m_outputSnapshot.store(makeSnapshot(pipeline.m_outputFormat), std::memory_order_release);

        return pipeline.m_outputFormat;
    });
}

void AudioPipeline::setDither(const bool enabled)
{
    onAudioThreadAsync([enabled](AudioPipeline& pipeline) { pipeline.m_ditherEnabled = enabled; });
}

AudioFormat AudioPipeline::setDspChain(std::vector<DspNodePtr> masterNodes, const Engine::DspChain& perTrackDefs,
                                       const AudioFormat& inputFormat)
{
    return onAudioThread([nodes = std::move(masterNodes), perTrackDefs, inputFormat](AudioPipeline& pipeline) mutable {
        pipeline.m_perTrackChainDefs = perTrackDefs;
        pipeline.m_dspChain.replaceNodes(std::move(nodes));

        const AudioFormat effectiveInput = pipeline.perTrackOutputFormat(inputFormat);

        if(pipeline.m_inputFormat.isValid()) {
            pipeline.m_dspChain.prepare(effectiveInput);
            pipeline.m_outputFormat = pipeline.m_dspChain.outputFormat();

            if(pipeline.m_outputBitdepth != SampleFormat::Unknown) {
                pipeline.m_outputFormat.setSampleFormat(pipeline.m_outputBitdepth);
            }

            pipeline.m_outputSnapshot.store(makeSnapshot(pipeline.m_outputFormat), std::memory_order_release);
        }

        pipeline.m_mixer.setOutputFormat(effectiveInput);
        pipeline.m_mixer.rebuildAllPerTrackDsps([&pipeline]() { return pipeline.buildPerTrackNodes(); });
        pipeline.resetMasterRateObservation();

        return pipeline.m_dspChain.outputFormatFor(effectiveInput);
    });
}

AudioFormat AudioPipeline::predictPerTrackFormat(const AudioFormat& inputFormat) const
{
    return onAudioThread(
        [inputFormat](const AudioPipeline& pipeline) { return pipeline.perTrackOutputFormat(inputFormat); });
}

AudioFormat AudioPipeline::predictOutputFormat(const AudioFormat& inputFormat) const
{
    return onAudioThread([inputFormat](const AudioPipeline& pipeline) {
        AudioFormat effectiveInput = pipeline.perTrackOutputFormat(inputFormat);

        if(pipeline.m_outputBitdepth != SampleFormat::Unknown) {
            effectiveInput.setSampleFormat(pipeline.m_outputBitdepth);
        }

        return pipeline.m_dspChain.outputFormatFor(effectiveInput);
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

void AudioPipeline::resetStreamForSeek(StreamId id)
{
    onAudioThread([id](AudioPipeline& pipeline) {
        if(auto stream = pipeline.m_streamRegistry.getStream(id)) {
            stream->applyCommand(AudioStream::Command::ResetForSeek);
        }

        pipeline.m_mixer.resetStreamForSeek(id);
        pipeline.m_dspChain.reset();
        pipeline.clearPendingOutput();
        pipeline.resetMasterRateObservation();
    });
}

void AudioPipeline::setFaderCurve(Engine::FadeCurve curve)
{
    enqueueAsync([curve](AudioPipeline& pipeline) { pipeline.m_outputFader.setFadeCurve(curve); });
}

void AudioPipeline::faderFadeIn(int durationMs, double targetVolume, uint64_t token)
{
    enqueueAsync([durationMs, targetVolume, token](AudioPipeline& pipeline) {
        const int sampleRate = pipeline.m_outputFormat.isValid() ? pipeline.m_outputFormat.sampleRate()
                                                                 : pipeline.m_inputFormat.sampleRate();
        pipeline.m_outputFader.fadeIn(durationMs, targetVolume, sampleRate, token);
    });
}

void AudioPipeline::faderFadeOut(int durationMs, double currentVolume, uint64_t token)
{
    enqueueAsync([durationMs, currentVolume, token](AudioPipeline& pipeline) {
        const int sampleRate = pipeline.m_outputFormat.isValid() ? pipeline.m_outputFormat.sampleRate()
                                                                 : pipeline.m_inputFormat.sampleRate();
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

void AudioPipeline::clearPendingSignals(const uint32_t signalMask)
{
    m_signalMailbox.clear(signalMask);
}

void AudioPipeline::resetQueuedNotifications(const bool resetWakeState)
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

void AudioPipeline::addStream(StreamId id)
{
    enqueueAsync([id](AudioPipeline& pipeline) {
        auto stream = pipeline.m_streamRegistry.getStream(id);
        if(!stream) {
            qCWarning(PIPELINE) << "addStream: stream not found in registry" << id;
            return;
        }

        pipeline.m_mixer.addStream(std::move(stream), pipeline.buildPerTrackNodes());
    });
}

void AudioPipeline::removeStream(StreamId id)
{
    enqueueAsync([id](AudioPipeline& pipeline) { pipeline.m_mixer.removeStream(id); });
}

void AudioPipeline::switchToStream(StreamId id)
{
    enqueueAsync([id](AudioPipeline& pipeline) {
        auto stream = pipeline.m_streamRegistry.getStream(id);
        if(!stream) {
            qCWarning(PIPELINE) << "switchToStream: stream not found in registry" << id;
            return;
        }

        if(pipeline.m_orphanTracker.hasOrphan() && pipeline.m_orphanTracker.streamId() != id) {
            pipeline.m_streamRegistry.unregisterStream(pipeline.m_orphanTracker.streamId());
        }

        pipeline.clearOrphanState();
        pipeline.m_mixer.switchTo(std::move(stream), pipeline.buildPerTrackNodes());
        pipeline.resetMasterRateObservation();
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
        if(pipeline.m_orphanTracker.hasOrphan()) {
            const StreamId orphanId = pipeline.m_orphanTracker.streamId();
            pipeline.m_mixer.removeStream(orphanId);
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
                auto currentPrimary = pipeline.m_mixer.primaryStream();

                targetStream->setFadeCurve(CrossfadeFadeInCurve);

                if(!pipeline.m_mixer.hasStream(newStreamId)) {
                    pipeline.m_mixer.addStream(targetStream, pipeline.buildPerTrackNodes());
                }

                if(currentPrimary && currentPrimary->state() != AudioStream::State::Stopped) {
                    if(!request.skipFadeOutStart) {
                        currentPrimary->setFadeCurve(CrossfadeFadeOutCurve);
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

                return true;
            }
            case TransitionType::Gapless: {
                if(!pipeline.m_mixer.hasStream(newStreamId)) {
                    pipeline.m_mixer.addStream(targetStream, pipeline.buildPerTrackNodes());
                }

                auto currentPrimary = pipeline.m_mixer.primaryStream();
                if(!currentPrimary || currentPrimary->id() == newStreamId) {
                    targetStream->applyCommand(AudioStream::Command::Play);
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
        pipeline.m_mixer.removeStream(orphanId);
        pipeline.m_streamRegistry.unregisterStream(orphanId);
        pipeline.clearOrphanState();
    };

    onAudioThread(cleanup);
}

PipelineStatus AudioPipeline::currentStatus() const
{
    PipelineStatus status;

    status.position         = m_position.load(std::memory_order_acquire);
    status.positionIsMapped = m_positionIsMapped.load(std::memory_order_acquire);
    status.state            = m_playbackState.load(std::memory_order_acquire);
    status.bufferUnderrun   = m_bufferUnderrun.load(std::memory_order_acquire);

    return status;
}

uint64_t AudioPipeline::playbackDelayMs() const
{
    return m_playbackDelayMs.load(std::memory_order_acquire);
}

double AudioPipeline::playbackDelayToTrackScale() const
{
    return m_playbackDelayToTrackScale.load(std::memory_order_acquire);
}

bool AudioPipeline::isRunning() const
{
    return m_running.load(std::memory_order_acquire);
}

bool AudioPipeline::hasOutput() const
{
    return m_outputInitialized.load(std::memory_order_acquire);
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

size_t AudioPipeline::drainFadeEvents(FadeEvent* outEvents, const size_t capacity)
{
    if(!outEvents || capacity == 0) {
        return 0;
    }

    return m_fadeEvents.reader().read(outEvents, capacity);
}

void AudioPipeline::audioThreadFunc()
{
    m_audioThreadIdHash.store(currentThreadIdHash(), std::memory_order_release);
    m_audioThreadReady.store(true, std::memory_order_release);

    while(true) {
        const bool hasMoreCommands = processCommands();

        if(m_shutdownRequested.load(std::memory_order_acquire)) {
            break;
        }

        if(m_playing.load(std::memory_order_acquire) && m_outputInitialized.load(std::memory_order_acquire) && m_output
           && m_output->initialised()) {
            processAudio();
        }
        else if(hasMoreCommands) {
            std::this_thread::yield();
        }
        else {
            std::unique_lock lock{m_wakeMutex};
            m_wakeCondition.wait_for(lock, std::chrono::milliseconds(100));
        }
    }

    m_audioThreadReady.store(false, std::memory_order_release);
    m_audioThreadIdHash.store(0, std::memory_order_release);
}

bool AudioPipeline::processCommands()
{
    std::deque<PipelineCommand> commands;

    bool hasMoreCommands{false};
    {
        const std::scoped_lock lock{m_commandMutex};
        const size_t commandCount = std::min(m_commandQueue.size(), MaxCommandsPerCycle);

        for(size_t i{0}; i < commandCount; ++i) {
            commands.push_back(std::move(m_commandQueue.front()));
            m_commandQueue.pop_front();
        }

        hasMoreCommands = !m_commandQueue.empty();
    }

    for(auto& command : commands) {
        if(command) {
            command(*this);
        }
    }

    return hasMoreCommands;
}

bool AudioPipeline::enqueueCommand(PipelineCommand cmd) const
{
    if(!m_running.load(std::memory_order_acquire) || m_shutdownRequested.load(std::memory_order_acquire)) {
        return false;
    }

    {
        const std::scoped_lock lock{m_commandMutex};
        if(m_commandQueue.size() >= MaxQueuedCommands) {
            qWarning() << "AudioPipeline command queue full; dropping command";
            return false;
        }
        m_commandQueue.push_back(std::move(cmd));
    }

    m_wakeCondition.notify_one();
    return true;
}

uint64_t AudioPipeline::currentThreadIdHash()
{
    return static_cast<uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

bool AudioPipeline::isAudioThread() const
{
    if(!m_audioThreadReady.load(std::memory_order_acquire)) {
        return false;
    }

    return m_audioThreadIdHash.load(std::memory_order_acquire) == currentThreadIdHash();
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

    if(!m_outputInitialized.load(std::memory_order_acquire) || !m_output || !m_output->initialised()) {
        return;
    }

    const auto state = m_output->currentState();
    int freeFrames   = state.freeFrames;
    int framesWrittenThisCycle{0};

    if(drainPendingOutput(state, freeFrames, framesWrittenThisCycle)) {
        return;
    }

    if(handleNoFreeFrames(state, freeFrames, framesWrittenThisCycle)) {
        return;
    }

    const AudioFormat inputFormat = m_processBuffer.format();
    const int channels            = inputFormat.channelCount();
    const int outputFramesTarget  = std::min({freeFrames, m_bufferFrames, processFrameChunkLimit(m_outputFormat)});
    int framesToProcess           = inputFramesForOutputFrames(inputFormat, m_outputFormat, outputFramesTarget);
    const int inputFrameCap       = MaxProcessChunkFrames;

    if(framesToProcess > inputFrameCap) {
        qCInfo(PIPELINE) << "Input processing chunk capped below output demand:"
                         << "requestedInputFrames=" << framesToProcess << "cappedInputFrames=" << inputFrameCap
                         << "targetOutputFrames=" << outputFramesTarget << "inputRate=" << inputFormat.sampleRate()
                         << "outputRate=" << m_outputFormat.sampleRate();
        framesToProcess = inputFrameCap;
    }

    if(framesToProcess <= 0 || channels <= 0) {
        notifyDataDemand(false);
        return;
    }

    const auto samplesToProcess = static_cast<size_t>(framesToProcess) * channels;
    m_processBuffer.resizeSamples(samplesToProcess);

    const int framesRead = m_mixer.read(m_processBuffer, framesToProcess);

    if(framesRead <= 0) {
        handleMixerUnderrun(state, framesWrittenThisCycle);
        return;
    }

    m_bufferUnderrun.store(false, std::memory_order_release);

    if(framesRead != framesToProcess) {
        const auto samplesRead = static_cast<size_t>(framesRead) * channels;
        m_processBuffer.resizeSamples(samplesRead);
    }

    applyMasterProcessing();
    tapAnalysis();
    writeToOutput(framesWrittenThisCycle);
    updatePlaybackState(state, framesWrittenThisCycle);

    cleanupOrphanIfDone();
}

OutputState AudioPipeline::stateWithWrites(const OutputState& state, const int framesWrittenThisCycle) const
{
    OutputState adjusted{state};
    adjusted.queuedFrames = std::clamp(std::max(0, state.queuedFrames) + framesWrittenThisCycle, 0, m_bufferFrames);

    if(m_outputFormat.sampleRate() > 0) {
        const double queuedDelay
            = static_cast<double>(adjusted.queuedFrames) / static_cast<double>(m_outputFormat.sampleRate());
        adjusted.delay = std::max(adjusted.delay, queuedDelay);
    }

    return adjusted;
}

void AudioPipeline::updatePlaybackDelay(const OutputState& outputState, const PositionBasis basis)
{
    uint64_t outputDelayMs{0};
    uint64_t dspDelayMs{0};
    double delayToTrackScale{1.0};

    if(m_outputFormat.isValid()) {
        const int queuedFrames = std::max(0, outputState.queuedFrames);
        const auto queuedMs    = m_outputFormat.durationForFrames(queuedFrames);
        const uint64_t deviceDelayMs
            = outputState.delay > 0.0 ? static_cast<uint64_t>(std::llround(outputState.delay * 1000.0)) : 0;

        // Some outputs provide explicit device delay, others only queued frames
        outputDelayMs = std::max(queuedMs, deviceDelayMs);

        if(basis == PositionBasis::DecodeHead) {
            const double dspLatencySeconds
                = std::max(0.0, m_dspChain.totalLatencySeconds() + m_mixer.primaryStreamDspLatencySeconds());
            dspDelayMs = static_cast<uint64_t>(std::llround(dspLatencySeconds * 1000.0));
        }

        delayToTrackScale = std::clamp(m_observedMasterRateScale, 0.05, 8.0);
    }

    uint64_t totalDelayMs{outputDelayMs};

    if(totalDelayMs > std::numeric_limits<uint64_t>::max() - dspDelayMs) {
        totalDelayMs = std::numeric_limits<uint64_t>::max();
    }
    else {
        totalDelayMs += dspDelayMs;
    }

    m_playbackDelayMs.store(totalDelayMs, std::memory_order_release);
    m_playbackDelayToTrackScale.store(delayToTrackScale, std::memory_order_release);
}

bool AudioPipeline::drainPendingOutput(const OutputState& state, int& freeFrames, int& framesWrittenThisCycle)
{
    if(!hasPendingOutput()) {
        return false;
    }

    const int pendingWritten = writeOutputBufferFrames(m_pendingOutputBuffer, m_pendingOutputFrameOffset);
    if(pendingWritten > 0) {
        framesWrittenThisCycle += pendingWritten;
        m_pendingOutputFrameOffset += pendingWritten;
        freeFrames = std::max(0, freeFrames - pendingWritten);

        if(m_pendingOutputFrameOffset >= m_pendingOutputBuffer.frameCount()) {
            clearPendingOutput();
        }
    }

    if(!hasPendingOutput()) {
        return false;
    }

    const auto outputStateWithWrites = stateWithWrites(state, framesWrittenThisCycle);
    const auto basis = m_positionIsMapped.load(std::memory_order_acquire) ? PositionBasis::RenderedSource
                                                                          : PositionBasis::DecodeHead;
    updatePlaybackDelay(outputStateWithWrites, basis);

    if(freeFrames <= 0 || pendingWritten <= 0) {
        const auto waitDuration = outputWriteBackoff(m_outputFormat, outputStateWithWrites);
        std::unique_lock lock{m_wakeMutex};
        m_wakeCondition.wait_for(lock, waitDuration);
    }

    notifyDataDemand(false);
    return true;
}

bool AudioPipeline::handleNoFreeFrames(const OutputState& state, const int freeFrames, const int framesWrittenThisCycle)
{
    if(freeFrames > 0) {
        return false;
    }

    const auto outputStateWithWrites = stateWithWrites(state, framesWrittenThisCycle);
    const auto basis = m_positionIsMapped.load(std::memory_order_acquire) ? PositionBasis::RenderedSource
                                                                          : PositionBasis::DecodeHead;
    updatePlaybackDelay(outputStateWithWrites, basis);

    const auto waitDuration = outputWriteBackoff(m_outputFormat, outputStateWithWrites);

    std::unique_lock lock{m_wakeMutex};
    m_wakeCondition.wait_for(lock, waitDuration);

    notifyDataDemand(false);
    return true;
}

bool AudioPipeline::handleMixerUnderrun(const OutputState& state, const int framesWrittenThisCycle)
{
    const auto outputStateWithWrites = stateWithWrites(state, framesWrittenThisCycle);
    const auto basis = m_positionIsMapped.load(std::memory_order_acquire) ? PositionBasis::RenderedSource
                                                                          : PositionBasis::DecodeHead;
    updatePlaybackDelay(outputStateWithWrites, basis);

    if(m_mixer.streamCount() > 0) {
        m_bufferUnderrun.store(true, std::memory_order_release);
        notifyDataDemand(true);

        if(m_orphanTracker.hasOrphan()) {
            qCWarning(PIPELINE) << "Output underrun during active transition:" << "orphanId="
                                << m_orphanTracker.streamId() << "queuedFrames=" << outputStateWithWrites.queuedFrames
                                << "freeFrames=" << outputStateWithWrites.freeFrames
                                << "delayMs=" << static_cast<int>(std::llround(outputStateWithWrites.delay * 1000.0));
        }
    }
    else {
        notifyDataDemand(false);
    }

    const auto waitDuration = outputWriteBackoff(m_outputFormat, outputStateWithWrites);

    std::unique_lock lock{m_wakeMutex};
    m_wakeCondition.wait_for(lock, waitDuration);

    return true;
}

void AudioPipeline::applyMasterProcessing()
{
    // Process through DSP chain
    m_processChunks.setToSingle(m_processBuffer);
    m_dspChain.process(m_processChunks);
    m_outputFader.process(m_processChunks);

    if(!m_outputSupportsVolume) {
        applyMasterVolume(m_processChunks, m_masterVolume);
    }

    if(const auto completion = m_outputFader.takeCompletion()) {
        queueFadeEvent(FadeEvent{.type = completion->type, .token = completion->token});
    }
}

void AudioPipeline::tapAnalysis()
{
    auto* analysisBus = m_analysisBus.load(std::memory_order_acquire);
    if(!analysisBus || m_processChunks.count() == 0
       || !analysisBus->hasSubscription(Engine::AnalysisDataType::LevelFrameData)) {
        return;
    }

    int channelCount{0};
    int sampleRate{0};
    uint64_t streamTimeMs{0};
    bool haveTiming{false};
    size_t totalSamples{0};

    for(size_t i{0}; i < m_processChunks.count(); ++i) {
        const auto* chunk = m_processChunks.item(i);
        if(!chunk || !chunk->isValid() || chunk->sampleCount() <= 0) {
            continue;
        }

        const auto format = chunk->format();
        if(!format.isValid() || format.sampleFormat() != SampleFormat::F64 || format.channelCount() <= 0
           || format.channelCount() > LevelFrame::MaxChannels || format.sampleRate() <= 0) {
            continue;
        }

        if(!haveTiming) {
            channelCount = format.channelCount();
            sampleRate   = format.sampleRate();
            streamTimeMs = chunk->startTimeNs() / Fooyin::Time::NsPerMs;
            haveTiming   = true;
        }
        else if(format.channelCount() != channelCount || format.sampleRate() != sampleRate) {
            return;
        }

        totalSamples += static_cast<size_t>(chunk->sampleCount());
    }

    if(!haveTiming || totalSamples == 0) {
        return;
    }

    if(m_analysisScratch.size() < totalSamples) {
        m_analysisScratch.resize(totalSamples);
    }

    size_t writeOffset{0};
    for(size_t i{0}; i < m_processChunks.count(); ++i) {
        const auto* chunk = m_processChunks.item(i);
        if(!chunk || !chunk->isValid() || chunk->sampleCount() <= 0) {
            continue;
        }

        const auto format = chunk->format();
        if(!format.isValid() || format.sampleFormat() != SampleFormat::F64 || format.channelCount() != channelCount
           || format.sampleRate() != sampleRate) {
            continue;
        }

        const auto samples = chunk->constData();
        std::transform(samples.begin(), samples.end(),
                       m_analysisScratch.begin() + static_cast<std::ptrdiff_t>(writeOffset),
                       [](double sample) { return static_cast<float>(sample); });
        writeOffset += samples.size();
    }

    if(writeOffset == 0) {
        return;
    }

    const auto playbackDelayMs  = m_playbackDelayMs.load(std::memory_order_acquire);
    const auto presentationTime = AudioAnalysisBus::Clock::now() + std::chrono::milliseconds{playbackDelayMs};
    analysisBus->push(std::span<const float>{m_analysisScratch.data(), writeOffset}, channelCount, sampleRate,
                      streamTimeMs, presentationTime);
}

void AudioPipeline::writeToOutput(int& framesWrittenThisCycle)
{
    m_cycleMappedPositionValid = false;
    m_cycleMappedPositionMs    = 0;

    if(!coalesceChunks(m_processChunks, m_outputFormat, m_coalescedOutputBuffer, m_ditherEnabled)) {
        return;
    }

    const auto& combinedBuffer = m_coalescedOutputBuffer;
    const int writtenFrames    = writeOutputBufferFrames(combinedBuffer, 0);
    framesWrittenThisCycle += writtenFrames;

    double masterScale = std::clamp(m_observedMasterRateScale, 0.05, 8.0);

    if(writtenFrames > 0) {
        uint64_t cycleStartMs{0};
        if(sourceTimelinePositionForFrameOffset(m_processChunks, 0, masterScale, cycleStartMs)) {
            if(m_hasMasterRateObservation && m_prevMasterOutputFrames > 0 && m_outputFormat.sampleRate() > 0
               && cycleStartMs > m_prevMasterOutputStartMs) {
                const double sourceAdvanceMs = static_cast<double>(cycleStartMs - m_prevMasterOutputStartMs);
                const double outputAdvanceMs = (static_cast<double>(m_prevMasterOutputFrames) * 1000.0)
                                             / static_cast<double>(m_outputFormat.sampleRate());

                if(outputAdvanceMs > 0.0) {
                    const double observedScale = sourceAdvanceMs / outputAdvanceMs;
                    if(std::isfinite(observedScale)) {
                        m_observedMasterRateScale = std::clamp(observedScale, 0.05, 8.0);
                    }
                }
            }

            m_prevMasterOutputStartMs  = cycleStartMs;
            m_prevMasterOutputFrames   = writtenFrames;
            m_hasMasterRateObservation = true;
            masterScale                = std::clamp(m_observedMasterRateScale, 0.05, 8.0);
        }

        uint64_t mappedPositionMs{0};
        if(sourceTimelinePositionForFrameOffset(m_processChunks, writtenFrames, masterScale, mappedPositionMs)) {
            m_cycleMappedPositionValid = true;
            m_cycleMappedPositionMs    = mappedPositionMs;
        }
    }

    if(writtenFrames < combinedBuffer.frameCount()) {
        m_pendingOutputBuffer
            = sliceBufferFrames(combinedBuffer, writtenFrames, combinedBuffer.frameCount() - writtenFrames);
        if(m_pendingOutputBuffer.isValid()) {
            uint64_t pendingStartMs{0};
            if(sourceTimelinePositionForFrameOffset(m_processChunks, writtenFrames, masterScale, pendingStartMs)) {
                m_pendingOutputBuffer.setStartTime(pendingStartMs);
            }
        }
        m_pendingOutputFrameOffset = 0;
    }
}

bool AudioPipeline::sourceTimelinePositionForFrameOffset(const ProcessingBufferList& chunks, int frameOffset,
                                                         double fallbackScale, uint64_t& positionMsOut)
{
    if(frameOffset < 0) {
        return false;
    }

    m_timelineChunksScratch.clear();
    m_timelineChunksScratch.reserve(chunks.count());

    for(size_t i{0}; i < chunks.count(); ++i) {
        const auto* chunk = chunks.item(i);
        if(!chunk || !chunk->isValid()) {
            continue;
        }

        const int frames = chunk->frameCount();
        if(frames <= 0) {
            continue;
        }

        const auto format = chunk->format();
        if(!format.isValid() || format.sampleRate() <= 0) {
            continue;
        }

        m_timelineChunksScratch.push_back(
            TimelineChunk{.startNs = chunk->startTimeNs(), .frames = frames, .sampleRate = format.sampleRate()});
    }

    if(m_timelineChunksScratch.empty()) {
        return false;
    }

    const double clampedScale = std::clamp(fallbackScale, 0.01, 8.0);
    int remaining             = frameOffset;

    for(size_t idx{0}; idx < m_timelineChunksScratch.size(); ++idx) {
        const auto& chunk = m_timelineChunksScratch[idx];
        if(chunk.frames <= 0) {
            continue;
        }

        const bool hasNextChunk = (idx + 1U) < m_timelineChunksScratch.size();
        uint64_t frameNs{0};

        if(hasNextChunk) {
            const auto& nextChunk = m_timelineChunksScratch[idx + 1U];
            if(nextChunk.startNs >= chunk.startNs) {
                const uint64_t deltaNs = nextChunk.startNs - chunk.startNs;
                frameNs                = deltaNs / static_cast<uint64_t>(chunk.frames);
            }
        }

        if(frameNs == 0) {
            const auto roundedNs
                = std::llround((Fooyin::Time::NsPerSecond * clampedScale) / static_cast<double>(chunk.sampleRate));
            frameNs = static_cast<uint64_t>(std::max<int64_t>(1, roundedNs));
        }

        const int takeFrames = std::min(remaining, chunk.frames);
        if(takeFrames < chunk.frames || !hasNextChunk) {
            const uint64_t offsetNs = frameNs * static_cast<uint64_t>(std::max(0, takeFrames));
            positionMsOut           = (chunk.startNs + offsetNs) / Fooyin::Time::NsPerMs;
            return true;
        }

        remaining -= takeFrames;
        if(remaining <= 0) {
            positionMsOut = m_timelineChunksScratch[idx + 1U].startNs / Fooyin::Time::NsPerMs;
            return true;
        }
    }

    const auto& tailChunk = m_timelineChunksScratch.back();
    const auto roundedTailNs
        = std::llround((Fooyin::Time::NsPerSecond * clampedScale) / static_cast<double>(tailChunk.sampleRate));
    const uint64_t frameNs = static_cast<uint64_t>(std::max<int64_t>(1, roundedTailNs));
    positionMsOut          = (tailChunk.startNs + (frameNs * static_cast<uint64_t>(remaining))) / Fooyin::Time::NsPerMs;
    return true;
}

void AudioPipeline::updatePlaybackState(const OutputState& state, const int framesWrittenThisCycle)
{
    const bool wasMapped   = m_positionIsMapped.load(std::memory_order_acquire);
    bool acceptCycleMapped = m_cycleMappedPositionValid;
    if(acceptCycleMapped && wasMapped) {
        const uint64_t currentPos = m_position.load(std::memory_order_acquire);
        if(m_cycleMappedPositionMs + MaxMappedPositionRegressionMs < currentPos) {
            bool allowBackwardJump{false};
            if(const auto stream = m_mixer.primaryStream()) {
                const uint64_t streamPos = stream->positionMs();
                allowBackwardJump        = streamPos + MaxMappedPositionRegressionMs < currentPos;
            }
            if(!allowBackwardJump) {
                acceptCycleMapped = false;
            }
        }
    }

    if(acceptCycleMapped) {
        m_position.store(m_cycleMappedPositionMs, std::memory_order_release);
        m_positionIsMapped.store(true, std::memory_order_release);
    }
    else if(wasMapped) {
        if(framesWrittenThisCycle > 0 && m_outputFormat.sampleRate() > 0) {
            const double scale     = std::clamp(m_playbackDelayToTrackScale.load(std::memory_order_acquire), 0.05, 8.0);
            const double advanceMs = (static_cast<double>(framesWrittenThisCycle) * 1000.0
                                      / static_cast<double>(m_outputFormat.sampleRate()))
                                   * scale;

            const uint64_t current = m_position.load(std::memory_order_acquire);
            const uint64_t deltaMs = static_cast<uint64_t>(std::llround(std::max(0.0, advanceMs)));
            const uint64_t next    = deltaMs > (std::numeric_limits<uint64_t>::max() - current)
                                       ? std::numeric_limits<uint64_t>::max()
                                       : (current + deltaMs);
            m_position.store(next, std::memory_order_release);
        }
        m_positionIsMapped.store(true, std::memory_order_release);
    }
    else if(!wasMapped) {
        if(auto stream = m_mixer.primaryStream()) {
            m_position.store(stream->positionMs(), std::memory_order_release);
        }
        m_positionIsMapped.store(false, std::memory_order_release);
    }

    const bool mappedNow = m_positionIsMapped.load(std::memory_order_acquire);
    const auto basis     = mappedNow ? PositionBasis::RenderedSource : PositionBasis::DecodeHead;
    updatePlaybackDelay(stateWithWrites(state, framesWrittenThisCycle), basis);
    notifyDataDemand(m_mixer.needsMoreData());
}

void AudioPipeline::setOrphanState(StreamId streamId, uint64_t playbackDelayMs)
{
    if(streamId == InvalidStreamId) {
        clearOrphanState();
        return;
    }

    const StreamId replacedOrphanId = m_orphanTracker.replace(streamId);
    if(replacedOrphanId != InvalidStreamId) {
        m_mixer.removeStream(replacedOrphanId);
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

    const StreamId orphanId      = m_orphanTracker.streamId();
    const auto orphanStream      = m_streamRegistry.getStream(orphanId);
    const auto cleanupEvaluation = m_orphanTracker.evaluate(orphanStream.get());
    if(!cleanupEvaluation.shouldCleanup) {
        return;
    }

    if(cleanupEvaluation.timedOut) {
        qCWarning(PIPELINE) << "Orphan stream drain timed out; forcing cleanup";
    }

    m_mixer.removeStream(orphanId);
    m_streamRegistry.unregisterStream(orphanId);
    clearOrphanState();
}
std::vector<DspNodePtr> AudioPipeline::buildPerTrackNodes() const
{
    auto* registry = m_dspRegistry.load(std::memory_order_acquire);
    if(!registry || m_perTrackChainDefs.empty()) {
        return {};
    }

    std::vector<DspNodePtr> nodes;
    nodes.reserve(m_perTrackChainDefs.size());

    for(const auto& entry : m_perTrackChainDefs) {
        if(auto node = registry->create(entry.id)) {
            if(!entry.settings.isEmpty()) {
                node->loadSettings(entry.settings);
            }
            nodes.push_back(std::move(node));
        }
    }

    return nodes;
}

AudioFormat AudioPipeline::perTrackOutputFormat(const AudioFormat& inputFormat) const
{
    auto* registry = m_dspRegistry.load(std::memory_order_acquire);
    if(!registry || m_perTrackChainDefs.empty() || !inputFormat.isValid()) {
        return inputFormat;
    }

    AudioFormat current{inputFormat};
    for(const auto& entry : m_perTrackChainDefs) {
        if(auto node = registry->create(entry.id)) {
            if(!entry.settings.isEmpty()) {
                node->loadSettings(entry.settings);
            }
            current = node->outputFormat(current);
        }
    }

    return current;
}
} // namespace Fooyin
