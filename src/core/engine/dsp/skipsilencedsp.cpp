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

#include "skipsilencedsp.h"

#include "skipsilencesettings.h"

#include <utils/timeconstants.h>

#include <QIODevice>

#include <algorithm>
#include <cmath>
#include <limits>

#include <span>

constexpr auto BlockFrames  = 256;
constexpr auto HysteresisDb = 6;

namespace {
double dbToLin(int db)
{
    return std::pow(10.0, static_cast<double>(db) / 20.0);
}

uint64_t startTimeForFrameNs(const Fooyin::AudioFormat& fmt, uint64_t bufferStartTimeNs, int frameOffset)
{
    if(!fmt.isValid() || fmt.sampleRate() <= 0 || frameOffset <= 0) {
        return bufferStartTimeNs;
    }

    return bufferStartTimeNs
         + (static_cast<uint64_t>(frameOffset) * Fooyin::Time::NsPerSecond / static_cast<uint64_t>(fmt.sampleRate()));
}

double blockPeakAbs(const double* interleaved, int frames, int channels) noexcept
{
    double peak       = 0.0;
    const int samples = frames * channels;
    for(int i = 0; i < samples; ++i) {
        peak = std::max(peak, std::abs(interleaved[i]));
    }
    return peak;
}

uint64_t sourceFrameDurationNsFor(const Fooyin::ProcessingBuffer& buffer, const Fooyin::AudioFormat& format)
{
    if(buffer.sourceFrameDurationNs() > 0) {
        return buffer.sourceFrameDurationNs();
    }

    if(!format.isValid() || format.sampleRate() <= 0) {
        return 0;
    }

    const double frameNs
        = static_cast<double>(Fooyin::Time::NsPerSecond) / static_cast<double>(std::max(1, format.sampleRate()));
    return static_cast<uint64_t>(std::max<int64_t>(1, std::llround(frameNs)));
}
} // namespace

