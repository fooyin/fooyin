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

#include <core/library/musiclibrary.h>
#include <core/track.h>

#include <QList>
#include <QObject>
#include <QUrl>

#include <functional>

namespace Fooyin {
class PlayerController;
class Playlist;
class PlaylistController;
class PlaylistHandler;
class PlaylistInteractorPrivate;
class PlaylistWidget;
class SettingsManager;
class UId;

class FYGUI_EXPORT PlaylistInteractor : public QObject
{
    Q_OBJECT

public:
    PlaylistInteractor(PlaylistHandler* handler, PlaylistController* controller, MusicLibrary* library,
                       SettingsManager* settings, QObject* parent = nullptr);
    ~PlaylistInteractor() override;

    [[nodiscard]] PlaylistHandler* handler() const;
    [[nodiscard]] PlaylistController* playlistController() const;
    [[nodiscard]] MusicLibrary* library() const;
    [[nodiscard]] PlayerController* playerController() const;

    void filesToPlaylist(const QList<QUrl>& urls, const UId& id);
    void filesToCurrentPlaylist(const QList<QUrl>& urls);
    void filesToCurrentPlaylistAndPlayIfStopped(const QList<QUrl>& urls);
    void filesToCurrentPlaylistReplace(const QList<QUrl>& urls, bool play = false);
    void filesToNewPlaylist(const QString& playlistName, const QList<QUrl>& urls, bool play = false);
    void filesToNewPlaylist(const QString& playlistName, const QList<QUrl>& urls, const QUrl& fileToPlay, bool replace,
                            bool play = false);
    void filesToNewPlaylistReplace(const QString& playlistName, const QList<QUrl>& urls, bool play = false);
    void filesToActivePlaylist(const QList<QUrl>& urls);
    void loadPlaylist(const QList<QPair<QString, QUrl>>& playlistData, bool play = false);

    void tracksToPlaylist(const TrackList& tracks, const UId& id);
    void tracksToCurrentPlaylist(const TrackList& tracks);
    void tracksToCurrentPlaylistAndPlayIfStopped(const TrackList& tracks);
    void tracksToCurrentPlaylistReplace(const TrackList& tracks, bool play = false);
    void tracksToNewPlaylist(const QString& playlistName, const TrackList& tracks, bool play = false);
    void tracksToNewPlaylistReplace(const QString& playlistName, const TrackList& tracks, bool play = false);
    void tracksToActivePlaylist(const TrackList& tracks);

    void filesToTracks(const QList<QUrl>& urls, std::function<void(const TrackList&)> func);
    void playlistFilesToTracks(const QList<QUrl>& urls, std::function<void(const TrackList&)> func);

    void trackIdsToPlaylist(const QByteArray& data, const UId& id);

private:
    std::unique_ptr<PlaylistInteractorPrivate> p;
};
} // namespace Fooyin
