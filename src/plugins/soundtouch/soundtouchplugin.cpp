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

#include "soundtouchplugin.h"

#include "soundtouchdsp.h"
#include "soundtouchsettingswidget.h"

namespace Fooyin::SoundTouch {
std::vector<DspNode::Entry> SoundTouchPlugin::dspCreators() const
{
    auto tempo = std::make_unique<SoundTouchTempoDsp>();
    auto pitch = std::make_unique<SoundTouchPitchDsp>();
    auto rate  = std::make_unique<SoundTouchRateDsp>();

    return {
        {.id = tempo->id(), .name = tempo->name(), .factory = []() { return std::make_unique<SoundTouchTempoDsp>(); }},
        {.id = pitch->id(), .name = pitch->name(), .factory = []() { return std::make_unique<SoundTouchPitchDsp>(); }},
        {.id = rate->id(), .name = rate->name(), .factory = []() { return std::make_unique<SoundTouchRateDsp>(); }},
    };
}

std::vector<std::unique_ptr<DspSettingsProvider>> SoundTouchPlugin::settingsProviders() const
{
    std::vector<std::unique_ptr<DspSettingsProvider>> providers;
    providers.push_back(std::make_unique<SoundTouchTempoSettingsProvider>());
    providers.push_back(std::make_unique<SoundTouchPitchSettingsProvider>());
    providers.push_back(std::make_unique<SoundTouchRateSettingsProvider>());
    return providers;
}
} // namespace Fooyin::SoundTouch
