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
class QMenu;

namespace Fy::Utils {
namespace File {
QString cleanPath(const QString& path);
bool isSamePath(const QString& filename1, const QString& filename2);
bool isSubdir(const QString& dir, const QString& parentDir);
bool exists(const QString& filename);
QString getParentDirectory(const QString& filename);
bool createDirectories(const QString& path);
void openDirectory(const QString& dir);
} // namespace File

int randomNumber(int min, int max);
QString msToString(uint64_t ms);
QString secsToString(uint64_t secs);
QString formatFileSize(uint64_t bytes);
void setMinimumWidth(QLabel* label, const QString& text);

uint64_t currentDateToInt();
QString formatTimeMs(uint64_t time);
QString capitalise(const QString& s);

QPixmap scaleImage(QPixmap& image, int size);
QPixmap changePixmapColour(const QPixmap& orig, const QColor& color);
void showMessageBox(const QString& text, const QString& infoText);
void cloneMenu(QMenu* originalMenu, QMenu* clonedMenu);
} // namespace Fy::Utils
