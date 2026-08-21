/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <core/player/playbackqueue.h>
#include <core/ratingsymbols.h>
#include <core/track.h>
#include <gui/theme/fytheme.h>

#include <QByteArray>
#include <QModelIndexList>
#include <QStyleOptionViewItem>

class QMimeData;
class QLabel;
class QAbstractItemView;
class QPalette;
class QPainter;
class QPixmap;

namespace Fooyin {
class MusicLibrary;
class SettingsManager;

namespace Gui {
FYGUI_EXPORT TrackList tracksFromMimeData(MusicLibrary* library, QByteArray data);
FYGUI_EXPORT void populateExternalTrackMimeData(const TrackList& tracks, QMimeData* mimeData);
FYGUI_EXPORT TrackList sortTracksForLibraryViewerPlaylist(SettingsManager* settings, const TrackList& tracks);
FYGUI_EXPORT TrackIds sortTrackIdsForLibraryViewerPlaylist(MusicLibrary* library, SettingsManager* settings,
                                                           const TrackIds& ids);
FYGUI_EXPORT QByteArray queueTracksToMimeData(const QueueTracks& tracks);
FYGUI_EXPORT QueueTracks queueTracksFromMimeData(MusicLibrary* library, QByteArray data);
FYGUI_EXPORT RatingStarSymbols ratingStarSymbols(const SettingsManager& settings);

FYGUI_EXPORT QMap<PaletteKey, QColor> coloursFromPalette();
FYGUI_EXPORT QMap<PaletteKey, QColor> coloursFromStylePalette();
FYGUI_EXPORT QMap<PaletteKey, QColor> coloursFromPalette(const QPalette& palette);
FYGUI_EXPORT bool styleSupportsCustomPalette(const QString& styleName);
FYGUI_EXPORT bool styleSupportsDarkMode(const QString& styleName);
FYGUI_EXPORT bool styleUsesNormalItemViewSelectionText(const QString& styleName, bool alternatingRows = false);
FYGUI_EXPORT QPalette::ColorRole itemViewSelectionTextRole(const QStyleOptionViewItem& option);
FYGUI_EXPORT QIcon::Mode itemViewIconMode(const QStyleOptionViewItem& option);

/*! Returns the style-provided item text rectangle with spacing for leading row decorations. */
FYGUI_EXPORT QRect itemViewTextRect(const QStyleOptionViewItem& option);

FYGUI_EXPORT void drawRoundedPixmap(QPainter& painter, const QRect& rect, Qt::Alignment alignment,
                                    const QPixmap& pixmap, int radiusPercent);

FYGUI_EXPORT void refreshItemViewPalette(QAbstractItemView* view);
FYGUI_EXPORT void refreshItemViewPalette(QAbstractItemView* view, const QPalette& palette);
FYGUI_EXPORT void updateItemViewStyle(QAbstractItemView* view);
FYGUI_EXPORT void updateItemViewStyle(QAbstractItemView* view, const QPalette& palette);

FYGUI_EXPORT QLabel* createSectionHeader(const QString& text, QWidget* parent = nullptr);
} // namespace Gui
} // namespace Fooyin
