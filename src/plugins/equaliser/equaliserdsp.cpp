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

#include "equaliserdsp.h"

#include <utils/timeconstants.h>

#include <QIODevice>

#include <algorithm>
#include <cmath>

constexpr quint32 PresetVersion = 1;
constexpr auto GainMinDb        = -20.0;
constexpr auto GainMaxDb        = 20.0;
constexpr auto NeutralEpsilonDb = 1.0e-9;

namespace {
uint64_t nominalFrameDurationNs(const int sampleRate)
{
    const int safeRate   = std::max(1, sampleRate);
    const double frameNs = static_cast<double>(Fooyin::Time::NsPerSecond) / static_cast<double>(safeRate);
    return static_cast<uint64_t>(std::max<int64_t>(1, std::llround(frameNs)));
}
} // namespace

namespace Fooyin::Equaliser {
EqualiserDsp::EqualiserDsp()
    : m_processor{SuperEq::Processor::DefaultWindowBits}
    , m_settings{defaultSettings()}
    , m_appliedSettings{}
    , m_hasAppliedSettings{false}
    , m_settingsDirty{true}
    , m_hasOutputCursor{false}
    , m_outputCursorNs{0}
    , m_outputCursorRemainderNs{0.0}
    , m_outputSourceFrameDurationNs{0}
{ }

QString EqualiserDsp::name() const
{
    return QStringLiteral("Equaliser");
}

QString EqualiserDsp::id() const
{
    return QStringLiteral("fooyin.dsp.equaliser");
}

bool EqualiserDsp::supportsLiveSettings() const
{
    return true;
}

AudioFormat EqualiserDsp::outputFormat(const AudioFormat& input) const
{
    return input;
}

void EqualiserDsp::prepare(const AudioFormat& format)
{
    m_format = format;

    if(!m_format.isValid()) {
        reset();
        return;
    }

    const int channels   = std::max(1, m_format.channelCount());
    const int sampleRate = std::max(1, m_format.sampleRate());

    m_processor.prepare(channels, static_cast<double>(sampleRate));
    m_outputScratch.clear();

    m_hasOutputCursor             = false;
    m_outputCursorNs              = 0;
    m_outputCursorRemainderNs     = 0.0;
    m_outputSourceFrameDurationNs = 0;

    m_hasAppliedSettings = false;
    m_settingsDirty      = true;

    applySettingsIfNeeded();
}

void EqualiserDsp::process(ProcessingBufferList& chunks)
{
    if(applySettingsIfNeeded()) {
        return;
    }

    ProcessingBufferList output;
    output.clear();

    const size_t count = chunks.count();
    for(size_t i{0}; i < count; ++i) {
        const auto* buffer = chunks.item(i);
        if(!buffer || !buffer->isValid() || buffer->frameCount() <= 0) {
            continue;
        }

        ensurePreparedFor(buffer->format());
        if(!m_format.isValid()) {
            continue;
        }

        if(!m_hasOutputCursor) {
            m_outputCursorNs          = buffer->startTimeNs();
            m_outputCursorRemainderNs = 0.0;
            m_hasOutputCursor         = true;
        }

        const uint64_t sourceFrameDurationNs = buffer->sourceFrameDurationNs() > 0
                                                 ? buffer->sourceFrameDurationNs()
                                                 : nominalFrameDurationNs(m_format.sampleRate());
        m_outputSourceFrameDurationNs        = sourceFrameDurationNs;

        const int producedFrames = m_processor.processInterleaved(buffer->constData(), m_outputScratch);
        if(producedFrames <= 0) {
            continue;
        }

        emitOutputChunk(output, producedFrames, sourceFrameDurationNs);
    }

    chunks.clear();

    const size_t outCount = output.count();
    for(size_t i{0}; i < outCount; ++i) {
        const auto* buffer = output.item(i);
        if(buffer && buffer->isValid()) {
            chunks.addChunk(*buffer);
        }
    }
}

void EqualiserDsp::reset()
{
    m_processor.reset();
    m_hasOutputCursor             = false;
    m_outputCursorNs              = 0;
    m_outputCursorRemainderNs     = 0.0;
    m_outputSourceFrameDurationNs = 0;
}

void EqualiserDsp::flush(ProcessingBufferList& chunks, const FlushMode mode)
{
    if(mode == FlushMode::Flush) {
        reset();
        return;
    }

    if(!m_format.isValid()) {
        return;
    }

    if(applySettingsIfNeeded()) {
        reset();
        return;
    }

    if(!m_hasOutputCursor) {
        m_hasOutputCursor         = true;
        m_outputCursorNs          = 0;
        m_outputCursorRemainderNs = 0.0;
    }

    const uint64_t sourceFrameDurationNs = m_outputSourceFrameDurationNs > 0
                                             ? m_outputSourceFrameDurationNs
                                             : nominalFrameDurationNs(m_format.sampleRate());

    for(;;) {
        const int producedFrames = m_processor.flushInterleaved(m_outputScratch);
        if(producedFrames <= 0) {
            break;
        }

        emitOutputChunk(chunks, producedFrames, sourceFrameDurationNs);
    }

    reset();
}

int EqualiserDsp::latencyFrames() const
{
    return isNeutralSettings(m_settings) ? 0 : m_processor.windowLength();
}

QByteArray EqualiserDsp::saveSettings() const
{
    QByteArray data;
    QDataStream stream{&data, QIODevice::WriteOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    stream << quint32(PresetVersion);
    stream << m_settings.preampDb;
    for(const double band : m_settings.bandDb) {
        stream << band;
    }

    return data;
}

bool EqualiserDsp::loadSettings(const QByteArray& preset)
{
    if(preset.isEmpty()) {
        return false;
    }

    QDataStream stream{preset};
    stream.setVersion(QDataStream::Qt_6_0);

    quint32 version{0};
    Settings s = defaultSettings();

    stream >> version;
    if(stream.status() != QDataStream::Ok || version != PresetVersion) {
        return false;
    }

    stream >> s.preampDb;
    s.preampDb = clampGainDb(s.preampDb);

    for(double& band : s.bandDb) {
        stream >> band;
        band = clampGainDb(band);
    }

    if(stream.status() != QDataStream::Ok) {
        return false;
    }

    publishSettings(s);
    return true;
}

void EqualiserDsp::setPreampDb(const double value)
{
    Settings next = m_settings;
    next.preampDb = clampGainDb(value);
    publishSettings(next);
}

double EqualiserDsp::preampDb() const
{
    return m_settings.preampDb;
}

void EqualiserDsp::setBandDb(const int band, const double value)
{
    if(band < 0 || band >= BandCount) {
        return;
    }

    Settings next                          = m_settings;
    next.bandDb[static_cast<size_t>(band)] = clampGainDb(value);
    publishSettings(next);
}

double EqualiserDsp::bandDb(const int band) const
{
    if(band < 0 || band >= BandCount) {
        return 0.0;
    }

    return m_settings.bandDb[static_cast<size_t>(band)];
}

EqualiserDsp::Settings EqualiserDsp::defaultSettings()
{
    Settings s;
    s.preampDb = 0.0;
    s.bandDb.fill(0.0);
    return s;
}

double EqualiserDsp::clampGainDb(const double value)
{
    return std::clamp(value, GainMinDb, GainMaxDb);
}

bool EqualiserDsp::isNeutralSettings(const Settings& settings)
{
    if(std::abs(settings.preampDb) > NeutralEpsilonDb) {
        return false;
    }

    return std::ranges::all_of(settings.bandDb,
                               [](const double bandDb) { return std::abs(bandDb) <= NeutralEpsilonDb; });
}

bool EqualiserDsp::settingsEqual(const Settings& lhs, const Settings& rhs)
{
    if(std::abs(lhs.preampDb - rhs.preampDb) > NeutralEpsilonDb) {
        return false;
    }

    for(size_t i{0}; i < lhs.bandDb.size(); ++i) {
        if(std::abs(lhs.bandDb[i] - rhs.bandDb[i]) > NeutralEpsilonDb) {
            return false;
        }
    }

    return true;
}

void EqualiserDsp::publishSettings(const Settings& settings)
{
    if(settingsEqual(settings, m_settings)) {
        return;
    }

    m_settings      = settings;
    m_settingsDirty = true;
}

bool EqualiserDsp::applySettingsIfNeeded()
{
    const Settings& current = m_settings;
    const bool neutral      = isNeutralSettings(current);

    if(!m_settingsDirty && m_hasAppliedSettings) {
        return neutral;
    }

    const bool wasNeutral = m_hasAppliedSettings && isNeutralSettings(m_appliedSettings);

    if(neutral) {
        if(!wasNeutral) {
            reset();
        }
    }
    else {
        m_processor.setGainDb(current.preampDb, current.bandDb);
    }

    m_appliedSettings    = current;
    m_hasAppliedSettings = true;
    m_settingsDirty      = false;
    return neutral;
}

void EqualiserDsp::ensurePreparedFor(const AudioFormat& format)
{
    const AudioFormat expected = outputFormat(format);
    if(!m_format.isValid() || m_format.sampleRate() != expected.sampleRate()
       || m_format.channelCount() != expected.channelCount()) {
        prepare(expected);
    }
}

void EqualiserDsp::emitOutputChunk(ProcessingBufferList& output, const int frames, const uint64_t sourceFrameDurationNs)
{
    if(frames <= 0 || !m_format.isValid()) {
        return;
    }

    const int channels       = std::max(1, m_format.channelCount());
    const size_t sampleCount = static_cast<size_t>(frames) * static_cast<size_t>(channels);

    ProcessingBuffer out{m_format, m_outputCursorNs};
    out.resizeSamples(sampleCount);

    auto outData           = out.data();
    const size_t copyCount = std::min(outData.size(), m_outputScratch.size());
    for(size_t i{0}; i < copyCount; ++i) {
        outData[i] = m_outputScratch[i];
    }

    out.setSourceFrameDurationNs(sourceFrameDurationNs);
    output.addChunk(out);

    advanceOutputCursor(frames, sourceFrameDurationNs);
}

void EqualiserDsp::advanceOutputCursor(const int frames, const uint64_t sourceFrameDurationNs)
{
    if(frames <= 0 || sourceFrameDurationNs == 0) {
        return;
    }

    const double advanceNsFull
        = (static_cast<double>(frames) * static_cast<double>(sourceFrameDurationNs)) + m_outputCursorRemainderNs;
    const auto advanceNs = static_cast<uint64_t>(std::max(0.0, std::floor(advanceNsFull)));

    m_outputCursorRemainderNs = advanceNsFull - static_cast<double>(advanceNs);
    m_outputCursorNs += advanceNs;
}
} // namespace Fooyin::Equaliser
