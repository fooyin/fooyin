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
#include <cstdint>

namespace Fooyin::Gui::FrameRate {
enum class Preset : uint8_t
{
    Fps15  = 15,
    Fps20  = 20,
    Fps24  = 24,
    Fps25  = 25,
    Fps30  = 30,
    Fps40  = 40,
    Fps48  = 48,
    Fps50  = 50,
    Fps60  = 60,
    Fps72  = 72,
    Fps75  = 75,
    Fps90  = 90,
    Fps100 = 100,
    Fps120 = 120,
};

constexpr std::array<Preset, 14> Presets = {
    Preset::Fps15, Preset::Fps20, Preset::Fps24, Preset::Fps25, Preset::Fps30, Preset::Fps40,  Preset::Fps48,
    Preset::Fps50, Preset::Fps60, Preset::Fps72, Preset::Fps75, Preset::Fps90, Preset::Fps100, Preset::Fps120,
};

constexpr int toFps(Preset preset)
{
    return static_cast<int>(preset);
}

constexpr int minFps()
{
    return toFps(Presets.front());
}

constexpr int maxFps()
{
    return toFps(Presets.back());
}

constexpr int clampFps(int fps)
{
    return fps < minFps() ? minFps() : (fps > maxFps() ? maxFps() : fps);
}

constexpr int nearestPresetFps(int fps)
{
    const int value = clampFps(fps);
    int best        = toFps(Presets.front());
    int bestDelta   = value > best ? value - best : best - value;

    for(const auto preset : Presets) {
        const int candidate = toFps(preset);
        const int delta     = value > candidate ? value - candidate : candidate - value;
        if(delta < bestDelta) {
            best      = candidate;
            bestDelta = delta;
        }
    }

    return best;
}

constexpr int intervalMsForFps(int fps)
{
    const int clamped = clampFps(fps);
    return (1000 + (clamped / 2)) / clamped;
}
} // namespace Fooyin::Gui::FrameRate
