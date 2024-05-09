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

#include "playlistinteractor.h"

#include "playlistcontroller.h"
#include "playlistwidget.h"

#include <core/library/musiclibrary.h>
#include <core/playlist/playlist.h>
#include <core/playlist/playlisthandler.h>
#include <core/track.h>
#include <utils/fileutils.h>
#include <utils/id.h>

#include <QProgressDialog>

namespace Fooyin {
struct PlaylistInteractor::Private
{
    PlaylistHandler* handler;
    PlaylistController* controller;
    MusicLibrary* library;

    Private(PlaylistHandler* handler_, PlaylistController* controller_, MusicLibrary* library_)
        : handler{handler_}
        , controller{controller_}
        , library{library_}
    { }

    template <typename Func>
    void scanTracks(const TrackList& tracks, Func&& func) const
    {
        auto* scanDialog
            = new QProgressDialog(QStringLiteral("Reading tracks..."), QStringLiteral("Abort"), 0, 100, nullptr);
        scanDialog->setAttribute(Qt::WA_DeleteOnClose);
        scanDialog->setWindowModality(Qt::WindowModal);

        const ScanRequest request = library->scanTracks(tracks);

        QObject::connect(library, &MusicLibrary::scanProgress, scanDialog, [scanDialog, request](int id, int percent) {
            if(id != request.id) {
                return;
            }

            if(scanDialog->wasCanceled()) {
                request.cancel();
                scanDialog->close();
            }

            scanDialog->setValue(percent);
        });

        QObject::connect(library, &MusicLibrary::tracksScanned, scanDialog,
                         [request, func](int id, const TrackList& scannedTracks) {
                             if(id == request.id) {
                                 func(scannedTracks);
                             }
                         });
    }
};

PlaylistInteractor::PlaylistInteractor(PlaylistHandler* handler, PlaylistController* controller, MusicLibrary* library,
                                       QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(handler, controller, library)}
{ }

PlaylistInteractor::~PlaylistInteractor() = default;

PlaylistHandler* PlaylistInteractor::handler() const
{
    return p->handler;
}

PlaylistController* PlaylistInteractor::controller() const
{
    return p->controller;
}

MusicLibrary* PlaylistInteractor::library() const
{
    return p->library;
}

void PlaylistInteractor::filesToCurrentPlaylist(const QList<QUrl>& urls) const
{
    const QStringList filepaths = Utils::File::getFiles(urls, Track::supportedFileExtensions());
    if(filepaths.empty()) {
        return;
    }

    TrackList tracks;
    std::ranges::transform(filepaths, std::back_inserter(tracks), [](const QString& path) { return Track{path}; });

    p->scanTracks(tracks, [this](const TrackList& scannedTracks) {
        if(auto* playlist = p->controller->currentPlaylist()) {
            p->handler->appendToPlaylist(playlist->id(), scannedTracks);
        }
    });
}

void PlaylistInteractor::filesToCurrentPlaylistReplace(const QList<QUrl>& urls, bool play) const
{
    const QStringList filepaths = Utils::File::getFiles(urls, Track::supportedFileExtensions());
    if(filepaths.empty()) {
        return;
    }

    TrackList tracks;
    std::ranges::transform(filepaths, std::back_inserter(tracks), [](const QString& path) { return Track{path}; });

    p->scanTracks(tracks, [this, play](const TrackList& scannedTracks) {
        if(auto* playlist = p->controller->currentPlaylist()) {
            p->handler->replacePlaylistTracks(playlist->id(), scannedTracks);
            if(play) {
                p->handler->startPlayback(playlist);
            }
        }
    });
}

void PlaylistInteractor::filesToNewPlaylist(const QString& playlistName, const QList<QUrl>& urls, bool play) const
{
    const QStringList filepaths = Utils::File::getFiles(urls, Track::supportedFileExtensions());
    if(filepaths.empty()) {
        return;
    }

    TrackList tracks;
    std::ranges::transform(filepaths, std::back_inserter(tracks), [](const QString& path) { return Track{path}; });

    auto handleScanResult = [this, playlistName, play](const TrackList& scannedTracks) {
        Playlist* playlist = p->handler->playlistByName(playlistName);
        if(playlist) {
            const int indexToPlay = playlist->trackCount();
            p->handler->appendToPlaylist(playlist->id(), scannedTracks);
            playlist->changeCurrentIndex(indexToPlay);
        }
        else {
            playlist = p->handler->createPlaylist(playlistName, scannedTracks);
        }

        if(playlist) {
            p->controller->changeCurrentPlaylist(playlist);
            if(play) {
                p->handler->startPlayback(playlist);
            }
        }
    };

    p->scanTracks(tracks, handleScanResult);
}

void PlaylistInteractor::filesToActivePlaylist(const QList<QUrl>& urls) const
{
    if(!p->handler->activePlaylist()) {
        return;
    }

    const QStringList filepaths = Utils::File::getFiles(urls, Track::supportedFileExtensions());
    if(filepaths.empty()) {
        return;
    }

    TrackList tracks;
    std::ranges::transform(filepaths, std::back_inserter(tracks), [](const QString& path) { return Track{path}; });

    p->scanTracks(tracks, [this](const TrackList& scannedTracks) {
        if(auto* playlist = p->handler->activePlaylist()) {
            p->handler->appendToPlaylist(playlist->id(), scannedTracks);
        }
    });
}

void PlaylistInteractor::filesToTracks(const QList<QUrl>& urls, const std::function<void(const TrackList&)>& func) const
{
    const QStringList filepaths = Utils::File::getFiles(urls, Track::supportedFileExtensions());
    if(filepaths.empty()) {
        return;
    }

    TrackList tracks;
    std::ranges::transform(filepaths, std::back_inserter(tracks), [](const QString& path) { return Track{path}; });

    p->scanTracks(tracks, func);
}
} // namespace Fooyin
