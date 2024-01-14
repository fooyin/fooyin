/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

namespace Fooyin {
PresetRegistry::PresetRegistry(SettingsManager* settings, QObject* parent)
    : ItemRegistry{PlaylistPresets, settings, parent}
{
    QObject::connect(this, &RegistryBase::itemChanged, this, [this](int id) {
        const auto preset = itemById(id);
        emit presetChanged(preset);
    });

    loadItems();
}

void PresetRegistry::loadDefaults()
{
    PlaylistPreset preset;

    preset.name = QStringLiteral("Album - Disc");

    preset.header.rowHeight = 76;
    preset.header.title.emplace_back("$if(%albumartist%,%albumartist%,Unknown Artist)", 2);
    preset.header.subtitle.emplace_back("$if(%album%,%album%,Unknown Album)", 1);
    preset.header.sideText.emplace_back("%year%", 1);
    preset.header.info.emplace_back(
        "$if(%ggenres%,%ggenres% | )$ifgreater(%gcount%,1,%gcount% Tracks,%gcount% Track) | $timems(%gduration%)", -1);
    Fooyin::SubheaderRow subheader;
    subheader.rowHeight = 19;
    subheader.leftText.emplace_back("$ifgreater(%disctotal%,1,Disc #%disc%)", 1);
    subheader.rightText.emplace_back("$timems(%gduration%)", 1);
    preset.subHeaders.push_back(subheader);
    preset.track.rowHeight = 24;
    preset.track.leftText.emplace_back("$num(%track%,2).   ");
    preset.track.leftText.emplace_back("%title%");
    preset.track.rightText.emplace_back("$ifgreater(%playcount%,0,%playcount% |)      ");
    preset.track.rightText.emplace_back("$timems(%duration%)");

    addDefaultItem(preset);

    preset.subHeaders.clear();

    preset.name = QStringLiteral("Split Discs");

    preset.header.subtitle.clear();
    preset.header.subtitle.emplace_back("$if(%album%,%album%,Unknown Album)$ifgreater(%disctotal%,1, ▪ Disc #%disc%)",
                                        1);

    addDefaultItem(preset);

    preset.name = QStringLiteral("Simple Header");

    preset.header.simple    = true;
    preset.header.rowHeight = 30;
    preset.header.title.clear();
    preset.header.subtitle.clear();
    preset.header.title.emplace_back(
        "$if(%albumartist%,%albumartist%,Unknown Artist) ▪ $if(%album%,%album%,Unknown Album)", 2);

    addDefaultItem(preset);

    preset.name = QStringLiteral("Table");

    preset.header = {};
    preset.subHeaders.clear();

    addDefaultItem(preset);
}
} // namespace Fooyin

#include "moc_presetregistry.cpp"
