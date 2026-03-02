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

#include "equaliserplugin.h"

#include "equaliserdsp.h"
#include "equalisersettingswidget.h"

namespace Fooyin::Equaliser {
std::vector<DspNode::Entry> EqualiserPlugin::dspCreators() const
{
    auto dsp = std::make_unique<EqualiserDsp>();
    return {{.id = dsp->id(), .name = dsp->name(), .factory = []() { return std::make_unique<EqualiserDsp>(); }}};
}

std::vector<std::unique_ptr<DspSettingsProvider>> EqualiserPlugin::settingsProviders() const
{
    std::vector<std::unique_ptr<DspSettingsProvider>> providers;
    providers.push_back(std::make_unique<EqualiserSettingsProvider>());
    return providers;
}
} // namespace Fooyin::Equaliser
