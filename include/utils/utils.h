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

#include <QDateTime>
#include <QStringList>

class QColor;
class QHeaderView;
class QIcon;
class QImage;
class QMainWindow;
class QMenu;
class QPixmap;
class QSize;
class QString;

namespace Fooyin::Utils {
FYUTILS_EXPORT int randomNumber(int min, int max);

FYUTILS_EXPORT uint64_t currentDateToInt();
FYUTILS_EXPORT QString formatTimeMs(uint64_t time);
FYUTILS_EXPORT std::array<const char*, 6> dateFormats();
FYUTILS_EXPORT QDateTime dateStringToDate(const QString& str);
FYUTILS_EXPORT std::optional<int64_t> dateStringToMs(const QString& str);
FYUTILS_EXPORT QString msToDateString(int64_t dateMs);

FYUTILS_EXPORT QStringList extensionsToWildcards(const QStringList& extensions);
FYUTILS_EXPORT QString extensionsToFilterList(const QStringList& extensions, const QString& name);
FYUTILS_EXPORT QString extensionFromFilter(const QString& filter);

FYUTILS_EXPORT QPixmap scalePixmap(const QPixmap& image, const QSize& size, double dpr, bool upscale = false);
FYUTILS_EXPORT QPixmap scalePixmap(const QPixmap& image, int width, double dpr, bool upscale = false);
FYUTILS_EXPORT QImage scaleImage(const QImage& image, const QSize& size, double dpr, bool upscale = false);
FYUTILS_EXPORT QImage scaleImage(const QImage& image, int width, double dpr, bool upscale = false);
FYUTILS_EXPORT QPixmap changePixmapColour(const QPixmap& orig, const QColor& color);

FYUTILS_EXPORT QMainWindow* getMainWindow();
FYUTILS_EXPORT double windowDpr();
FYUTILS_EXPORT void showMessageBox(const QString& text, const QString& infoText);
FYUTILS_EXPORT void appendMenuActions(QMenu* originalMenu, QMenu* menu);
FYUTILS_EXPORT int visibleSectionCount(const QHeaderView* headerView);
FYUTILS_EXPORT int firstVisualIndex(const QHeaderView* headerView);
FYUTILS_EXPORT int realVisualIndex(const QHeaderView* headerView, int logicalIndex);
FYUTILS_EXPORT std::vector<int> logicalIndexOrder(const QHeaderView* headerView);

FYUTILS_EXPORT bool isDarkMode();
FYUTILS_EXPORT QIcon iconFromTheme(const QString& icon);
FYUTILS_EXPORT QIcon iconFromTheme(const char* icon);
FYUTILS_EXPORT QPixmap pixmapFromTheme(const char* icon);
FYUTILS_EXPORT QPixmap pixmapFromTheme(const char* icon, const QSize& size);
} // namespace Fooyin::Utils
