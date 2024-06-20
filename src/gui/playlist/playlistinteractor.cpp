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

#include <core/library/musiclibrary.h>
#include <core/playlist/playlist.h>
#include <core/playlist/playlisthandler.h>
#include <core/track.h>
#include <utils/fileutils.h>
#include <utils/utils.h>

#include <QMainWindow>
#include <QProgressDialog>

namespace Fooyin {
struct PlaylistInteractor::Private
{
    PlaylistHandler* m_handler;
    PlaylistController* m_controller;
    MusicLibrary* m_library;

    Private(PlaylistHandler* handler, PlaylistController* controller, MusicLibrary* library)
        : m_handler{handler}
        , m_controller{controller}
        , m_library{library}
    { }

    template <typename Func>
    void scanFiles(const QList<QUrl>& files, Func&& func) const
    {
        auto* scanDialog = new QProgressDialog(QStringLiteral("Reading tracks..."), QStringLiteral("Abort"), 0, 100,
                                               Utils::getMainWindow());
        scanDialog->setAttribute(Qt::WA_DeleteOnClose);
        scanDialog->setModal(true);
        scanDialog->setValue(0);

        const ScanRequest request = m_library->scanFiles(files);

        QObject::connect(m_library, &MusicLibrary::scanProgress, scanDialog,
                         [scanDialog, request](int id, int percent) {
                             if(id != request.id) {
                                 return;
                             }

                             if(scanDialog->wasCanceled()) {
                                 request.cancel();
                                 scanDialog->close();
                             }

                             scanDialog->setValue(percent);
                         });

        QObject::connect(m_library, &MusicLibrary::tracksScanned, scanDialog,
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
    return p->m_handler;
}

PlaylistController* PlaylistInteractor::controller() const
{
    return p->m_controller;
}

MusicLibrary* PlaylistInteractor::library() const
{
    return p->m_library;
}

void PlaylistInteractor::filesToPlaylist(const Id& id, const QList<QUrl>& urls) const
{
    if(urls.empty() || !id.isValid()) {
        return;
    }

    p->scanFiles(urls, [this, id](const TrackList& scannedTracks) {
        if(auto* playlist = p->m_controller->playlistHandler()->playlistById(id)) {
            p->m_handler->appendToPlaylist(playlist->id(), scannedTracks);
        }
    });
}

void PlaylistInteractor::filesToCurrentPlaylist(const QList<QUrl>& urls) const
{
    if(urls.empty()) {
        return;
    }

    p->scanFiles(urls, [this](const TrackList& scannedTracks) {
        if(auto* playlist = p->m_controller->currentPlaylist()) {
            p->m_handler->appendToPlaylist(playlist->id(), scannedTracks);
        }
    });
}

void PlaylistInteractor::filesToCurrentPlaylistReplace(const QList<QUrl>& urls, bool play) const
{
    if(urls.empty()) {
        return;
    }

    p->scanFiles(urls, [this, play](const TrackList& scannedTracks) {
        if(auto* playlist = p->m_controller->currentPlaylist()) {
            p->m_handler->replacePlaylistTracks(playlist->id(), scannedTracks);
            playlist->changeCurrentIndex(0);
            if(play) {
                p->m_handler->startPlayback(playlist);
            }
        }
    });
}

void PlaylistInteractor::filesToNewPlaylist(const QString& playlistName, const QList<QUrl>& urls, bool play) const
{
    if(urls.empty()) {
        return;
    }

    auto handleScanResult = [this, playlistName, play](const TrackList& scannedTracks) {
        Playlist* playlist = p->m_handler->playlistByName(playlistName);
        if(playlist) {
            const int indexToPlay = playlist->trackCount();
            p->m_handler->appendToPlaylist(playlist->id(), scannedTracks);
            playlist->changeCurrentIndex(indexToPlay);
        }
        else {
            playlist = p->m_handler->createPlaylist(playlistName, scannedTracks);
        }

        if(playlist) {
            p->m_controller->changeCurrentPlaylist(playlist);
            if(play) {
                p->m_handler->startPlayback(playlist);
            }
        }
    };

    p->scanFiles(urls, handleScanResult);
}

void PlaylistInteractor::filesToActivePlaylist(const QList<QUrl>& urls) const
{
    if(!p->m_handler->activePlaylist()) {
        return;
    }

    if(urls.empty()) {
        return;
    }

    p->scanFiles(urls, [this](const TrackList& scannedTracks) {
        if(auto* playlist = p->m_handler->activePlaylist()) {
            p->m_handler->appendToPlaylist(playlist->id(), scannedTracks);
        }
    });
}

void PlaylistInteractor::filesToTracks(const QList<QUrl>& urls, const std::function<void(const TrackList&)>& func) const
{
    if(urls.empty()) {
        return;
    }

    p->scanFiles(urls, func);
}
} // namespace Fooyin
