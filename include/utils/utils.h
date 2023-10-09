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

#include "fyutils_export.h"

#include <QPixmap>
#include <QtGlobal>

class QString;
class QLabel;
class QWidget;
class QMenu;

namespace Fy::Utils {
namespace File {
FYUTILS_EXPORT QString cleanPath(const QString& path);
FYUTILS_EXPORT bool isSamePath(const QString& filename1, const QString& filename2);
FYUTILS_EXPORT bool isSubdir(const QString& dir, const QString& parentDir);
FYUTILS_EXPORT bool exists(const QString& filename);
FYUTILS_EXPORT QString getParentDirectory(const QString& filename);
FYUTILS_EXPORT bool createDirectories(const QString& path);
FYUTILS_EXPORT void openDirectory(const QString& dir);
} // namespace File

FYUTILS_EXPORT int randomNumber(int min, int max);
FYUTILS_EXPORT QString msToString(uint64_t ms);
FYUTILS_EXPORT QString secsToString(uint64_t secs);
FYUTILS_EXPORT QString formatFileSize(uint64_t bytes);
FYUTILS_EXPORT void setMinimumWidth(QLabel* label, const QString& text);

FYUTILS_EXPORT uint64_t currentDateToInt();
FYUTILS_EXPORT QString formatTimeMs(uint64_t time);
FYUTILS_EXPORT QString capitalise(const QString& s);

FYUTILS_EXPORT QPixmap scaleImage(QPixmap& image, int size);
FYUTILS_EXPORT QPixmap changePixmapColour(const QPixmap& orig, const QColor& color);
FYUTILS_EXPORT void showMessageBox(const QString& text, const QString& infoText);
FYUTILS_EXPORT void cloneMenu(QMenu* originalMenu, QMenu* clonedMenu);
} // namespace Fy::Utils
