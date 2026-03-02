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

#include "supereqadapter.h"

#include "supereq/paramlist.h"
#include "supereq/supereq.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace {
int clampWindowBits(int bits)
{
    return std::clamp(bits, 8, 18);
}

int windowLengthForBits(int bits)
{
    const int safeBits = clampWindowBits(bits);
    return (1 << (safeBits - 1)) - 1;
}

int windowBitsForSampleRate(int sampleRate, int defaultBits = Fooyin::Equaliser::SuperEq::Processor::DefaultWindowBits,
                            int referenceRate = 96000, int minBits = 8, int maxBits = 16)
{
    if(sampleRate <= 0 || referenceRate <= 0) {
        return std::clamp(defaultBits, minBits, maxBits);
    }

    const int refLen = windowLengthForBits(defaultBits); // samples
    if(refLen <= 0) {
        return std::clamp(defaultBits, minBits, maxBits);
    }

    const auto num        = static_cast<int64_t>(refLen) * static_cast<int64_t>(sampleRate);
    const auto den        = static_cast<int64_t>(referenceRate);
    const auto desiredLen = std::max<int64_t>(1, (num + den / 2) / den);

    const int bits = 1 + std::bit_width(static_cast<uint64_t>(desiredLen));
    return std::clamp(bits, minBits, maxBits);
}

double dbToAmp(const double valueDb)
{
    static constexpr double Ln10 = 2.30258509299404568402;
    return std::exp((Ln10 * valueDb) / 20.0);
}
} // namespace

namespace Fooyin::Equaliser::SuperEq {
class Processor::ChannelProcessor
{
public:
    explicit ChannelProcessor(const int windowBits)
        : m_eq{windowBits}
    { }

    void setTable(const std::array<double, BandCount>& linearBands, const double sampleRate)
    {
        std::array<double, BandCount> bands = linearBands;
        m_eq.equ_makeTable(bands.data(), &m_paramRoot, sampleRate);
    }

    void reset()
    {
        m_eq.equ_clearbuf();
    }

    void writeSamples(double* samples, const int count)
    {
        m_eq.write_samples(samples, count);
    }

    void flush()
    {
        m_eq.write_samples(nullptr, 0);
    }

    const double* getOutput(int& samples)
    {
        return m_eq.get_output(&samples);
    }

