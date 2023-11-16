/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "presetregistry.h"

namespace {
void loadDefaults(Fooyin::PresetRegistry* registry)
{
    Fooyin::PlaylistPreset preset;

    preset.name = QStringLiteral("Album - Disc");

    preset.header.rowHeight = 76;
    preset.header.title.emplace_back("$if(%albumartist%,%albumartist%,Unknown Artist)", 16);
    preset.header.subtitle.emplace_back("$if(%album%,%album%,Unknown Album)", 14);
    preset.header.sideText.emplace_back("%year%", 14);
    preset.header.info.emplace_back(
        "$if(%ggenres%,%ggenres% | )$ifgreater(%gcount%,1,%gcount% Tracks,%gcount% Track) | $timems(%gduration%)", 12);
    Fooyin::SubheaderRow subheader;
    subheader.rowHeight = 19;
    subheader.leftText.emplace_back("$ifgreater(%disctotal%,1,Disc #%disc%)", 13);
    subheader.rightText.emplace_back("$timems(%gduration%)", 13);
    preset.subHeaders.push_back(subheader);
    preset.track.rowHeight = 23;
    preset.track.leftText.emplace_back("$num(%track%,2).   ", 13);
    preset.track.leftText.emplace_back("%title%", 13);
    preset.track.rightText.emplace_back("$ifgreater(%playcount%,0,| %playcount%)   ", 10);
    preset.track.rightText.emplace_back("$timems(%duration%)", 13);

    registry->addItem(preset);

    preset.subHeaders.clear();

    preset.name = QStringLiteral("Split Discs");

    preset.header.subtitle.clear();
    preset.header.subtitle.emplace_back("$if(%album%,%album%,Unknown Album)$ifgreater(%disctotal%,1, ▪ Disc #%disc%)",
                                        14);

    registry->addItem(preset);

    preset.name = QStringLiteral("Simple Header");

    preset.header.simple    = true;
    preset.header.rowHeight = 30;
    preset.header.title.clear();
    preset.header.subtitle.clear();
    preset.header.title.emplace_back(
        "$if(%albumartist%,%albumartist%,Unknown Artist) ▪ $if(%album%,%album%,Unknown Album)", 16);

    registry->addItem(preset);
}
} // namespace

namespace Fooyin {
PresetRegistry::PresetRegistry(SettingsManager* settings, QObject* parent)
    : ItemRegistry{settings, parent}
{
    QObject::connect(this, &RegistryBase::itemChanged, this, [this](int id) {
        const auto preset = itemById(id);
        emit presetChanged(preset);
    });
}

void PresetRegistry::loadItems()
{
    ItemRegistry::loadItems();

    if(m_items.empty()) {
        loadDefaults(this);
    }
}
} // namespace Fooyin

#include "moc_presetregistry.cpp"
