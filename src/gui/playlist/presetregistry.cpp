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

using namespace Qt::StringLiterals;

namespace Fooyin {
PresetRegistry::PresetRegistry(SettingsManager* settings, QObject* parent)
    : ItemRegistry{u"PlaylistWidget/Presets"_s, settings, parent}
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

    preset.name = tr("Track Table");

    preset.track.leftText.script = u" $padright(,$mul($sub(%depth%,1),5))[\\[%queueindexes%\\]  ]"
                                   "[$num(%track%,2).  ]%title%[<alpha=180>  ▪  %uniqueartist%]"_s;
    preset.track.rightText.script = u"$ifgreater(%playcount%,0,%playcount% |)      %duration% "_s;

    addDefaultItem(preset);

    preset.name = tr("Album/Disc");

    preset.header.title.script    = u"<b><sized=2>$if2(%albumartist%,Unknown Artist)"_s;
    preset.header.subtitle.script = u"<sized=1>$if2(%album%,Unknown Album)"_s;
    preset.header.sideText.script = u"<b><sized=2>%year%</sized></b>"_s;
    preset.header.info.script
        = u"<sized=-1>[%genres% | ]%trackcount% $ifgreater(%trackcount%,1,Tracks,Track) | %playtime%"_s;

    SubheaderRow subheader;
    subheader.leftText.script  = u"$ifgreater(%disctotal%,1,Disc #%disc%)"_s;
    subheader.rightText.script = u"$ifgreater(%disctotal%,1,%playtime%)"_s;
    preset.subHeaders.push_back(subheader);

    addDefaultItem(preset);

    preset.subHeaders.clear();

    preset.name = tr("Disc Albums");

    preset.header.subtitle.script = u"<sized=1>$if2(%album%,Unknown Album)$ifgreater(%disctotal%,1, ▪ Disc #%disc%)"_s;

    addDefaultItem(preset);

    preset.name = tr("Simple Artist/Album/Year");

    preset.header.simple = true;
    preset.header.subtitle.script.clear();
    preset.header.info.script.clear();
    preset.header.title.script = u"<b><sized=2>$if2(%albumartist%,Unknown Artist) ▪ $if2(%album%,Unknown Album)"_s;

    addDefaultItem(preset);
}
} // namespace Fooyin

#include "moc_presetregistry.cpp"
