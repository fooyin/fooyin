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

#include "soxresamplerplugin.h"

#include "soxresamplerdsp.h"
#include "soxresamplersettingswidget.h"

namespace Fooyin::Soxr {
std::vector<DspNode::Entry> SoxResamplerPlugin::dspCreators() const
{
    auto dsp = std::make_unique<SoxResamplerDSP>();
    return {{.id = dsp->id(), .name = QStringLiteral("Resampler (SoX)"), .factory = []() {
                 return std::make_unique<SoxResamplerDSP>();
             }}};
}

std::vector<std::unique_ptr<DspSettingsProvider>> SoxResamplerPlugin::settingsProviders() const
{
    std::vector<std::unique_ptr<DspSettingsProvider>> providers;
    providers.push_back(std::make_unique<SoxResamplerSettingsProvider>());
    return providers;
}
} // namespace Fooyin::Soxr
