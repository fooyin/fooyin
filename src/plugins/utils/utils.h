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
QString capitalise(const QString& s);
} // namespace Util
