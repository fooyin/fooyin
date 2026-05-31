/*
 * Fooyin
 * Copyright © 2026, Piotr Wicijowski <piotr@wicijowski.pl>
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

#include <core/playlist/playlist.h>
#include <utils/id.h>

#include <QObject>

namespace Fooyin {
/*!
 * Provides access to the playlist currently selected in the UI.
 *
 * This is UI selection state and is separate from the active playback playlist.
 */
class FYGUI_EXPORT CurrentPlaylistController : public QObject
{
    Q_OBJECT

public:
    [[nodiscard]] virtual Playlist* currentPlaylist() const = 0;
    [[nodiscard]] virtual UId currentPlaylistId() const     = 0;
    virtual void changeCurrentPlaylist(const UId& id)       = 0;

Q_SIGNALS:
    void currentPlaylistChanged(Fooyin::Playlist* previous, Fooyin::Playlist* current);

protected:
    using QObject::QObject;
};
} // namespace Fooyin
