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

namespace Fooyin {
PlaylistColumnRegistry::PlaylistColumnRegistry(SettingsManager* settings, QObject* parent)
    : ItemRegistry{QStringLiteral("PlaylistWidget/PlaylistColumns"), settings, parent}
{
    QObject::connect(this, &RegistryBase::itemChanged, this, [this](int id) {
        const auto column = itemById(id);
        emit columnChanged(column);
    });

    loadItems();
}

void PlaylistColumnRegistry::loadDefaults()
{
    addDefaultItem({.id = 0, .index = 0, .name = tr("Track"), .field = QStringLiteral("[%disc%.]$num(%track%,2)")});
    addDefaultItem({.id = 1, .index = 1, .name = tr("Title"), .field = QStringLiteral("%title%")});
    addDefaultItem({.id = 2, .index = 2, .name = tr("Artist"), .field = QStringLiteral("%artist%")});
    addDefaultItem({.id    = 2,
                    .index = 2,
                    .name  = tr("Artist/Album"),
                    .field = QStringLiteral("[$if2(%albumartist%,%artist%) - ]%album%")});
    addDefaultItem({.id = 3, .index = 3, .name = tr("Album Artist"), .field = QStringLiteral("%albumartist%")});
    addDefaultItem({.id = 4, .index = 4, .name = tr("Album"), .field = QStringLiteral("%album%")});
    addDefaultItem({.id    = 5,
                    .index = 5,
                    .name  = tr("Playcount"),
                    .field = QStringLiteral("$ifgreater(%playcount%,0,%playcount%)")});
    addDefaultItem({.id = 6, .index = 6, .name = tr("Duration"), .field = QStringLiteral("$timems(%duration%)")});
    addDefaultItem({.id = 7, .index = 7, .name = tr("Playing"), .field = QString::fromLatin1(PlayingIcon)});
    addDefaultItem({.id = 8, .index = 8, .name = tr("Codec"), .field = QStringLiteral("%codec%")});
    addDefaultItem({.id = 9, .index = 9, .name = tr("Extension"), .field = QStringLiteral("%extension%")});
    addDefaultItem({.id = 10, .index = 10, .name = tr("Bitrate"), .field = QStringLiteral("%bitrate% kbps")});
    addDefaultItem({.id = 11, .index = 11, .name = tr("Sample Rate"), .field = QStringLiteral("%samplerate% Hz")});
    addDefaultItem({.id = 12, .index = 12, .name = tr("Front Cover"), .field = QString::fromLatin1(FrontCover)});
    addDefaultItem({.id = 13, .index = 13, .name = tr("Back Cover"), .field = QString::fromLatin1(BackCover)});
    addDefaultItem({.id = 14, .index = 14, .name = tr("Artist Picture"), .field = QString::fromLatin1(ArtistPicture)});
}
} // namespace Fooyin
