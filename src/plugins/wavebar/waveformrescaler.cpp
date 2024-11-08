/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "waveformrescaler.h"

#include <utils/settings/settingsmanager.h>

namespace {
int buildSample(Fooyin::WaveBar::WaveformSample& sample, Fooyin::WaveBar::WaveformData<float>& data, int channel,
                double start, double end)
{
    int sampleCount{0};

    auto& [inMax, inMin, inRms] = data.channelData.at(channel);

    const int lastIndex = std::floor(end);

    for(int i = std::floor(start); i < std::ceil(end); ++i) {
        for(int index{i}; index < lastIndex; ++index) {
            if(std::cmp_less(index, inMax.size())) {
                const float sampleMax = inMax.at(index);
                const float sampleMin = inMin.at(index);
                const float sampleRms = inRms.at(index);

                sample.max = std::max(sample.max, sampleMax);
                sample.min = std::min(sample.min, sampleMin);
                sample.rms += sampleRms * sampleRms;

                ++sampleCount;
            }
        }
    }

    return sampleCount;
}
} // namespace

namespace Fooyin::WaveBar {
WaveformRescaler::WaveformRescaler(QObject* parent)
    : Worker{parent}
    , m_width{0}
    , m_sampleWidth{1}
    , m_downMix{DownmixOption::Off}
{ }

void WaveformRescaler::rescale()
{
    if(m_width == 0) {
        return;
    }

    setState(Running);

    WaveformData<float> data{m_data};
    data.channelData.clear();

    if(m_downMix == DownmixOption::Stereo && data.channels > 2) {
        data.channels = 2;
    }
    else if(m_downMix == DownmixOption::Mono) {
        data.channels = 1;
    }

    data.channelData.resize(data.channels);

    const double sampleSize
        = static_cast<double>(m_data.complete ? m_data.sampleCount() : m_data.samplesPerChannel) * m_sampleWidth;
    const auto samplesPerPixel = sampleSize / m_width;

    for(int ch{0}; ch < data.channels; ++ch) {
        auto& [outMax, outMin, outRms] = data.channelData.at(ch);

        double start{0.0};

        for(int x{0}; x < m_width; ++x) {
            if(!mayRun()) {
                return;
            }

            const double end = std::max(1.0, (x + 1) * samplesPerPixel);

            int sampleCount{0};
            WaveformSample sample;

            if(m_downMix == DownmixOption::Mono || (m_downMix == DownmixOption::Stereo && m_data.channels > 2)) {
                for(int mixCh{0}; mixCh < m_data.channels; ++mixCh) {
                    sampleCount += buildSample(sample, m_data, mixCh, start, end);
                }
            }
            else {
                sampleCount += buildSample(sample, m_data, ch, start, end);
            }

            if(sampleCount > 0) {
                sample.rms /= static_cast<float>(sampleCount);
                sample.rms = std::sqrt(sample.rms);

                outMax.emplace_back(sample.max);
                outMin.emplace_back(sample.min);
                outRms.emplace_back(sample.rms);
            }

            start = end;
        }
    }

    setState(Idle);
    emit waveformRescaled(data);
}

void WaveformRescaler::rescale(int width)
{
    m_width = width;

    if(m_data.empty()) {
        return;
    }

    rescale();
}

void WaveformRescaler::rescale(const WaveformData<float>& data, int width)
{
    if(std::exchange(m_data, data) != data) {
        rescale(width);
    }
}

void WaveformRescaler::changeSampleWidth(int width)
{
    if(std::exchange(m_sampleWidth, width) != width) {
        rescale(m_width);
    }
}

void WaveformRescaler::changeDownmix(DownmixOption option)
{
    if(std::exchange(m_downMix, option) != option) {
        rescale(m_width);
    }
}
} // namespace Fooyin::WaveBar
