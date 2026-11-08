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

#include <utils/audioutils.h>

#include <algorithm>
#include <cmath>
#include <limits>

constexpr double MinVolumeDb = -40.0;

namespace Fooyin::Audio {
double dbToVolume(double db)
{
    return std::clamp(std::pow(10.0, db / 20.0), 0.0, 1.0);
}

double volumeToDb(double volume)
{
    if(volume <= 0.0) {
        return -std::numeric_limits<double>::infinity();
    }
    return 20.0 * std::log10(volume);
}

double volumeToPosition(double volume, VolumeCurve curve)
{
    volume = std::clamp(volume, 0.0, 1.0);

    switch(curve) {
        case VolumeCurve::LinearDb:
            return (std::max(volumeToDb(volume), MinVolumeDb) - MinVolumeDb) / -MinVolumeDb;
        case VolumeCurve::LinearAmplitude:
            return volume;
        case VolumeCurve::Quadratic:
            return std::sqrt(volume);
        case VolumeCurve::Cubic:
            return std::cbrt(volume);
        case VolumeCurve::LogarithmicDb:
            return std::exp2(volumeToDb(volume) / 10.0);
    }

    return volume;
}

double volumeFromPosition(double position, VolumeCurve curve)
{
    position = std::clamp(position, 0.0, 1.0);
    if(position == 0.0) {
        return 0.0;
    }

    switch(curve) {
        case VolumeCurve::LinearDb:
            return dbToVolume(MinVolumeDb * (1.0 - position));
        case VolumeCurve::LinearAmplitude:
            return position;
        case VolumeCurve::Quadratic:
            return position * position;
        case VolumeCurve::Cubic:
            return position * position * position;
        case VolumeCurve::LogarithmicDb:
            // Each halving of the position reduces the gain by 10 dB
            return dbToVolume(10.0 * std::log2(position));
    }

    return position;
}
} // namespace Fooyin::Audio
