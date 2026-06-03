/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include "settings/wavebarsettings.h"
#include "waveformdata.h"

#include <utils/worker.h>

namespace Fooyin::WaveBar {
class WaveformRescaler : public Worker
{
    Q_OBJECT

public:
    explicit WaveformRescaler(QObject* parent = nullptr);

Q_SIGNALS:
    void waveformRescaled(const Fooyin::WaveBar::WaveformData<float>& data);

public Q_SLOTS:
    void rescale();
    void rescale(int width);
    void rescale(const Fooyin::WaveBar::WaveformData<float>& data, int width);

    void changeSampleWidth(int width);
    void changeDownmix(DownmixOption option);
    void changeSupersampleFactor(int factor);
    void changePeakDisplayMode(PeakDisplayMode mode);
    void changeNormaliseToPeak(bool normalise);
    void changeDecibelScale(bool decibelScale);

private:
    void normaliseToPeak(WaveformData<float>& data) const;
    void applyDecibelScale(WaveformData<float>& data) const;
    void smoothAverage(WaveformData<float>& data) const;

    WaveformData<float> m_data;
    int m_width;
    int m_sampleWidth;
    int m_supersampleFactor;
    DownmixOption m_downMix;
    PeakDisplayMode m_peakDisplayMode;
    bool m_normaliseToPeak;
    bool m_decibelScale;
};
} // namespace Fooyin::WaveBar
