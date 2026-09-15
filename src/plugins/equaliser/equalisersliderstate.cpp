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

#include "equalisersliderstate.h"

#include <algorithm>
#include <cmath>

namespace Fooyin::Equaliser {
int gainToSliderValue(double gainDb)
{
    const double clamped = std::clamp(gainDb, -20.0, 20.0);
    return static_cast<int>(std::lround(clamped * static_cast<double>(EqualiserSliderScale)));
}

double sliderValueToGain(int sliderValue)
{
    return std::clamp(static_cast<double>(sliderValue) / static_cast<double>(EqualiserSliderScale), -20.0, 20.0);
}

QString gainText(int sliderValue)
{
    return QString::number(sliderValueToGain(sliderValue), 'f', 1);
}

std::optional<EqualiserSliderState> sliderStateFromSettings(const QByteArray& settings)
{
    EqualiserDsp dsp;
    if(!dsp.loadSettings(settings)) {
        return {};
    }

    EqualiserSliderState state;
    state.preamp = gainToSliderValue(dsp.preampDb());

    for(int i{0}; std::cmp_less(i, state.bands.size()); ++i) {
        state.bands[i] = gainToSliderValue(dsp.bandDb(i));
    }

    return state;
}

QByteArray settingsFromSliderState(const EqualiserSliderState& state)
{
    EqualiserDsp dsp;
    dsp.setPreampDb(sliderValueToGain(state.preamp));

    for(size_t i{0}; i < state.bands.size(); ++i) {
        dsp.setBandDb(static_cast<int>(i), sliderValueToGain(state.bands[i]));
    }

    return dsp.saveSettings();
}

EqualiserSliderState zeroSliderState()
{
    return {};
}

bool autoLevelSliderState(EqualiserSliderState& state)
{
    const int maxBand = std::max(0, *std::ranges::max_element(state.bands));
    if(maxBand == 0) {
        return false;
    }

    for(int& band : state.bands) {
        band -= maxBand;
    }
    return true;
}
} // namespace Fooyin::Equaliser
