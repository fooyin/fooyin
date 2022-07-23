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

#include "library/models/track.h"
#include "widgets/widgetprovider.h"

#include <QPixmap>
#include <QtGlobal>

class QString;
class QLabel;
class QWidget;

namespace Util {
namespace File {
    QString cleanPath(const QString& path);
    bool isSamePath(const QString& filename1, const QString& filename2);
    bool isSubdir(const QString& dir, const QString& parentDir);
    bool exists(const QString& filename);
    QString getParentDirectory(const QString& filename);
    QString coverInDirectory(const QString& directory);
    bool createDirectories(const QString& path);
} // namespace File

namespace Widgets {
    QWidget* indentWidget(QWidget* widget, QWidget* parent);
} // namespace Widgets

int randomNumber(int min, int max);
QString msToString(quint64 ms);
QString secsToString(quint64 secs);
void setMinimumWidth(QLabel* label, const QString& text);

quint64 currentDateToInt();

QString calcAlbumHash(const QString& albumName, const QString& albumArtist, int year);
QString calcCoverHash(const QString& albumName, const QString& albumArtist);
QPixmap getCover(const QString& path, int size);
bool saveCover(const QPixmap& cover, const QString& hash);
QString storeCover(const Track& track);
QString capitalise(const QString& s);
} // namespace Util