namespace Fooyin {
SkipSilenceDsp::SkipSilenceDsp()
    : m_minSilenceDurationMs{SkipSilenceSettings::DefaultMinSilenceDurationMs}
    , m_thresholdDb{SkipSilenceSettings::DefaultThresholdDb}
    , m_keepInitialPeriod{SkipSilenceSettings::DefaultKeepInitialPeriod}
    , m_includeMiddleSilence{SkipSilenceSettings::DefaultIncludeMiddleSilence}
    , m_haveSeenNonSilence{false}
{ }

QString SkipSilenceDsp::name() const
{
    return QStringLiteral("Skip Silence");
}

QString SkipSilenceDsp::id() const
{
    return QStringLiteral("fooyin.dsp.skipsilence");
}

void SkipSilenceDsp::prepare(const AudioFormat& format)
{
    if(format != m_format) {
        reset();
    }
    m_format = format;
}

void SkipSilenceDsp::process(ProcessingBufferList& chunks)
{
    const size_t count = chunks.count();
    if(count == 0) {
        return;
    }

    ProcessingBufferList output;
    for(size_t i = 0; i < count; ++i) {
        const auto* buffer = chunks.item(i);
        if(buffer && buffer->isValid()) {
            processBuffer(*buffer, output);
        }
    }

    chunks.clear();
    for(size_t i{0}; i < output.count(); ++i) {
        const auto* b = output.item(i);
        if(b && b->isValid()) {
            chunks.addChunk(*b);
        }
    }
}

void SkipSilenceDsp::reset()
{
    m_pendingSilence.reset();
    m_haveSeenNonSilence = false;
}

void SkipSilenceDsp::flush(ProcessingBufferList& chunks, FlushMode mode)
{
    if(mode == FlushMode::EndOfTrack) {
        if(m_pendingSilence.active && m_format.isValid()) {
            const int minMs     = m_minSilenceDurationMs.load(std::memory_order_relaxed);
            const int minFrames = std::max(0, m_format.framesForDuration(static_cast<uint64_t>(minMs)));
            finalisePendingSilence(chunks, minFrames);
        }
        m_pendingSilence.reset();
        m_haveSeenNonSilence = false;
        return;
    }

    if(mode == FlushMode::Flush) {
        m_pendingSilence.reset();
        m_haveSeenNonSilence = false;
    }
}

int SkipSilenceDsp::latencyFrames() const
{
    if(!m_pendingSilence.active || !m_pendingSilence.buffer.isValid()) {
        return 0;
    }

    return static_cast<int>(std::clamp<int64_t>(m_pendingSilence.buffer.frameCount(), 0,
                                                static_cast<int64_t>(std::numeric_limits<int>::max())));
}

void SkipSilenceDsp::processBuffer(const ProcessingBuffer& buffer, ProcessingBufferList& output)
{
    const auto format = buffer.format();
    if(!format.isValid()) {
        return;
    }

    const int frames   = buffer.frameCount();
    const int channels = format.channelCount();
    if(frames <= 0 || channels <= 0) {
        return;
    }

    const auto dataSpan          = buffer.constData();
    const size_t expectedSamples = static_cast<size_t>(frames) * static_cast<size_t>(channels);
    if(dataSpan.size() < expectedSamples) {
        return;
    }

    const uint64_t sourceFrameDurationNs = sourceFrameDurationNsFor(buffer, format);

    const int minMs        = m_minSilenceDurationMs.load(std::memory_order_relaxed);
    const int minFrames    = std::max(0, format.framesForDuration(static_cast<uint64_t>(minMs)));
    const int thresholdDb  = m_thresholdDb.load(std::memory_order_relaxed);
    const bool keepInitial = m_keepInitialPeriod.load(std::memory_order_relaxed);
    const bool includeMid  = m_includeMiddleSilence.load(std::memory_order_relaxed);

    const int enterDb
        = std::clamp(thresholdDb, SkipSilenceSettings::ThresholdMinDb, SkipSilenceSettings::ThresholdMaxDb);
    const int exitDb      = std::clamp(thresholdDb + HysteresisDb, SkipSilenceSettings::ThresholdMinDb,
                                       SkipSilenceSettings::ThresholdMaxDb);
    const double enterThr = dbToLin(enterDb);
    const double exitThr  = dbToLin(exitDb);

    const double* samples = dataSpan.data();

    auto isSilentBlock = [&](int frameStart, int frameCount, bool currentlySilent, bool droppingSilence) noexcept {
        const double thr  = (currentlySilent && !droppingSilence) ? exitThr : enterThr;
        const double peak = blockPeakAbs(samples + (static_cast<size_t>(frameStart) * static_cast<size_t>(channels)),
                                         frameCount, channels);
        return peak <= thr;
    };

    int frame{0};
    bool currentSilent{false};

    while(frame < frames) {
        const int blockFrames           = std::min(BlockFrames, frames - frame);
        const bool isDroppingSilence    = m_pendingSilence.active && m_pendingSilence.drop;
        const bool silent               = isSilentBlock(frame, blockFrames, currentSilent, isDroppingSilence);
        const uint64_t blockStartTimeNs = startTimeForFrameNs(format, buffer.startTimeNs(), frame);

        if(silent) {
            const bool leading  = !m_haveSeenNonSilence;
            const bool preserve = leading && keepInitial;

            if(preserve) {
                emitSegment(output, format, samples + (static_cast<size_t>(frame) * static_cast<size_t>(channels)),
                            blockFrames, channels, blockStartTimeNs, sourceFrameDurationNs);
                currentSilent = true;
                frame += blockFrames;
                continue;
            }

            if(!m_pendingSilence.active) {
                m_pendingSilence.active      = true;
                m_pendingSilence.leading     = leading;
                m_pendingSilence.preserve    = preserve;
                m_pendingSilence.drop        = false;
                m_pendingSilence.frames      = 0;
                m_pendingSilence.startTimeNs = blockStartTimeNs;
                m_pendingSilence.buffer      = ProcessingBuffer{};
            }

            appendPendingSilence(format, samples + (static_cast<size_t>(frame) * static_cast<size_t>(channels)),
                                 blockFrames, channels, blockStartTimeNs, sourceFrameDurationNs);

            const bool policyDropsThisSilence
                = (m_pendingSilence.leading && !keepInitial) || (!m_pendingSilence.leading && !includeMid);

            if(!m_pendingSilence.preserve && !m_pendingSilence.drop && minFrames > 0
               && m_pendingSilence.frames >= minFrames && policyDropsThisSilence) {
                m_pendingSilence.drop = true;
                m_pendingSilence.buffer.reset();
            }
        }
        else {
            if(m_pendingSilence.active) {
                finalisePendingSilence(output, minFrames);
            }

            emitSegment(output, format, samples + (static_cast<size_t>(frame) * static_cast<size_t>(channels)),
                        blockFrames, channels, blockStartTimeNs, sourceFrameDurationNs);
            m_haveSeenNonSilence = true;
        }

        currentSilent = silent;
        frame += blockFrames;
    }
}

void SkipSilenceDsp::appendPendingSilence(const AudioFormat& format, const double* data, int frames, int channels,
                                          uint64_t startTimeNs, uint64_t sourceFrameDurationNs)
{
    if(!m_pendingSilence.active || frames <= 0) {
        return;
    }

    if(!m_pendingSilence.drop) {
        if(!m_pendingSilence.buffer.isValid()) {
            m_pendingSilence.buffer = ProcessingBuffer{format, startTimeNs};
            m_pendingSilence.buffer.setSourceFrameDurationNs(sourceFrameDurationNs);
        }

        const size_t sampleCount = static_cast<size_t>(frames) * static_cast<size_t>(channels);
        m_pendingSilence.buffer.append(std::span<const double>{data, sampleCount});
    }

    m_pendingSilence.frames += frames;
}

void SkipSilenceDsp::finalisePendingSilence(ProcessingBufferList& output, int minFrames)
{
    if(!m_pendingSilence.active) {
        return;
    }

    const bool includeMid  = m_includeMiddleSilence.load(std::memory_order_relaxed);
    const bool keepInitial = m_keepInitialPeriod.load(std::memory_order_relaxed);

    const bool longSilence = (minFrames <= 0) || (m_pendingSilence.frames >= minFrames);

    bool shouldOutput{false};
    if(m_pendingSilence.preserve) {
        shouldOutput = true;
    }
    else if(m_pendingSilence.leading) {
        shouldOutput = keepInitial || !longSilence;
    }
    else {
        shouldOutput = includeMid || !longSilence;
    }

    if(shouldOutput && m_pendingSilence.buffer.isValid() && !m_pendingSilence.drop) {
        output.addChunk(m_pendingSilence.buffer);
    }

    m_pendingSilence.reset();
}

void SkipSilenceDsp::emitSegment(ProcessingBufferList& output, const AudioFormat& format, const double* data,
                                 int frames, int channels, uint64_t startTimeNs, uint64_t sourceFrameDurationNs)
{
    if(frames <= 0) {
        return;
    }

    const size_t samples = static_cast<size_t>(frames) * static_cast<size_t>(channels);

    // Fast path
    if(auto* chunk = output.addItem(format, startTimeNs, samples)) {
        auto span = chunk->data();
        if(span.size() >= samples) {
            std::copy_n(data, samples, span.data());
            chunk->setSourceFrameDurationNs(sourceFrameDurationNs);
            return;
        }
    }

    // Fallback path
    ProcessingBuffer tmp{format, startTimeNs};
    tmp.setSourceFrameDurationNs(sourceFrameDurationNs);
    tmp.append(std::span<const double>{data, samples});
    output.addChunk(tmp);
}

int SkipSilenceDsp::minSilenceDurationMs() const
{
    return m_minSilenceDurationMs.load(std::memory_order_relaxed);
}

void SkipSilenceDsp::setMinSilenceDurationMs(int durationMs)
{
    const int clamped
        = std::clamp(durationMs, SkipSilenceSettings::MinSilenceMinMs, SkipSilenceSettings::MinSilenceMaxMs);
    m_minSilenceDurationMs.store(clamped, std::memory_order_relaxed);
}

int SkipSilenceDsp::thresholdDb() const
{
    return m_thresholdDb.load(std::memory_order_relaxed);
}

void SkipSilenceDsp::setThresholdDb(int thresholdDb)
{
    const int clamped
        = std::clamp(thresholdDb, SkipSilenceSettings::ThresholdMinDb, SkipSilenceSettings::ThresholdMaxDb);
    m_thresholdDb.store(clamped, std::memory_order_relaxed);
}

bool SkipSilenceDsp::keepInitialPeriod() const
{
    return m_keepInitialPeriod.load(std::memory_order_relaxed);
}

void SkipSilenceDsp::setKeepInitialPeriod(bool keep)
{
    m_keepInitialPeriod.store(keep, std::memory_order_relaxed);
}

bool SkipSilenceDsp::includeMiddleSilence() const
{
    return m_includeMiddleSilence.load(std::memory_order_relaxed);
}

void SkipSilenceDsp::setIncludeMiddleSilence(bool include)
{
    m_includeMiddleSilence.store(include, std::memory_order_relaxed);
}

QByteArray SkipSilenceDsp::saveSettings() const
{
    QByteArray data;
    QDataStream stream{&data, QIODevice::WriteOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    stream << static_cast<quint32>(SkipSilenceSettings::SerializationVersion);
    stream << static_cast<quint32>(minSilenceDurationMs());
    stream << static_cast<quint32>(thresholdDb());
    stream << keepInitialPeriod();
    stream << includeMiddleSilence();

    return data;
}

bool SkipSilenceDsp::loadSettings(const QByteArray& preset)
{
    if(preset.isEmpty()) {
        return false;
    }

    QDataStream stream{preset};
    stream.setVersion(QDataStream::Qt_6_0);

    quint32 version{0};
    qint32 minSilence{0};
    qint32 threshold{0};
    bool keepInitial{false};
    bool includeMiddle{false};

    stream >> version;

    if(stream.status() != QDataStream::Ok || version != SkipSilenceSettings::SerializationVersion) {
        return false;
    }

    stream >> minSilence >> threshold >> keepInitial >> includeMiddle;

    if(stream.status() != QDataStream::Ok) {
        return false;
    }

    setMinSilenceDurationMs(minSilence);
    setThresholdDb(threshold);
    setKeepInitialPeriod(keepInitial);
    setIncludeMiddleSilence(includeMiddle);

    return true;
}
} // namespace Fooyin
