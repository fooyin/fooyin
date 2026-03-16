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
class PlaylistWidget;
class UId;

class PlaylistInteractor : public QObject
{
    Q_OBJECT

public:
    PlaylistInteractor(PlaylistHandler* handler, PlaylistController* controller, MusicLibrary* library,
                       QObject* parent = nullptr);

    [[nodiscard]] PlaylistHandler* handler() const;
    [[nodiscard]] PlaylistController* playlistController() const;
    [[nodiscard]] MusicLibrary* library() const;
    [[nodiscard]] PlayerController* playerController() const;

    void filesToPlaylist(const QList<QUrl>& urls, const UId& id);
    void filesToCurrentPlaylist(const QList<QUrl>& urls);
    void filesToCurrentPlaylistReplace(const QList<QUrl>& urls, bool play = false);
    void filesToNewPlaylist(const QString& playlistName, const QList<QUrl>& urls, bool play = false);
    void filesToNewPlaylistReplace(const QString& playlistName, const QList<QUrl>& urls, bool play = false);
    void filesToActivePlaylist(const QList<QUrl>& urls);
    void loadPlaylist(const QList<QPair<QString, QUrl>>& playlistData, bool play = false);

    template <typename Func>
    void filesToTracks(const QList<QUrl>& urls, Func&& func)
    {
        if(urls.empty()) {
            return;
        }

        scanFiles(urls, std::forward<Func>(func));
    }

    void trackMimeToPlaylist(const QByteArray& data, const UId& id);

private:
    [[nodiscard]] ScanRequest startFileScan(const QList<QUrl>& urls) const;
    [[nodiscard]] ScanRequest startPlaylistLoad(const QList<QUrl>& urls) const;
    void beginTrackScan(const QString& labelText, const ScanRequest& request,
                        std::function<void(const TrackList&)> func);
    void activatePlaylist(Playlist* playlist, bool play = false) const;
    void activatePlaylist(Playlist* playlist, int indexToPlay, bool play = false) const;
    void appendToPlaylist(Playlist* playlist, const TrackList& tracks) const;
    [[nodiscard]] Playlist* appendOrCreateNamedPlaylist(const QString& playlistName, const TrackList& tracks) const;

    template <typename Func>
    void scanFiles(const QList<QUrl>& urls, Func&& func)
    {
        beginTrackScan(tr("Reading tracks…"), startFileScan(urls), std::forward<Func>(func));
    }

    template <typename Func>
    void loadPlaylistTracks(const QList<QUrl>& urls, Func&& func)
    {
        beginTrackScan(tr("Loading playlist…"), startPlaylistLoad(urls), std::forward<Func>(func));
    }

    PlaylistHandler* m_handler;
    PlaylistController* m_controller;
    MusicLibrary* m_library;
};
} // namespace Fooyin
