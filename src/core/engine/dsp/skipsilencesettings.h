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

namespace Fooyin::SkipSilenceSettings {
inline constexpr int MinSilenceMinMs                = 200;
inline constexpr int MinSilenceMaxMs                = 20000;
inline constexpr int ThresholdMinDb                 = -100;
inline constexpr int ThresholdMaxDb                 = -20;
inline constexpr int DefaultMinSilenceDurationMs    = 5000;
inline constexpr int DefaultThresholdDb             = -60;
inline constexpr bool DefaultKeepInitialPeriod      = true;
inline constexpr bool DefaultIncludeMiddleSilence   = false;
inline constexpr std::uint32_t SerializationVersion = 1;
} // namespace Fooyin::SkipSilenceSettings
