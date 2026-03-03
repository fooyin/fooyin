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

#include <array>
#include <memory>
#include <span>
#include <vector>

namespace Fooyin::Equaliser::SuperEq {
class Processor
{
public:
    static constexpr auto BandCount         = 18;
    static constexpr auto DefaultWindowBits = 14;

    Processor();
    explicit Processor(int windowBits);
    ~Processor();

    void prepare(int channelCount, double sampleRate);
    void setGainDb(double preampDb, const std::array<double, BandCount>& bandDb);

    void reset();

    int processInterleaved(std::span<const double> inputInterleaved, std::vector<double>& outputInterleaved);
    int flushInterleaved(std::vector<double>& outputInterleaved);

    [[nodiscard]] int pendingInputFrames() const;
    [[nodiscard]] int channelCount() const;
    [[nodiscard]] int windowLength() const;

private:
    class ChannelProcessor;

    void rebuildTablesIfNeeded();

    int m_windowBits;
    int m_channelCount;
    double m_sampleRate;
    bool m_tableDirty;

    double m_preampDb;
    std::array<double, BandCount> m_bandDb;

    std::vector<std::unique_ptr<ChannelProcessor>> m_channels;

    std::vector<double> m_channelInputScratch;
    std::vector<const double*> m_outputPointers;
    std::vector<int> m_outputSamples;
};
} // namespace Fooyin::Equaliser::SuperEq
