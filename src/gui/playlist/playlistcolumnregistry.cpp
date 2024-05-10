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
    addDefaultItem({.name = tr("Track"), .field = QStringLiteral("[%disc%.]$num(%track%,2)")});
    addDefaultItem({.name = tr("Title"), .field = QStringLiteral("%title%")});
    addDefaultItem({.name = tr("Artist"), .field = QStringLiteral("%artist%")});
    addDefaultItem({.name = tr("Artist/Album"), .field = QStringLiteral("[$if2(%albumartist%,%artist%) - ]%album%")});
    addDefaultItem({.name = tr("Album Artist"), .field = QStringLiteral("%albumartist%")});
    addDefaultItem({.name = tr("Album"), .field = QStringLiteral("%album%")});
    addDefaultItem({.name = tr("Playcount"), .field = QStringLiteral("$ifgreater(%playcount%,0,%playcount%)")});
    addDefaultItem({.name = tr("Duration"), .field = QStringLiteral("$timems(%duration%)")});
    addDefaultItem({.name = tr("Playing"), .field = QString::fromLatin1(PlayingIcon)});
    addDefaultItem({.name = tr("Codec"), .field = QStringLiteral("%codec%")});
    addDefaultItem({.name = tr("Extension"), .field = QStringLiteral("%extension%")});
    addDefaultItem({.name = tr("Bitrate"), .field = QStringLiteral("%bitrate% kbps")});
    addDefaultItem({.name = tr("Sample Rate"), .field = QStringLiteral("%samplerate% Hz")});
    addDefaultItem({.name = tr("Front Cover"), .field = QString::fromLatin1(FrontCover), .isPixmap = true});
    addDefaultItem({.name = tr("Back Cover"), .field = QString::fromLatin1(BackCover), .isPixmap = true});
    addDefaultItem({.name = tr("Artist Picture"), .field = QString::fromLatin1(ArtistPicture), .isPixmap = true});
}
} // namespace Fooyin
