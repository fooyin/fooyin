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

#include "guiutils.h"
#include "playlistcontroller.h"

#include <core/library/musiclibrary.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlist.h>
#include <core/playlist/playlisthandler.h>
#include <core/track.h>
#include <gui/widgets/elapsedprogressdialog.h>
#include <utils/datastream.h>
#include <utils/fileutils.h>
#include <utils/utils.h>

#include <QIODevice>
#include <QMainWindow>
#include <QPointer>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
class TrackScanController : public QObject
{
    Q_OBJECT

public:
    TrackScanController(MusicLibrary* library, const QString& labelText, ScanRequest request,
                        std::function<void(const TrackList&)> callback, QObject* parent = nullptr)
        : QObject{parent}
        , m_request{std::move(request)}
        , m_callback{std::move(callback)}
        , m_dialog{
              new ElapsedProgressDialog(labelText, PlaylistInteractor::tr("Abort"), 0, 100, Utils::getMainWindow())}
    {
        m_dialog->setAttribute(Qt::WA_DeleteOnClose);
        m_dialog->setModal(true);
        m_dialog->setMinimumDuration(500ms);
        m_dialog->startTimer();

        QObject::connect(library, &MusicLibrary::scanProgress, this, &TrackScanController::handleProgress);
        QObject::connect(library, &MusicLibrary::tracksScanned, this, &TrackScanController::handleTracksScanned);
        QObject::connect(
            library, &MusicLibrary::scanFinished, this,
            [this](int id, ScanRequest::Type /*type*/, bool cancelled) { handleScanFinished(id, cancelled); });
    }

private:
    [[nodiscard]] bool checkScan(int id) const
    {
        return !m_completed && id == m_request.id;
    }

    void handleProgress(const ScanProgress& progress)
    {
        if(!checkScan(progress.id)) {
            return;
        }

        if(!m_dialog) {
            return;
        }

        if(m_dialog->wasCancelled()) {
            m_request.cancel();
            m_dialog->close();
            m_completed = true;
            deleteLater();
            return;
        }

        const bool isIndeterminate = progress.total <= 0;
        m_dialog->setBusy(isIndeterminate);

        if(!isIndeterminate) {
            m_dialog->setValue(progress.percentage());
        }

        if(!progress.file.isEmpty()) {
            m_dialog->setText(PlaylistInteractor::tr("Current file") + ":\n"_L1 + progress.file);
        }
    }

    void handleScanFinished(int id, bool cancelled)
    {
        if(!checkScan(id)) {
            return;
        }

        m_completed = true;

        if(m_dialog) {
            m_dialog->close();
        }

        if(!cancelled) {
            m_callback(m_tracks);
        }

        deleteLater();
    }

    void handleTracksScanned(int id, const TrackList& scannedTracks)
    {
        if(!checkScan(id)) {
            return;
        }

        m_tracks.insert(m_tracks.end(), scannedTracks.cbegin(), scannedTracks.cend());
    }

    ScanRequest m_request;
    std::function<void(const TrackList&)> m_callback;
    QPointer<ElapsedProgressDialog> m_dialog;
    TrackList m_tracks;
    bool m_completed{false};
};
} // namespace

PlaylistInteractor::PlaylistInteractor(PlaylistHandler* handler, PlaylistController* controller, MusicLibrary* library,
                                       QObject* parent)
    : QObject{parent}
    , m_handler{handler}
    , m_controller{controller}
    , m_library{library}
{ }

PlaylistHandler* PlaylistInteractor::handler() const
{
    return m_handler;
}

PlaylistController* PlaylistInteractor::playlistController() const
{
    return m_controller;
}

MusicLibrary* PlaylistInteractor::library() const
{
    return m_library;
}

PlayerController* PlaylistInteractor::playerController() const
{
    return m_controller->playerController();
}

ScanRequest PlaylistInteractor::startFileScan(const QList<QUrl>& urls) const
{
    return m_library->scanFiles(urls);
}

ScanRequest PlaylistInteractor::startPlaylistLoad(const QList<QUrl>& urls) const
{
    return m_library->loadPlaylist(urls);
}

void PlaylistInteractor::beginTrackScan(const QString& labelText, const ScanRequest& request,
                                        std::function<void(const TrackList&)> func)
{
    auto* controller = new TrackScanController(m_library, labelText, request, std::move(func), this);
    controller->setObjectName(u"TrackScanController"_s);
}

void PlaylistInteractor::activatePlaylist(Playlist* playlist, const bool play) const
{
    if(!playlist) {
        return;
    }

    m_controller->changeCurrentPlaylist(playlist);
    if(play) {
        playerController()->startPlayback(playlist);
    }
}

