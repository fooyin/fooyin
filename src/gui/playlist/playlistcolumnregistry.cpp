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

#include "playlistcolumnregistry.h"

#include "playlistscriptregistry.h"

#include <core/constants.h>

using namespace Qt::StringLiterals;

namespace Fooyin {
PlaylistColumnRegistry::PlaylistColumnRegistry(SettingsManager* settings, QObject* parent)
    : ItemRegistry{u"PlaylistWidget/PlaylistColumns"_s, settings, parent}
{
    QObject::connect(this, &RegistryBase::itemChanged, this, [this](int id) {
        if(const auto column = itemById(id)) {
            emit columnChanged(column.value());
        }
    });

    loadItems();
}

void PlaylistColumnRegistry::loadDefaults()
{
    addDefaultItem({.id = 0, .name = tr("Track"), .field = u"[%disc%.]$num(%track%,2)"_s});
    addDefaultItem({.id = 1, .name = tr("Title"), .field = u"%title%"_s});
    addDefaultItem({.id = 2, .name = tr("Artist"), .field = u"%artist%"_s});
    addDefaultItem({.id = 3, .name = tr("Artist/Album"), .field = u"[%albumartist% - ]%album%"_s});
    addDefaultItem({.id = 4, .name = tr("Album Artist"), .field = u"%albumartist%"_s});
    addDefaultItem({.id = 5, .name = tr("Album"), .field = u"%album%"_s});
    addDefaultItem({.id = 7, .name = tr("Duration"), .field = u"%duration%"_s});
    addDefaultItem({.id = 8, .name = tr("Playing"), .field = QString::fromLatin1(PlayingIcon)});
    addDefaultItem({.id = 9, .name = tr("Codec"), .field = u"%codec%[ / %codec_profile%]"_s});
    addDefaultItem({.id = 10, .name = tr("Extension"), .field = u"%extension%"_s});
    addDefaultItem({.id = 11, .name = tr("Bitrate"), .field = u"%bitrate% kbps"_s});
    addDefaultItem({.id = 12, .name = tr("Sample Rate"), .field = u"%samplerate% Hz"_s});
    addDefaultItem({.id = 16, .name = tr("Channels"), .field = u"%channels%"_s});
    addDefaultItem({.id = 18, .name = tr("Bit Depth"), .field = u"%bitdepth%"_s});
    addDefaultItem({.id = 17, .name = tr("Last Modified"), .field = u"%lastmodified%"_s});
    addDefaultItem({.id = 6, .name = tr("Playcount"), .field = u"$ifgreater(%playcount%,0,%playcount%)"_s});
    addDefaultItem({.id = 21, .name = tr("Rating"), .field = u"%rating_editor%"_s});
    addDefaultItem({.id = 19, .name = tr("First Played"), .field = u"%firstplayed%"_s});
    addDefaultItem({.id = 20, .name = tr("Last Played"), .field = u"%lastplayed%"_s});
    addDefaultItem({.id = 13, .name = tr("Front Cover"), .field = u"%frontcover%"_s, .isPixmap = true});
    addDefaultItem({.id = 14, .name = tr("Back Cover"), .field = u"%backcover%"_s, .isPixmap = true});
    addDefaultItem({.id = 15, .name = tr("Artist Picture"), .field = u"%artistpicture%"_s, .isPixmap = true});
}
} // namespace Fooyin