    [[nodiscard]] int pendingInputSamples()
    {
        return m_eq.samples_buffered();
    }

private:
    supereq<double> m_eq;
    paramlist m_paramRoot;
};

Processor::Processor()
    : Processor{DefaultWindowBits}
{ }

Processor::Processor(const int windowBits)
    : m_windowBits{clampWindowBits(windowBits)}
    , m_channelCount{0}
    , m_sampleRate{44100.0}
    , m_tableDirty{true}
    , m_preampDb{0.0}
    , m_bandDb{}
{
    m_bandDb.fill(0.0);
}

Processor::~Processor() = default;

void Processor::prepare(const int channelCount, const double sampleRate)
{
    const int safeChannels      = std::max(1, channelCount);
    const double safeRate       = sampleRate > 0.0 ? sampleRate : 44100.0;
    const int adaptedWindowBits = windowBitsForSampleRate(static_cast<int>(std::lround(safeRate)), DefaultWindowBits);

    m_channelCount = safeChannels;
    m_sampleRate   = safeRate;
    m_windowBits   = adaptedWindowBits;

    m_channels.clear();
    m_channels.reserve(static_cast<size_t>(safeChannels));
    for(int i{0}; i < safeChannels; ++i) {
        m_channels.push_back(std::make_unique<ChannelProcessor>(m_windowBits));
    }

    m_outputPointers.resize(static_cast<size_t>(safeChannels), nullptr);
    m_outputSamples.resize(static_cast<size_t>(safeChannels), 0);

    m_tableDirty = true;
}

void Processor::setGainDb(const double preampDb, const std::array<double, BandCount>& bandDb)
{
    m_preampDb   = preampDb;
    m_bandDb     = bandDb;
    m_tableDirty = true;
}

void Processor::reset()
{
    for(auto& channel : m_channels) {
        if(channel) {
            (*channel).reset();
        }
    }
}

void Processor::rebuildTablesIfNeeded()
{
    if(!m_tableDirty) {
        return;
    }

    std::array<double, BandCount> linearBands{};
    for(size_t i{0}; i < linearBands.size(); ++i) {
        linearBands[i] = dbToAmp(m_preampDb + m_bandDb[i]);
    }

    for(auto& channel : m_channels) {
        if(channel) {
            channel->setTable(linearBands, m_sampleRate);
        }
    }

    m_tableDirty = false;
}

int Processor::processInterleaved(std::span<const double> inputInterleaved, std::vector<double>& outputInterleaved)
{
    outputInterleaved.clear();

    if(m_channelCount <= 0 || m_channels.empty() || inputInterleaved.empty()) {
        return 0;
    }

    const auto channels = static_cast<size_t>(m_channelCount);
    const size_t frames = inputInterleaved.size() / channels;
    if(frames == 0) {
        return 0;
    }

    rebuildTablesIfNeeded();

    m_channelInputScratch.resize(frames * channels);

    for(size_t frame{0}; frame < frames; ++frame) {
        const size_t srcBase = frame * channels;

        for(size_t channel{0}; channel < channels; ++channel) {
            m_channelInputScratch[(channel * frames) + frame] = inputInterleaved[srcBase + channel];
        }
    }

    int outputFrames = std::numeric_limits<int>::max();

    for(size_t channel{0}; channel < channels; ++channel) {
        auto& processor = m_channels[channel];
        processor->writeSamples(&m_channelInputScratch[channel * frames], static_cast<int>(frames));

        int sampleCount{0};
        m_outputPointers[channel] = processor->getOutput(sampleCount);
        m_outputSamples[channel]  = sampleCount;

        outputFrames = std::min(outputFrames, sampleCount);
    }

    if(outputFrames <= 0) {
        return 0;
    }

    outputInterleaved.resize(static_cast<size_t>(outputFrames) * channels);
    for(int frame{0}; frame < outputFrames; ++frame) {
        const size_t dstBase = static_cast<size_t>(frame) * channels;

        for(size_t channel{0}; channel < channels; ++channel) {
            outputInterleaved[dstBase + channel] = m_outputPointers[channel][frame];
        }
    }

    return outputFrames;
}

int Processor::flushInterleaved(std::vector<double>& outputInterleaved)
{
    outputInterleaved.clear();

    if(m_channelCount <= 0 || m_channels.empty()) {
        return 0;
    }

    rebuildTablesIfNeeded();

    const auto channels = static_cast<size_t>(m_channelCount);
    int outputFrames    = std::numeric_limits<int>::max();

    for(size_t channel{0}; channel < channels; ++channel) {
        auto& processor = m_channels[channel];
        processor->flush();

        int sampleCount{0};
        m_outputPointers[channel] = processor->getOutput(sampleCount);
        m_outputSamples[channel]  = sampleCount;

        outputFrames = std::min(outputFrames, sampleCount);
    }

    if(outputFrames <= 0) {
        return 0;
    }

    outputInterleaved.resize(static_cast<size_t>(outputFrames) * channels);

    for(int frame{0}; frame < outputFrames; ++frame) {
        const size_t dstBase = static_cast<size_t>(frame) * channels;

        for(size_t channel{0}; channel < channels; ++channel) {
            outputInterleaved[dstBase + channel] = m_outputPointers[channel][frame];
        }
    }

    return outputFrames;
}

int Processor::pendingInputFrames() const
{
    if(m_channels.empty()) {
        return 0;
    }

    int pendingSamples{0};

    for(const auto& channel : m_channels) {
        if(channel) {
            pendingSamples = std::max(pendingSamples, channel->pendingInputSamples());
        }
    }

    return std::max(0, pendingSamples);
}

int Processor::channelCount() const
{
    return m_channelCount;
}

int Processor::windowLength() const
{
    return windowLengthForBits(m_windowBits);
}
} // namespace Fooyin::Equaliser::SuperEq
