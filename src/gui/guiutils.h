/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <core/player/playbackqueue.h>
#include <core/track.h>
#include <gui/theme/fytheme.h>

#include <QByteArray>
#include <QModelIndexList>

namespace Fooyin {
class MusicLibrary;

namespace Gui {
TrackList tracksFromMimeData(MusicLibrary* library, QByteArray data);
QByteArray queueTracksToMimeData(const QueueTracks& tracks);
QueueTracks queueTracksFromMimeData(MusicLibrary* library, QByteArray data);

QMap<PaletteKey, QColor> coloursFromPalette();
QMap<PaletteKey, QColor> coloursFromStylePalette();
QMap<PaletteKey, QColor> coloursFromPalette(const QPalette& palette);
} // namespace Gui
} // namespace Fooyin
