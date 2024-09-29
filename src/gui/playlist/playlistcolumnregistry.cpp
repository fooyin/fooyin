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

namespace Fooyin {
PlaylistColumnRegistry::PlaylistColumnRegistry(SettingsManager* settings, QObject* parent)
    : ItemRegistry{QStringLiteral("PlaylistWidget/PlaylistColumns"), settings, parent}
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
    addDefaultItem({.id = 0, .name = tr("Track"), .field = QStringLiteral("[%disc%.]$num(%track%,2)")});
    addDefaultItem({.id = 1, .name = tr("Title"), .field = QStringLiteral("%title%")});
    addDefaultItem({.id = 2, .name = tr("Artist"), .field = QStringLiteral("%artist%")});
    addDefaultItem({.id = 3, .name = tr("Artist/Album"), .field = QStringLiteral("[%albumartist% - ]%album%")});
    addDefaultItem({.id = 4, .name = tr("Album Artist"), .field = QStringLiteral("%albumartist%")});
    addDefaultItem({.id = 5, .name = tr("Album"), .field = QStringLiteral("%album%")});
    addDefaultItem({.id = 7, .name = tr("Duration"), .field = QStringLiteral("%duration%")});
    addDefaultItem({.id = 8, .name = tr("Playing"), .field = QString::fromLatin1(PlayingIcon)});
    addDefaultItem({.id = 9, .name = tr("Codec"), .field = QStringLiteral("%codec%[ / %codec_profile%]")});
    addDefaultItem({.id = 10, .name = tr("Extension"), .field = QStringLiteral("%extension%")});
    addDefaultItem({.id = 11, .name = tr("Bitrate"), .field = QStringLiteral("%bitrate% kbps")});
    addDefaultItem({.id = 12, .name = tr("Sample Rate"), .field = QStringLiteral("%samplerate% Hz")});
    addDefaultItem({.id = 16, .name = tr("Channels"), .field = QStringLiteral("%channels%")});
    addDefaultItem({.id = 18, .name = tr("Bit Depth"), .field = QStringLiteral("%bitdepth%")});
    addDefaultItem({.id = 17, .name = tr("Last Modified"), .field = QStringLiteral("%lastmodified%")});
    addDefaultItem(
        {.id = 6, .name = tr("Playcount"), .field = QStringLiteral("$ifgreater(%playcount%,0,%playcount%)")});
    addDefaultItem({.id = 21, .name = tr("Rating"), .field = QString::fromLatin1(RatingEditor)});
    addDefaultItem({.id = 19, .name = tr("First Played"), .field = QStringLiteral("%firstplayed%")});
    addDefaultItem({.id = 20, .name = tr("Last Played"), .field = QStringLiteral("%lastplayed%")});
    addDefaultItem({.id = 13, .name = tr("Front Cover"), .field = QString::fromLatin1(FrontCover), .isPixmap = true});
    addDefaultItem({.id = 14, .name = tr("Back Cover"), .field = QString::fromLatin1(BackCover), .isPixmap = true});
    addDefaultItem(
        {.id = 15, .name = tr("Artist Picture"), .field = QString::fromLatin1(ArtistPicture), .isPixmap = true});
}
} // namespace Fooyin
