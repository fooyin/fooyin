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

#pragma once

#include "fyutils_export.h"

#include <QStringList>
#include <QUrl>

class QColor;
class QDir;
class QIcon;
class QImage;
class QKeySequence;
class QLabel;
class QMenu;
class QPixmap;
class QSize;
class QString;
class QWidget;

namespace Fooyin::Utils {
FYUTILS_EXPORT int randomNumber(int min, int max);
FYUTILS_EXPORT QString msToString(uint64_t ms);
FYUTILS_EXPORT QString secsToString(uint64_t secs);
FYUTILS_EXPORT QString formatFileSize(uint64_t bytes, bool includeBytes = false);
FYUTILS_EXPORT QString addLeadingZero(int number, int leadingCount);
FYUTILS_EXPORT QString appendShortcut(const QString& str, const QKeySequence& shortcut);

FYUTILS_EXPORT uint64_t currentDateToInt();
FYUTILS_EXPORT QString formatTimeMs(uint64_t time);
FYUTILS_EXPORT QString capitalise(const QString& str);

FYUTILS_EXPORT QPixmap scalePixmap(const QPixmap& image, const QSize& size);
FYUTILS_EXPORT QPixmap scalePixmap(const QPixmap& image, int width);
FYUTILS_EXPORT QImage scaleImage(const QImage& image, const QSize& size);
FYUTILS_EXPORT QImage scaleImage(const QImage& image, int width);
FYUTILS_EXPORT QPixmap changePixmapColour(const QPixmap& orig, const QColor& color);

FYUTILS_EXPORT void showMessageBox(const QString& text, const QString& infoText);
FYUTILS_EXPORT void appendMenuActions(QMenu* originalMenu, QMenu* menu);

FYUTILS_EXPORT bool isDarkMode();
FYUTILS_EXPORT QIcon iconFromTheme(const QString& icon);
FYUTILS_EXPORT QIcon iconFromTheme(const char* icon);
} // namespace Fooyin::Utils
