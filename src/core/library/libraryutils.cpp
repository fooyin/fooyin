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

#include "libraryutils.h"

#include "core/corepaths.h"
#include "core/tagging/tags.h"

#include <utils/utils.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>

using namespace Fy::Utils;

namespace Fy::Core::Library::Utils {
QString storeCover(const Track& track)
{
    QString coverPath       = "";
    const QString coverHash = calcCoverHash(track.album(), track.albumArtist());

    const QString cacheCover  = Core::coverPath() + coverHash + ".jpg";
    const QString folderCover = coverInDirectory(File::getParentDirectory(track.filepath()));

    if(File::exists(cacheCover)) {
        coverPath = cacheCover;
    }
    else if(!folderCover.isEmpty()) {
        coverPath = folderCover;
    }
    else {
        const QPixmap cover = Tagging::readCover(track.filepath());
        if(!cover.isNull()) {
            const bool saved = saveCover(cover, coverHash);
            if(saved) {
                coverPath = cacheCover;
            }
        }
    }
    return coverPath;
}

QString calcAlbumHash(const QString& albumName, const QString& albumArtist, const QString& date)
{
    const QString albumId = albumName.toLower() + albumArtist.toLower() + date;
    QString albumKey      = QCryptographicHash::hash(albumId.toUtf8(), QCryptographicHash::Sha1).toHex();
    return albumKey;
}

QString calcCoverHash(const QString& albumName, const QString& albumArtist)
{
    const QString albumId = albumName.toLower() + albumArtist.toLower();
    QString albumKey      = QCryptographicHash::hash(albumId.toUtf8(), QCryptographicHash::Sha1).toHex();
    return albumKey;
}

QPixmap getCover(const QString& path, int size)
{
    if(File::exists(path)) {
        QPixmap cover;
        cover.load(path);
        if(!cover.isNull()) {
            static const int maximumSize = size;
            const int width              = cover.size().width();
            const int height             = cover.size().height();
            if(width > maximumSize || height > maximumSize) {
                cover = cover.scaled(maximumSize, maximumSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            return cover;
        }
    }
    return {};
}

bool saveCover(const QPixmap& cover, const QString& hash)
{
    auto path = Core::coverPath();
    QFile file(QString(path + hash).append(".jpg"));
    file.open(QIODevice::WriteOnly);
    return cover.save(&file, "JPG", 100);
}

QString coverInDirectory(const QString& directory)
{
    const QDir baseDirectory = QDir(directory);
    const QStringList fileExtensions{"*.jpg", "*.jpeg", "*.png", "*.gif", "*.tiff", "*.bmp"};
    // Use first image found as album cover
    const QStringList fileList = baseDirectory.entryList(fileExtensions, QDir::Files);
    if(!fileList.isEmpty()) {
        QString cover = baseDirectory.absolutePath() + "/" + fileList.constFirst();
        return cover;
    }
    return {};
}

} // namespace Fy::Core::Library::Utils