void PlaylistInteractor::activatePlaylist(Playlist* playlist, const int indexToPlay, const bool play) const
{
    if(!playlist) {
        return;
    }

    playlist->changeCurrentIndex(indexToPlay);
    activatePlaylist(playlist, play);
}

void PlaylistInteractor::appendToPlaylist(Playlist* playlist, const TrackList& tracks) const
{
    if(!playlist || tracks.empty()) {
        return;
    }

    m_handler->appendToPlaylist(playlist->id(), tracks);
}

Playlist* PlaylistInteractor::appendOrCreateNamedPlaylist(const QString& playlistName, const TrackList& tracks) const
{
    if(tracks.empty()) {
        return nullptr;
    }

    if(Playlist* playlist = m_handler->playlistByName(playlistName)) {
        const int indexToPlay = playlist->trackCount();
        appendToPlaylist(playlist, tracks);
        playlist->changeCurrentIndex(indexToPlay);
        return playlist;
    }

    return m_handler->createPlaylist(playlistName, tracks);
}

void PlaylistInteractor::filesToPlaylist(const QList<QUrl>& urls, const UId& id)
{
    if(urls.empty()) {
        return;
    }

    if(id.isValid()) {
        scanFiles(urls, [this, id](const TrackList& scannedTracks) {
            if(auto* playlist = m_handler->playlistById(id)) {
                appendToPlaylist(playlist, scannedTracks);
                activatePlaylist(playlist);
            }
        });
    }
    else {
        scanFiles(urls, [this](const TrackList& scannedTracks) {
            const QString playlistName = Track::findCommonField(scannedTracks);
            activatePlaylist(m_handler->createNewPlaylist(playlistName, scannedTracks));
        });
    }
}

void PlaylistInteractor::filesToCurrentPlaylist(const QList<QUrl>& urls)
{
    if(urls.empty()) {
        return;
    }

    scanFiles(urls, [this](const TrackList& scannedTracks) {
        appendToPlaylist(m_controller->currentPlaylist(), scannedTracks);
    });
}

void PlaylistInteractor::filesToCurrentPlaylistReplace(const QList<QUrl>& urls, bool play)
{
    if(urls.empty()) {
        return;
    }

    scanFiles(urls, [this, play](const TrackList& scannedTracks) {
        if(auto* playlist = m_controller->currentPlaylist()) {
            if(scannedTracks.empty()) {
                return;
            }
            m_handler->replacePlaylistTracks(playlist->id(), scannedTracks);
            activatePlaylist(playlist, 0, play);
        }
    });
}

void PlaylistInteractor::filesToNewPlaylist(const QString& playlistName, const QList<QUrl>& urls, bool play)
{
    if(urls.empty()) {
        return;
    }

    auto handleScanResult = [this, playlistName, play](const TrackList& scannedTracks) {
        activatePlaylist(appendOrCreateNamedPlaylist(playlistName, scannedTracks), play);
    };

    scanFiles(urls, handleScanResult);
}

void PlaylistInteractor::filesToNewPlaylistReplace(const QString& playlistName, const QList<QUrl>& urls, bool play)
{
    if(urls.empty()) {
        return;
    }

    auto handleScanResult = [this, playlistName, play](const TrackList& scannedTracks) {
        activatePlaylist(m_handler->createPlaylist(playlistName, scannedTracks), play);
    };

    scanFiles(urls, handleScanResult);
}

void PlaylistInteractor::filesToActivePlaylist(const QList<QUrl>& urls)
{
    if(!m_handler->activePlaylist()) {
        return;
    }

    if(urls.empty()) {
        return;
    }

    scanFiles(urls,
              [this](const TrackList& scannedTracks) { appendToPlaylist(m_handler->activePlaylist(), scannedTracks); });
}

void PlaylistInteractor::loadPlaylist(const QList<QPair<QString, QUrl>>& playlistData, bool play)
{
    if(playlistData.empty()) {
        return;
    }

    for(const QPair<QString, QUrl>& item : playlistData) {
        auto [name, url]      = item;
        auto handleScanResult = [this, name, play](const TrackList& scannedTracks) {
            activatePlaylist(appendOrCreateNamedPlaylist(name, scannedTracks), play);
        };
        loadPlaylistTracks({url}, handleScanResult);
    }
}

void PlaylistInteractor::trackMimeToPlaylist(const QByteArray& data, const UId& id)
{
    const TrackList tracks = Gui::tracksFromMimeData(m_library, data);
    if(tracks.empty()) {
        return;
    }

    if(id.isValid()) {
        m_handler->appendToPlaylist(id, tracks);
    }
    else {
        const QString playlistName = Track::findCommonField(tracks);
        activatePlaylist(m_handler->createPlaylist(playlistName, tracks));
    }
}
} // namespace Fooyin

#include "moc_playlistinteractor.cpp"
#include "playlistinteractor.moc"
