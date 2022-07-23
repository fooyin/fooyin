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

#include "utils.h"

#include "paths.h"
#include "tagging/tags.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QPixmap>
#include <QRandomGenerator>
#include <QVBoxLayout>
#include <QWidget>

namespace Util {
namespace File {
    QString cleanPath(const QString& path)
    {
        return (path.trimmed().isEmpty()) ? QString("") : QDir::cleanPath(path);
    }

    bool isSamePath(const QString& filename1, const QString& filename2)
    {
        const auto cleaned1 = cleanPath(filename1);
        const auto cleaned2 = cleanPath(filename2);

        return (cleaned1.compare(cleaned2) == 0);
    }

    bool isSubdir(const QString& dir, const QString& parentDir)
    {
        if(isSamePath(dir, parentDir)) {
            return false;
        }

        const auto cleanedDir = cleanPath(dir);
        const auto cleanedParentDir = cleanPath(parentDir);

        if(cleanedDir.isEmpty() || cleanedParentDir.isEmpty()) {
            return false;
        }

        const QFileInfo info(cleanedDir);

        QDir d1(cleanedDir);
        if(info.exists() && info.isFile()) {
            const auto d1String = getParentDirectory(cleanedDir);
            if(isSamePath(d1String, parentDir)) {
                return true;
            }

            d1 = QDir(d1String);
        }

        const QDir d2(cleanedParentDir);

        while(!d1.isRoot()) {
            d1 = QDir(getParentDirectory(d1.absolutePath()));
            if(isSamePath(d1.absolutePath(), d2.absolutePath())) {
                return true;
            }
        }

        return false;
    }

    bool exists(const QString& filename)
    {
        return (!filename.isEmpty()) && QFile::exists(filename);
    }

    QString getParentDirectory(const QString& filename)
    {
        const auto cleaned = cleanPath(filename);
        const auto index = cleaned.lastIndexOf(QDir::separator());

        return (index > 0) ? cleanPath(cleaned.left(index)) : QDir::rootPath();
    }

    QString coverInDirectory(const QString& directory)
    {
        QDir baseDirectory = QDir(directory);
        QStringList fileExtensions{"*.jpg", "*.jpeg", "*.png", "*.gif", "*.tiff", "*.bmp"};
        // Use first image found as album cover
        QStringList fileList = baseDirectory.entryList(fileExtensions, QDir::Files);
        if(!fileList.isEmpty()) {
            QString cover = baseDirectory.absolutePath() + "/" + fileList.constFirst();
            return cover;
        }
        return {};
    }

    bool createDirectories(const QString& path)
    {
        return QDir().mkpath(path);
    }
}; // namespace File

namespace Widgets {
    QWidget* indentWidget(QWidget* widget, QWidget* parent)
    {
        auto* indentWidget = new QWidget(parent);
        indentWidget->setLayout(new QVBoxLayout());
        indentWidget->layout()->addWidget(widget);
        indentWidget->layout()->setContentsMargins(25, 0, 0, 0);
        return indentWidget;
    }
} // namespace Widgets

int randomNumber(int min, int max)
{
    if(min == max) {
        return max;
    }
    return QRandomGenerator::global()->bounded(min, max + 1);
}

QString msToString(quint64 ms)
{
    int milliseconds = static_cast<int>(ms);
    QTime t(0, 0, 0);
    auto time = t.addMSecs(milliseconds);
    return time.toString(time.hour() == 0 ? "mm:ss" : "hh:mm:ss");
}

QString secsToString(quint64 secs)
{
    int seconds = static_cast<int>(secs);
    QTime t(0, 0, 0);
    auto time = t.addSecs(seconds);
    return time.toString(time.hour() == 0 ? "mm:ss" : "hh:mm:ss");
}

quint64 currentDateToInt()
{
    const auto str = QDateTime::currentDateTimeUtc().toString("yyyyMMddHHmmss");
    return str.toULongLong();
}

QString calcAlbumHash(const QString& albumName, const QString& albumArtist, int year)
{
    const QString albumId = albumName.toLower() + albumArtist.toLower() + QString::number(year);
    QString albumKey = QCryptographicHash::hash(albumId.toUtf8(), QCryptographicHash::Sha1).toHex();
    return albumKey;
}

QString calcCoverHash(const QString& albumName, const QString& albumArtist)
{
    const QString albumId = albumName.toLower() + albumArtist.toLower();
    QString albumKey = QCryptographicHash::hash(albumId.toUtf8(), QCryptographicHash::Sha1).toHex();
    return albumKey;
}

QPixmap getCover(const QString& path, int size)
{
    if(File::exists(path)) {
        QPixmap cover;
        cover.load(path);
        if(!cover.isNull()) {
            static const int maximumSize = size;
            const int width = cover.size().width();
            const int height = cover.size().height();
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
    auto path = coverPath();
    QFile file(QString(path + hash).append(".jpg"));
    file.open(QIODevice::WriteOnly);
    return cover.save(&file, "JPG", 100);
}

QString storeCover(const Track& track)
{
    QString coverPath = "";
    QString coverHash = Util::calcCoverHash(track.album(), track.albumArtist());

    const QString cacheCover = Util::coverPath() + coverHash + ".jpg";
    const QString folderCover = Util::File::coverInDirectory(Util::File::getParentDirectory(track.filepath()));

    if(Util::File::exists(cacheCover)) {
        coverPath = cacheCover;
    }
    else if(!folderCover.isEmpty()) {
        coverPath = folderCover;
    }
    else {
        QPixmap cover = Tagging::readCover(track.filepath());
        if(!cover.isNull()) {
            bool saved = Util::saveCover(cover, coverHash);
            if(saved) {
                coverPath = cacheCover;
            }
        }
    }
    return coverPath;
}

void setMinimumWidth(QLabel* label, const QString& text)
{
    QString oldText = label->text();
    label->setText(text);
    label->setMinimumWidth(0);
    auto width = label->sizeHint().width();
    label->setText(oldText);

    label->setMinimumWidth(width);
}

QString capitalise(const QString& s)
{
    QStringList parts = s.split(' ', Qt::SkipEmptyParts);

    for(auto& part : parts) {
        part.replace(0, 1, part[0].toUpper());
    }

    return parts.join(" ");
}
}; // namespace Util
