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

#include "soundtouchdsp.h"

#include <SoundTouch.h>
#include <utils/timeconstants.h>

#include <QDataStream>
#include <QIODevice>

#include <algorithm>
#include <cmath>

constexpr auto PresetVersion = 1;
constexpr auto TempoMin      = 0.25;
constexpr auto TempoMax      = 4.0;
constexpr auto PitchMin      = -24.0;
constexpr auto PitchMax      = 24.0;
constexpr auto RateMin       = 0.25;
constexpr auto RateMax       = 4.0;
constexpr auto MinRateScale  = 0.01;

namespace Fooyin {
SoundTouchDsp::SoundTouchDsp(const Parameter parameter)
    : m_parameter{parameter}
    , m_hasOutputCursor{false}
    , m_outputCursorNs{0}
    , m_outputCursorFracNs{0.0}
    , m_inputSourceFrameNs{0.0}
    , m_tempo{1.0}
    , m_pitchSemitones{0.0}
    , m_rate{1.0}
{ }

SoundTouchDsp::~SoundTouchDsp() = default;

QString SoundTouchDsp::name() const
{
    QString baseName        = nameForParameter(m_parameter);
    const double value      = parameterValue();
    const double defaultVal = parameterDefaultValue();

    if(std::abs(value - defaultVal) < 0.0001) {
        return baseName;
    }

    switch(m_parameter) {
        case Parameter::Tempo:
        case Parameter::Rate:
            return QStringLiteral("%1 (%2%)").arg(baseName, QString::number(value * 100.0, 'f', 1));
        case Parameter::Pitch:
            return QStringLiteral("%1 (%2 st)").arg(baseName, QString::number(value, 'f', 2));
    }

    return baseName;
}

QString SoundTouchDsp::id() const
{
    return idForParameter(m_parameter);
}

bool SoundTouchDsp::supportsLiveSettings() const
{
    return true;
}

AudioFormat SoundTouchDsp::outputFormat(const AudioFormat& input) const
{
    return input;
}

void SoundTouchDsp::prepare(const AudioFormat& format)
{
    m_format = format;
    recreateProcessor(m_format);
}

void SoundTouchDsp::process(ProcessingBufferList& chunks)
{
    ProcessingBufferList output;
    output.clear();

    if(!m_processor || !m_format.isValid()) {
        chunks.clear();
        return;
    }

    const size_t count = chunks.count();
    for(size_t i{0}; i < count; ++i) {
        const auto* buffer = chunks.item(i);

        if(!buffer || !buffer->isValid() || buffer->frameCount() <= 0) {
            continue;
        }

        processBuffer(*buffer, output);
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

void SoundTouchDsp::reset()
{
    if(m_processor) {
        m_processor->clear();
    }

    m_hasOutputCursor    = false;
    m_outputCursorNs     = 0;
    m_outputCursorFracNs = 0.0;
    m_inputSourceFrameNs = 0.0;
}

void SoundTouchDsp::flush(ProcessingBufferList& chunks, FlushMode mode)
{
    if(!m_processor) {
        return;
    }

    if(mode == FlushMode::Flush) {
        reset();
        return;
    }

    if(!m_hasOutputCursor) {
        m_outputCursorNs     = 0;
        m_hasOutputCursor    = true;
        m_outputCursorFracNs = 0.0;
    }

    m_processor->flush();
    drainProcessor(chunks);
    m_processor->clear();

    m_hasOutputCursor    = false;
    m_outputCursorNs     = 0;
    m_outputCursorFracNs = 0.0;
    m_inputSourceFrameNs = 0.0;
}

int SoundTouchDsp::latencyFrames() const
{
    if(!m_processor) {
        return 0;
    }

    const int latency = m_processor->getSetting(SETTING_INITIAL_LATENCY);
    return std::max(0, latency);
}

QByteArray SoundTouchDsp::saveSettings() const
{
    QByteArray data;
    QDataStream stream{&data, QIODevice::WriteOnly};
    stream.setVersion(QDataStream::Qt_6_0);

    stream << quint32(PresetVersion);
    stream << parameterValue();

    return data;
}

bool SoundTouchDsp::loadSettings(const QByteArray& preset)
{
    if(preset.isEmpty()) {
        return false;
    }

    QDataStream stream{preset};
    stream.setVersion(QDataStream::Qt_6_0);

    quint32 version{0};
    stream >> version;
    if(stream.status() != QDataStream::Ok) {
        return false;
    }

    if(version == PresetVersion) {
        double value = parameterDefaultValue();
        stream >> value;

        if(stream.status() != QDataStream::Ok) {
            return false;
        }

        setParameterValue(value);
        return true;
    }

    return false;
}

SoundTouchDsp::Parameter SoundTouchDsp::parameter() const
{
    return m_parameter;
}

double SoundTouchDsp::parameterValue() const
{
    switch(m_parameter) {
        case Parameter::Tempo:
            return tempo();
        case Parameter::Pitch:
            return pitchSemitones();
        case Parameter::Rate:
            return rate();
    }

    return parameterDefaultValue();
}

void SoundTouchDsp::setParameterValue(const double value)
{
    switch(m_parameter) {
        case Parameter::Tempo:
            setTempo(value);
            break;
        case Parameter::Pitch:
            setPitchSemitones(value);
            break;
        case Parameter::Rate:
            setRate(value);
            break;
    }
}

double SoundTouchDsp::parameterDefaultValue() const
{
    switch(m_parameter) {
        case Parameter::Tempo:
        case Parameter::Rate:
            return 1.0;
        case Parameter::Pitch:
            return 0.0;
    }

    return 1.0;
}

void SoundTouchDsp::setTempo(double value)
{
    const double clamped = clampTempo(value);
    if(std::abs(clamped - tempo()) < 0.000001) {
        return;
    }

    m_tempo = clamped;
    configureProcessor();
}

double SoundTouchDsp::tempo() const
{
    return m_tempo;
}

void SoundTouchDsp::setPitchSemitones(double value)
{
    const double clamped = clampPitch(value);
    if(std::abs(clamped - pitchSemitones()) < 0.000001) {
        return;
    }

    m_pitchSemitones = clamped;
    configureProcessor();
}

double SoundTouchDsp::pitchSemitones() const
{
    return m_pitchSemitones;
}

void SoundTouchDsp::setRate(double value)
{
    const double clamped = clampRate(value);
    if(std::abs(clamped - rate()) < 0.000001) {
        return;
    }

    m_rate = clamped;
    configureProcessor();
}

double SoundTouchDsp::rate() const
{
    return m_rate;
}

void SoundTouchDsp::recreateProcessor(const AudioFormat& format)
{
    m_processor = std::make_unique<soundtouch::SoundTouch>();
    m_processor->setSampleRate(static_cast<uint>(std::max(1, format.sampleRate())));
    m_processor->setChannels(static_cast<uint>(std::max(1, format.channelCount())));

    configureProcessor();

    m_hasOutputCursor    = false;
    m_outputCursorNs     = 0;
    m_outputCursorFracNs = 0.0;
    m_inputSourceFrameNs = 0.0;
}

void SoundTouchDsp::configureProcessor() const
{
    if(!m_processor) {
        return;
    }

    float tempoValue{1.0F};
    float pitchValue{0.0F};
    float rateValue{1.0F};

    switch(m_parameter) {
        case Parameter::Tempo:
            tempoValue = static_cast<float>(tempo());
            break;
        case Parameter::Pitch:
            pitchValue = static_cast<float>(pitchSemitones());
            break;
        case Parameter::Rate:
            rateValue = static_cast<float>(rate());
            break;
    }

    m_processor->setTempo(tempoValue);
    m_processor->setPitchSemiTones(pitchValue);
    m_processor->setRate(rateValue);
}

void SoundTouchDsp::processBuffer(const ProcessingBuffer& buffer, ProcessingBufferList& output)
{
    const AudioFormat format{buffer.format()};
    if(!format.isValid()) {
        return;
    }

    if(!m_processor || format.sampleRate() != m_format.sampleRate()
       || format.channelCount() != m_format.channelCount()) {
        m_format = format;
        recreateProcessor(m_format);
    }

    const int channels = std::max(1, m_format.channelCount());
    const int frames   = buffer.frameCount();
    if(frames <= 0) {
        return;
    }

    const auto input             = buffer.constData();
    const size_t expectedSamples = static_cast<size_t>(frames) * static_cast<size_t>(channels);
    if(input.size() < expectedSamples) {
        return;
    }

    if(!m_hasOutputCursor) {
        m_outputCursorNs     = buffer.startTimeNs();
        m_hasOutputCursor    = true;
        m_outputCursorFracNs = 0.0;
    }

    const int sampleRate             = std::max(1, m_format.sampleRate());
    const double nominalInputFrameNs = static_cast<double>(Time::NsPerSecond) / static_cast<double>(sampleRate);
    const double inputSourceFrameNs  = buffer.sourceFrameDurationNs() > 0
                                         ? static_cast<double>(buffer.sourceFrameDurationNs())
                                         : nominalInputFrameNs;

    if(m_inputSourceFrameNs <= 0.0) {
        m_inputSourceFrameNs = inputSourceFrameNs;
    }
    else {
        m_inputSourceFrameNs = (m_inputSourceFrameNs * 0.85) + (inputSourceFrameNs * 0.15);
    }

    m_inputBuffer.resize(expectedSamples);
    for(size_t i{0}; i < expectedSamples; ++i) {
        m_inputBuffer[i] = static_cast<float>(input[i]);
    }

    m_processor->putSamples(m_inputBuffer.data(), static_cast<uint>(frames));
    drainProcessor(output);
}

int SoundTouchDsp::drainProcessor(ProcessingBufferList& output)
{
    if(!m_processor || !m_format.isValid()) {
        return 0;
    }

    const int channels = std::max(1, m_format.channelCount());
    int producedFrames = 0;

    while(m_processor->numSamples() > 0) {
        const uint availableFrames = m_processor->numSamples();
        if(availableFrames == 0) {
            break;
        }

        const size_t sampleCount = static_cast<size_t>(availableFrames) * static_cast<size_t>(channels);
        m_outputBuffer.resize(sampleCount);

        const uint receivedFrames = m_processor->receiveSamples(m_outputBuffer.data(), availableFrames);
        if(receivedFrames == 0) {
            break;
        }

        const size_t receivedSamples = static_cast<size_t>(receivedFrames) * static_cast<size_t>(channels);
        auto* outBuffer              = output.addItem(m_format, m_outputCursorNs, receivedSamples);
        if(!outBuffer) {
            break;
        }

        auto outData = outBuffer->data();
        if(outData.size() < receivedSamples) {
            output.removeByIdx(output.count() - 1);
            break;
        }

        for(size_t i{0}; i < receivedSamples; ++i) {
            outData[i] = static_cast<double>(m_outputBuffer[i]);
        }

        producedFrames += static_cast<int>(receivedFrames);

        const double sourceScale         = std::max(MinRateScale, tempo() * rate());
        const int sampleRate             = std::max(1, m_format.sampleRate());
        const double nominalInputFrameNs = static_cast<double>(Time::NsPerSecond) / static_cast<double>(sampleRate);
        const double baseInputFrameNs    = m_inputSourceFrameNs > 0.0 ? m_inputSourceFrameNs : nominalInputFrameNs;
        const double mappedFrameNs       = std::max(1.0, baseInputFrameNs * sourceScale);

        outBuffer->setSourceFrameDurationNs(static_cast<uint64_t>(std::max<int64_t>(1, std::llround(mappedFrameNs))));

        const double advanceNsFull = (static_cast<double>(receivedFrames) * mappedFrameNs) + m_outputCursorFracNs;
        const uint64_t advanceNs   = static_cast<uint64_t>(std::max(0.0, std::floor(advanceNsFull)));

        m_outputCursorFracNs = advanceNsFull - static_cast<double>(advanceNs);
        m_outputCursorNs += advanceNs;
    }

    return producedFrames;
}

double SoundTouchDsp::clampTempo(double value)
{
    return std::clamp(value, TempoMin, TempoMax);
}

double SoundTouchDsp::clampPitch(double value)
{
    return std::clamp(value, PitchMin, PitchMax);
}

double SoundTouchDsp::clampRate(double value)
{
    return std::clamp(value, RateMin, RateMax);
}

QString SoundTouchDsp::idForParameter(const Parameter parameter)
{
    switch(parameter) {
        case Parameter::Tempo:
            return QStringLiteral("fooyin.dsp.soundtouch.tempo");
        case Parameter::Pitch:
            return QStringLiteral("fooyin.dsp.soundtouch.pitch");
        case Parameter::Rate:
            return QStringLiteral("fooyin.dsp.soundtouch.rate");
    }

    return QStringLiteral("fooyin.dsp.soundtouch");
}

QString SoundTouchDsp::nameForParameter(const Parameter parameter)
{
    switch(parameter) {
        case Parameter::Tempo:
            return QStringLiteral("Tempo Shift");
        case Parameter::Pitch:
            return QStringLiteral("Pitch Shift");
        case Parameter::Rate:
            return QStringLiteral("Playback Rate Shift");
    }

    return QStringLiteral("SoundTouch");
}

SoundTouchTempoDsp::SoundTouchTempoDsp()
    : SoundTouchDsp{Parameter::Tempo}
{ }

SoundTouchPitchDsp::SoundTouchPitchDsp()
    : SoundTouchDsp{Parameter::Pitch}
{ }

SoundTouchRateDsp::SoundTouchRateDsp()
    : SoundTouchDsp{Parameter::Rate}
{ }
} // namespace Fooyin
