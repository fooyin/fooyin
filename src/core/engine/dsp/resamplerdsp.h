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

#include <core/engine/dsp/dspnode.h>
#include <core/engine/dsp/processingbuffer.h>
#include <core/engine/dsp/resamplersettings.h>

#include <cstdint>
#include <set>

struct SwrContext;

namespace Fooyin {
class FYCORE_EXPORT ResamplerDsp : public DspNode
{
public:
    ResamplerDsp();
    ~ResamplerDsp() override;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString id() const override;

    [[nodiscard]] AudioFormat outputFormat(const AudioFormat& input) const override;
    void prepare(const AudioFormat& format) override;
    void process(ProcessingBufferList& chunks) override;
    void reset() override;
    void flush(ProcessingBufferList& chunks, FlushMode mode) override;
    [[nodiscard]] int latencyFrames() const override;

    [[nodiscard]] QByteArray saveSettings() const override;
    bool loadSettings(const QByteArray& preset) override;

    void setTargetSampleRate(int sampleRate);
    [[nodiscard]] int targetSampleRate() const;

    void setUseSoxr(bool useSoxr);
    [[nodiscard]] bool useSoxr() const;

    void setSoxrPrecision(double precision);
    [[nodiscard]] double soxrPrecision() const;

    void setSampleRateFilterMode(ResamplerSettings::SampleRateFilterMode mode);
    [[nodiscard]] ResamplerSettings::SampleRateFilterMode sampleRateFilterMode() const;

    void setFilteredRatesText(const QString& text);
    [[nodiscard]] QString filteredRatesText() const;

private:
    void recreateContext();
    bool createContext();
    void destroyContext();

    void processBuffer(const ProcessingBuffer& buffer, ProcessingBufferList& output);
    [[nodiscard]] bool shouldResampleInputRate(int inputRate) const;
    void advanceOutputCursor(int outFrames);

    SwrContext* m_swr;
    AudioFormat m_inputFormat;
    AudioFormat m_outputFormat;
    bool m_hasOutputCursor;
    uint64_t m_outputCursorNs;
    uint64_t m_outputCursorRemainder;

    int m_targetRate;
    bool m_useSoxr;
    double m_soxrPrecision;
    ResamplerSettings::SampleRateFilterMode m_sampleRateFilterMode;
    std::set<int> m_filteredRates;
    QString m_filteredRatesText;
};
} // namespace Fooyin
