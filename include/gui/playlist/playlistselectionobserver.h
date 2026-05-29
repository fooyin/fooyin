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

#include <utils/id.h>

#include <QObject>

namespace Fooyin {
class Playlist;

/*! Exposes the selected playlist in the main playlist UI flow to gui plugins. */
class FYGUI_EXPORT PlaylistSelectionObserver : public QObject
{
    Q_OBJECT

public:
    explicit PlaylistSelectionObserver(QObject* parent = nullptr);
    ~PlaylistSelectionObserver() override;

    [[nodiscard]] virtual Playlist* currentPlaylist() const = 0;
    [[nodiscard]] virtual UId currentPlaylistId() const     = 0;
    virtual void changeCurrentPlaylist(const UId& id)       = 0;

Q_SIGNALS:
    void currentPlaylistChanged(Fooyin::Playlist* previous, Fooyin::Playlist* current);
};
} // namespace Fooyin
