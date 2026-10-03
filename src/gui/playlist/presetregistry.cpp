/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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
namespace {
QString albumHeaderScript(const QString& albumScript)
{
    return u"<b><sized=2>$if2(%albumartist%,Unknown Artist)</sized></b>\n$crlf()\n"
           "<sized=1>"_s
         + albumScript
         + u"</sized><hr/><right><b><sized=2>%year%</sized></b></right>\n$crlf()\n"
           "<sized=-1>[%genres% | ]%trackcount% $ifgreater(%trackcount%,1,Tracks,Track) | %playtime%</sized>\n$crlf()\n"
           "<hr/>"_s;
}
} // namespace

PresetRegistry::PresetRegistry(SettingsManager* settings, QObject* parent)
    : ItemRegistry{u"PlaylistWidget/Presets"_s, settings, u"PlaylistWidget/PresetOverrides"_s, parent}
{
    QObject::connect(this, &RegistryBase::itemChanged, this, [this](int id) {
        if(const auto preset = itemById(id)) {
            Q_EMIT presetChanged(preset.value());
        }
    });

    loadItems();
}

void PresetRegistry::loadDefaults()
{
    PlaylistPreset preset;

    preset.name = tr("Track list");

    preset.track.text.script = u"$padright(,$mul($sub(%depth%,1),5))[\\[%queueindexes%\\]  ]"
                               "[$num(%track%,2).  ]%title%[<alpha=180>  ▪  %trackartist%</alpha>]\n"
                               "<right>\n"
                               "$ifgreater(%playcount%,0,%playcount% |)      %duration%"_s;

    addDefaultItem(preset);

    preset.name = tr("Albums grouped by disc");

    preset.header.text.script = albumHeaderScript(u"$if2(%album%,Unknown Album)"_s);

    SubheaderRow subheader;
    subheader.text.script = u"$ifgreater(%disctotal%,1,Disc #%disc%)\n<hr/>\n"
                            "<right>\n"
                            "$ifgreater(%disctotal%,1,%playtime%)"_s;
    preset.subHeaders.push_back(subheader);

    addDefaultItem(preset);

    preset.subHeaders.clear();

    preset.name = tr("Albums with disc headers");

    preset.header.text.script
        = albumHeaderScript(u"$if2(%album%,Unknown Album)$ifgreater(%disctotal%,1, ▪ Disc #%disc%)"_s);

    addDefaultItem(preset);

    preset.name = tr("Compact album headers");

    preset.header.showCover = false;
    preset.header.text.script
        = u"<b><sized=2>$if2(%albumartist%,Unknown Artist) ▪ $if2(%album%,Unknown Album)</sized></b>\n"
          "<hr/>\n<right><b><sized=2>%year%</sized></b>"_s;

    addDefaultItem(preset);
}
} // namespace Fooyin

#include "moc_presetregistry.cpp"
