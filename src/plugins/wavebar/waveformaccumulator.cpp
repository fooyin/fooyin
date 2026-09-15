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
 */

#include "waveformaccumulator.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace Fooyin::WaveBar {
WaveformAccumulator::WaveformAccumulator(WaveformData<float>* data, uint64_t totalFrames, int targetSampleCount)
    : m_data{data}
    , m_totalFrames{totalFrames}
    , m_processedFrames{0}
    , m_framesInBin{0}
    , m_targetSampleCount{static_cast<int>(
          std::min(totalFrames, static_cast<uint64_t>(std::clamp(targetSampleCount, 0, MaxTargetSampleCount))))}
    , m_binIndex{0}
    , m_binMax(std::max(0, data->channels), std::numeric_limits<float>::lowest())
    , m_binMin(std::max(0, data->channels), std::numeric_limits<float>::max())
    , m_binSquareSum(std::max(0, data->channels), 0.0)
{
    for(auto& channel : m_data->channelData) {
        channel.max.clear();
        channel.min.clear();
        channel.rms.clear();
        channel.max.reserve(static_cast<size_t>(m_targetSampleCount));
        channel.min.reserve(static_cast<size_t>(m_targetSampleCount));
        channel.rms.reserve(static_cast<size_t>(m_targetSampleCount));
    }
}

bool WaveformAccumulator::isValid() const
{
    return m_totalFrames > 0 && m_targetSampleCount > 0 && m_data->channels > 0
        && std::cmp_equal(m_data->channelData.size(), m_data->channels);
}

bool WaveformAccumulator::complete() const
{
    return isValid() && m_processedFrames >= m_totalFrames;
}

uint64_t WaveformAccumulator::processedFrames() const
{
    return m_processedFrames;
}

int WaveformAccumulator::targetSampleCount() const
{
    return m_targetSampleCount;
}

WaveformAccumulator::ProcessResult WaveformAccumulator::process(const AudioBuffer& buffer, std::stop_token stopToken)
{
    if(!isValid() || !buffer.isValid() || buffer.format().sampleFormat() != SampleFormat::F32
       || buffer.format().channelCount() != m_data->channels) {
        return ProcessResult::Invalid;
    }

    if(complete()) {
        return ProcessResult::Complete;
    }

    const int framesToProcess = static_cast<int>(
        std::min<uint64_t>(static_cast<uint64_t>(buffer.frameCount()), m_totalFrames - m_processedFrames));
    if(framesToProcess <= 0) {
        return ProcessResult::Complete;
    }

    const auto sampleCount = static_cast<size_t>(framesToProcess) * static_cast<size_t>(m_data->channels);
    std::vector<float> samples(sampleCount);
    std::memcpy(samples.data(), buffer.data(), sampleCount * sizeof(float));

    static constexpr int CancellationCheckInterval = 2048;

    for(int frame{0}; frame < framesToProcess; ++frame) {
        if((frame % CancellationCheckInterval) == 0 && stopToken.stop_requested()) {
            return ProcessResult::Cancelled;
        }

        const size_t frameOffset = static_cast<size_t>(frame) * static_cast<size_t>(m_data->channels);
        for(int channel{0}; channel < m_data->channels; ++channel) {
            const float sample = samples[frameOffset + static_cast<size_t>(channel)];
            const auto index   = static_cast<size_t>(channel);
            m_binMax[index]    = std::max(m_binMax[index], sample);
            m_binMin[index]    = std::min(m_binMin[index], sample);
            m_binSquareSum[index] += static_cast<double>(sample) * static_cast<double>(sample);
        }

        ++m_processedFrames;
        ++m_framesInBin;

        if(m_processedFrames >= binEndFrame() && !finishBin()) {
            return ProcessResult::Invalid;
        }
    }

    return complete() ? ProcessResult::Complete : ProcessResult::Accepted;
}

bool WaveformAccumulator::finish()
{
    return m_framesInBin == 0 || finishBin();
}

uint64_t WaveformAccumulator::binEndFrame() const
{
    if(m_targetSampleCount <= 0 || m_binIndex >= m_targetSampleCount) {
        return m_totalFrames;
    }

    const auto binNumber          = static_cast<uint64_t>(m_binIndex) + 1;
    const auto bins               = static_cast<uint64_t>(m_targetSampleCount);
    const uint64_t framesPerBin   = m_totalFrames / bins;
    const uint64_t remainder      = m_totalFrames % bins;
    const uint64_t remainderFrame = ((binNumber * remainder) + bins - 1) / bins;
    return (binNumber * framesPerBin) + remainderFrame;
}

bool WaveformAccumulator::finishBin()
{
    if(m_framesInBin == 0) {
        return true;
    }
    if(m_binIndex >= m_targetSampleCount || m_data->sampleCount() >= m_targetSampleCount) {
        return false;
    }

    const double normalise = 1.0 / static_cast<double>(m_framesInBin);
    for(int channel{0}; channel < m_data->channels; ++channel) {
        const auto index = static_cast<size_t>(channel);
        auto& output     = m_data->channelData[index];
        output.max.emplace_back(m_binMax[index]);
        output.min.emplace_back(m_binMin[index]);
        output.rms.emplace_back(static_cast<float>(std::sqrt(m_binSquareSum[index] * normalise)));

        m_binMax[index]       = std::numeric_limits<float>::lowest();
        m_binMin[index]       = std::numeric_limits<float>::max();
        m_binSquareSum[index] = 0.0;
    }

    m_framesInBin = 0;
    ++m_binIndex;
    return true;
}
} // namespace Fooyin::WaveBar
