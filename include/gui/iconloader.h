/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "fygui_export.h"

#include <QStringList>

class QIcon;
class QPixmap;
class QAction;
class QObject;
class QPainter;
class QRect;
class QSize;
class QString;
class QStyleOptionViewItem;

namespace Fooyin::Gui {
/*!
 * GUI icon helpers for fooyin and system theme icons.
 *
 * They are main-thread only.
 */

/*! Sets the preferred bundled icon theme override and fallback theme. */
FYGUI_EXPORT bool setThemeIconOverrides(const QString& primaryTheme, const QString& fallbackTheme);

/*! Loads an icon by name using the current bundled override, then the system theme, then the bundled fallback. */
FYGUI_EXPORT QIcon iconFromTheme(const QString& icon);
/*! Convenience overload for Latin-1 icon names. */
FYGUI_EXPORT QIcon iconFromTheme(const char* icon);
/*! Returns the bundled action icon names available in fooyin's icon themes. */
FYGUI_EXPORT QStringList availableThemeIcons();

/*! Sets an action icon from the current theme and stores the icon name for future refreshes. */
FYGUI_EXPORT void setThemeIcon(QAction* action, const QString& icon);
/*! Convenience overload for Latin-1 icon names. */
FYGUI_EXPORT void setThemeIcon(QAction* action, const char* icon);
/*! Returns the stored theme icon name for an action, if any. */
FYGUI_EXPORT QString themeIconName(const QAction* action);
/*! Reapplies a stored themed icon to an action. Returns false if no theme icon name is stored. */
FYGUI_EXPORT bool refreshThemeIcon(QAction* action);
/*! Reapplies stored themed icons for all descendant actions of the given object. */
FYGUI_EXPORT void refreshThemeIcons(QObject* object);

/*! Returns a cached pixmap using the default icon size. */
FYGUI_EXPORT QPixmap pixmapFromTheme(const char* icon);
/*! Returns a cached pixmap for the requested size. */
FYGUI_EXPORT QPixmap pixmapFromTheme(const char* icon, const QSize& size);

/*! Draws a monochrome item-view icon using the text colour for its selection, focus and enabled state. */
FYGUI_EXPORT void drawItemViewIcon(QPainter* painter, const QStyleOptionViewItem& option, const QIcon& icon,
                                   const QRect& rect, Qt::Alignment alignment = Qt::AlignCenter);
} // namespace Fooyin::Gui
