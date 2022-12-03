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

#pragma once

#include "core/library/models/track.h"

class QString;

namespace Library {
namespace Util {
    QString coverInDirectory(const QString& directory);
    QString calcAlbumHash(const QString& albumName, const QString& albumArtist, int year);
    QString calcCoverHash(const QString& albumName, const QString& albumArtist);
    QPixmap getCover(const QString& path, int size);
    bool saveCover(const QPixmap& cover, const QString& hash);
    QString storeCover(const Track& track);
}; // namespace Util
}; // namespace Library
