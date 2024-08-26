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
    : ItemRegistry{QStringLiteral("PlaylistWidget/Presets"), settings, parent}
{
    QObject::connect(this, &RegistryBase::itemChanged, this, [this](int id) {
        if(const auto preset = itemById(id)) {
            emit presetChanged(preset.value());
        }
    });

    loadItems();
}

void PresetRegistry::loadDefaults()
{
    PlaylistPreset preset;

    preset.name = QStringLiteral("Track Table");

    preset.track.leftText.script  = QStringLiteral(" $padright(,$mul($sub(%depth%,1),5))[\\[%queueindexes%\\]  ]"
                                                    "[$num(%track%,2).  ]%title%[<alpha=180>  ▪  %uniqueartist%]");
    preset.track.rightText.script = QStringLiteral("$ifgreater(%playcount%,0,%playcount% |)      %duration% ");

    addDefaultItem(preset);

    preset.name = QStringLiteral("Album/Disc");

    preset.header.title.script    = QStringLiteral("<b><sized=2>$if2(%albumartist%,Unknown Artist)");
    preset.header.subtitle.script = QStringLiteral("<sized=1>$if2(%album%,Unknown Album)");
    preset.header.sideText.script = QStringLiteral("<b><sized=2>%year%</sized></b>");
    preset.header.info.script
        = QStringLiteral("<sized=-1>[%genres% | ]%trackcount% $ifgreater(%trackcount%,1,Tracks,Track) | %playtime%");

    SubheaderRow subheader;
    subheader.leftText.script  = QStringLiteral("$ifgreater(%disctotal%,1,Disc #%disc%)");
    subheader.rightText.script = QStringLiteral("$ifgreater(%disctotal%,1,%playtime%)");
    preset.subHeaders.push_back(subheader);

    addDefaultItem(preset);

    preset.subHeaders.clear();

    preset.name = QStringLiteral("Disc Albums");

    preset.header.subtitle.script
        = QStringLiteral("<sized=1>$if2(%album%,Unknown Album)$ifgreater(%disctotal%,1, ▪ Disc #%disc%)");

    addDefaultItem(preset);

    preset.name = QStringLiteral("Simple Artist/Album/Year");

    preset.header.simple = true;
    preset.header.subtitle.script.clear();
    preset.header.info.script.clear();
    preset.header.title.script
        = QStringLiteral("<b><sized=2>$if2(%albumartist%,Unknown Artist) ▪ $if2(%album%,Unknown Album)");

    addDefaultItem(preset);
}
} // namespace Fooyin

#include "moc_presetregistry.cpp"
