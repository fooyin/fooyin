/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "coverprovider.h"

#include "core/library/libraryutils.h"
#include "core/library/models/album.h"

#include <QPixmapCache>

namespace Core::Covers {
QPixmap albumCover(Album* album)
{
    const auto id = QString::number(album->hasCover() ? album->id() : 0);
    QPixmap cover;
    if(!QPixmapCache::find(id, &cover)) {
        cover = Library::Utils::getCover(album->hasCover() ? album->coverPath() : "://images/nocover.png", 60);
        QPixmapCache::insert(id, cover);
    }
    return cover;
}
} // namespace Core::Covers
