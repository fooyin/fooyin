/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

    TextBlock titleBlock{"$if2(%albumartist%,Unknown Artist)", 2};
    titleBlock.font.setBold(true);
    preset.header.title.emplace_back(titleBlock);
    preset.header.subtitle.emplace_back("$if2(%album%,Unknown Album)", 1);
    TextBlock sideBlock{"%year%", 1};
    sideBlock.font.setBold(true);
    preset.header.sideText.emplace_back(sideBlock);
    preset.header.info.emplace_back("[%genres% | ]%trackcount% $ifgreater(%trackcount%,1,Tracks,Track) | %playtime%",
                                    -1);

    Fooyin::SubheaderRow subheader;
    subheader.leftText.emplace_back("$ifgreater(%disctotal%,1,Disc #%disc%)");
    subheader.rightText.emplace_back("$ifgreater(%disctotal%,1,%playtime%)");
    preset.subHeaders.push_back(subheader);

    preset.track.leftText.emplace_back("$num(%track%,2).   ");
    preset.track.leftText.emplace_back("%title%");
    preset.track.rightText.emplace_back("$ifgreater(%playcount%,0,%playcount% |)      ");
    preset.track.rightText.emplace_back("$timems(%duration%)");

    addDefaultItem(preset);

    preset.subHeaders.clear();

    preset.name = QStringLiteral("Split Discs");

    preset.header.subtitle.clear();
    preset.header.subtitle.emplace_back("$if2(%album%,Unknown Album)$ifgreater(%disctotal%,1, ▪ Disc #%disc%)", 1);

    addDefaultItem(preset);

    preset.name = QStringLiteral("Simple Header");

    preset.header.simple = true;
    preset.header.title.clear();
    preset.header.subtitle.clear();
    preset.header.title.emplace_back("$if2(%albumartist%,Unknown Artist) ▪ $if2(%album%,Unknown Album)", 2);

    addDefaultItem(preset);

    preset.name = QStringLiteral("Track Table");

    preset.header = {};
    preset.subHeaders.clear();

    addDefaultItem(preset);
}
} // namespace Fooyin

#include "moc_presetregistry.cpp"
