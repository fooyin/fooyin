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

#include <cstdint>

namespace Fooyin::Spectrum {
enum class WindowFunction : uint8_t
{
    BlackmanHarris = 0,
    Hann,
    None
};

enum class GradientOrientation : uint8_t
{
    Vertical = 0,
    Horizontal
};

enum class LabelMode : uint8_t
{
    Frequency = 0,
    Notes
};

enum class DrawStyle : uint8_t
{
    Bars = 0,
    Curve
};

constexpr auto MinBandCount     = 1;
constexpr auto MaxBandCount     = 2048;
constexpr auto DefaultBandCount = 80;

constexpr auto MinBarSpacing     = 0;
constexpr auto MaxBarSpacing     = 20;
constexpr auto DefaultBarSpacing = 1;

constexpr auto MinBarSections     = 1;
constexpr auto MaxBarSections     = 200;
constexpr auto DefaultBarSections = 1;

constexpr auto MinSectionSpacing     = 1;
constexpr auto MaxSectionSpacing     = 20;
constexpr auto DefaultSectionSpacing = 1;

constexpr auto MinFrequencyHz        = 20;
constexpr auto MaxFrequencyHz        = 25000;
constexpr auto DefaultMinFrequencyHz = 50;
constexpr auto DefaultMaxFrequencyHz = 25000;
constexpr auto MinPitchHz            = 220;
constexpr auto MaxPitchHz            = 880;
constexpr auto DefaultPitchHz        = 440;
constexpr auto MinTranspose          = -24;
constexpr auto MaxTranspose          = 24;
constexpr auto DefaultTranspose      = 0;
constexpr auto MinNote               = 0;
constexpr auto MaxNote               = 125;
constexpr auto DefaultMinNote        = 0;
constexpr auto DefaultMaxNote        = 125;

constexpr auto MinAmplitudeDb        = -120;
constexpr auto MaxAmplitudeDb        = 12;
constexpr auto DefaultAmplitudeMinDb = -60;
constexpr auto DefaultAmplitudeMaxDb = 0;

constexpr auto DefaultAmplitudesEnabled   = false;
constexpr auto MinAmplitudeHoldTimeMs     = 0;
constexpr auto MaxAmplitudeHoldTimeMs     = 5000;
constexpr auto DefaultAmplitudeHoldTimeMs = 0;
constexpr auto MinAmplitudeGravity        = 0;
constexpr auto MaxAmplitudeGravity        = 500;
constexpr auto DefaultAmplitudeGravity    = 100;

constexpr auto DefaultPeaksEnabled   = true;
constexpr auto MinPeakHoldTimeMs     = 0;
constexpr auto MaxPeakHoldTimeMs     = 5000;
constexpr auto DefaultPeakHoldTimeMs = 500;
constexpr auto MinPeakGravity        = 0;
constexpr auto MaxPeakGravity        = 500;
constexpr auto DefaultPeakGravity    = 50;

constexpr auto MinUpdateFps     = 20;
constexpr auto MaxUpdateFps     = 240;
constexpr auto DefaultUpdateFps = 40;

constexpr auto MinFftSize     = 256;
constexpr auto MaxFftSize     = 16384;
constexpr auto DefaultFftSize = 8192;
} // namespace Fooyin::Spectrum