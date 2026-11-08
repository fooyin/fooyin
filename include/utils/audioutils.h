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

#include "fyutils_export.h"

#include <cstdint>

namespace Fooyin::Audio {
enum class VolumeCurve : uint8_t
{
    LinearDb = 0,
    LinearAmplitude,
    Quadratic,
    Cubic,
    LogarithmicDb,
};

FYUTILS_EXPORT double dbToVolume(double db);
FYUTILS_EXPORT double volumeToDb(double volume);
// Map between linear amplitude and a normalised control position in [0, 1].
// LinearDb spans -40 dB to 0 dB, with position 0 reserved for mute.
FYUTILS_EXPORT double volumeToPosition(double volume, VolumeCurve curve);
FYUTILS_EXPORT double volumeFromPosition(double position, VolumeCurve curve);
} // namespace Fooyin::Audio
