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

#pragma once

#include "equaliserdsp.h"

#include <QByteArray>
#include <QString>

#include <array>
#include <optional>

namespace Fooyin::Equaliser {
constexpr auto EqualiserSliderScale = 10;

struct EqualiserSliderState
{
    int preamp{0};
    std::array<int, EqualiserDsp::BandCount> bands{};
};

[[nodiscard]] int gainToSliderValue(double gainDb);
[[nodiscard]] double sliderValueToGain(int sliderValue);
[[nodiscard]] QString gainText(int sliderValue);

[[nodiscard]] std::optional<EqualiserSliderState> sliderStateFromSettings(const QByteArray& settings);
[[nodiscard]] QByteArray settingsFromSliderState(const EqualiserSliderState& state);

[[nodiscard]] EqualiserSliderState zeroSliderState();
bool autoLevelSliderState(EqualiserSliderState& state);
} // namespace Fooyin::Equaliser
