/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "core/constants.h"
#include "core/corepaths.h"
#include "core/models/album.h"
#include "core/models/track.h"

#include <utils/utils.h>

#include <QPixmapCache>

namespace Fy::Core::Library {
QPixmap getCover(const QString& path, int size)
{
    if(Utils::File::exists(path)) {
        QPixmap cover;
        cover.load(path);
        if(!cover.isNull()) {
            const int scale  = size * 4;
            const int width  = cover.size().width();
            const int height = cover.size().height();
            if(width > size || height > size) {
                cover = cover.scaled(scale, scale).scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            return cover;
        }
    }
    return {};
}

QPixmap CoverProvider::trackCover(const Track& track)
{
    QPixmap cover;
    const QString coverKey = track.hasCover() ? track.albumHash() : "|NoCover|";

    if(!QPixmapCache::find(coverKey, &cover)) {
        if(track.hasCover()) {
            if(track.hasEmbeddedCover()) {
                const QPixmap embeddedCover = m_tagReader.readCover(track.filepath());
                if(!embeddedCover.isNull()) {
                    cover = embeddedCover;
                }
            }
            else {
                const QPixmap folderCover = getCover(track.coverPath(), 512);
                if(!folderCover.isNull()) {
                    cover = folderCover;
                }
            }
        }
        else {
            cover = getCover(Constants::NoCover, 512);
        }
        QPixmapCache::insert(coverKey, cover);
    }
    return cover;
}

QPixmap CoverProvider::albumThumbnail(const Album& album) const
{
    QString coverKey  = "|NoCoverThumb|";
    QString coverPath = Constants::NoCover;
    if(album.hasCover()) {
        coverKey  = QString{"%1 - %2"}.arg(album.artist(), album.title());
        coverPath = QString{"%1%2.jpg"}.arg(Core::coverPath(), coverKey);
    }
    QPixmap cover;
    if(!QPixmapCache::find(coverKey, &cover)) {
        cover = getCover(coverPath, 60);
        QPixmapCache::insert(coverKey, cover);
    }
    return cover;
}
} // namespace Fy::Core::Library
