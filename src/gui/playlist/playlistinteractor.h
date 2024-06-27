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

#include <core/track.h>

#include <QList>
#include <QObject>
#include <QUrl>

namespace Fooyin {
class Id;
class MusicLibrary;
class PlayerController;
class PlaylistController;
class PlaylistHandler;
class PlaylistWidget;

class PlaylistInteractor : public QObject
{
    Q_OBJECT

public:
    PlaylistInteractor(PlaylistHandler* handler, PlaylistController* controller, MusicLibrary* library,
                       QObject* parent = nullptr);
    ~PlaylistInteractor() override;

    [[nodiscard]] PlaylistHandler* handler() const;
    [[nodiscard]] PlaylistController* playlistController() const;
    [[nodiscard]] MusicLibrary* library() const;
    [[nodiscard]] PlayerController* playerController() const;

    void filesToPlaylist(const Id& id, const QList<QUrl>& urls) const;
    void filesToCurrentPlaylist(const QList<QUrl>& urls) const;
    void filesToCurrentPlaylistReplace(const QList<QUrl>& urls, bool play = false) const;
    void filesToNewPlaylist(const QString& playlistName, const QList<QUrl>& urls, bool play = false) const;
    void filesToActivePlaylist(const QList<QUrl>& urls) const;
    void filesToTracks(const QList<QUrl>& urls, const std::function<void(const TrackList&)>& func) const;

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
