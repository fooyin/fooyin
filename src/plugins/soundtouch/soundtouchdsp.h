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

#include <SoundTouch.h>
#include <core/engine/dsp/dspnode.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace Fooyin {
class SoundTouchDsp : public DspNode
{
public:
    enum class Parameter : uint8_t
    {
        Tempo = 0,
        Pitch,
        Rate
    };

    explicit SoundTouchDsp(Parameter parameter);
    ~SoundTouchDsp() override;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString id() const override;
    [[nodiscard]] bool supportsLiveSettings() const override;

    [[nodiscard]] AudioFormat outputFormat(const AudioFormat& input) const override;
    void prepare(const AudioFormat& format) override;
    void process(ProcessingBufferList& chunks) override;
    void reset() override;
    void flush(ProcessingBufferList& chunks, FlushMode mode) override;
    [[nodiscard]] int latencyFrames() const override;

    [[nodiscard]] QByteArray saveSettings() const override;
    bool loadSettings(const QByteArray& preset) override;

    [[nodiscard]] Parameter parameter() const;
    [[nodiscard]] double parameterValue() const;
    void setParameterValue(double value);
    [[nodiscard]] double parameterDefaultValue() const;

    void setTempo(double value);
    [[nodiscard]] double tempo() const;

    void setPitchSemitones(double value);
    [[nodiscard]] double pitchSemitones() const;

    void setRate(double value);
    [[nodiscard]] double rate() const;

private:
    void recreateProcessor(const AudioFormat& format);
    void configureProcessor() const;
    void processBuffer(const ProcessingBuffer& buffer, ProcessingBufferList& output);
    int drainProcessor(ProcessingBufferList& output);
    static double clampTempo(double value);
    static double clampPitch(double value);
    static double clampRate(double value);
    [[nodiscard]] static QString idForParameter(Parameter parameter);
    [[nodiscard]] static QString nameForParameter(Parameter parameter);

    AudioFormat m_format;

    Parameter m_parameter;
    std::unique_ptr<soundtouch::SoundTouch> m_processor;

    std::vector<float> m_inputBuffer;
    std::vector<float> m_outputBuffer;

    bool m_hasOutputCursor;
    uint64_t m_outputCursorNs;
    double m_outputCursorFracNs;
    double m_inputSourceFrameNs;

    double m_tempo;
    double m_pitchSemitones;
    double m_rate;
};

class SoundTouchTempoDsp final : public SoundTouchDsp
{
public:
    SoundTouchTempoDsp();
};

class SoundTouchPitchDsp final : public SoundTouchDsp
{
public:
    SoundTouchPitchDsp();
};

class SoundTouchRateDsp final : public SoundTouchDsp
{
public:
    SoundTouchRateDsp();
};
} // namespace Fooyin
