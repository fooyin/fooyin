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

#include "dsppresetregistry.h"

#include <ranges>

using namespace Qt::StringLiterals;

namespace Fooyin {
DspPresetRegistry::DspPresetRegistry(SettingsManager* settings, QObject* parent)
    : ItemRegistry{u"DSP/ChainPresets"_s, settings, parent}
{
    QObject::connect(this, &RegistryBase::itemChanged, this, [this](int id) {
        if(const auto preset = itemById(id)) {
            Q_EMIT presetChanged(preset.value());
        }
    });

    loadItems();
}

DspPresetRegistry::ItemList DspPresetRegistry::presetsByName() const
{
    auto presets = items();
    std::ranges::sort(presets, [](const auto& lhs, const auto& rhs) {
        const int comparison = QString::localeAwareCompare(lhs.name, rhs.name);
        return comparison != 0 ? comparison < 0 : lhs.id < rhs.id;
    });
    return presets;
}

void DspPresetRegistry::loadDefaults() { }
} // namespace Fooyin

#include "moc_dsppresetregistry.cpp"
