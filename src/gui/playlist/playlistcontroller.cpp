/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "playlistcontroller.h"

#include <core/library/musiclibrary.h>
#include <core/player/playermanager.h>
#include <core/playlist/playlistmanager.h>
#include <core/track.h>
#include <gui/guisettings.h>
#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>

#include <QIODevice>
#include <QProgressDialog>
#include <QUndoStack>

constexpr auto PlaylistStates = "PlaylistWidget/PlaylistStates";

namespace Fooyin {
struct PlaylistController::Private
{
    PlaylistController* self;

    PlaylistManager* handler;
    PlayerManager* playerManager;
    MusicLibrary* library;
    TrackSelectionController* selectionController;
    SettingsManager* settings;

    bool loaded{false};
    Playlist* currentPlaylist{nullptr};

    std::unordered_map<int, QUndoStack> histories;
    std::unordered_map<int, PlaylistViewState> states;

    Private(PlaylistController* self_, PlaylistManager* handler_, PlayerManager* playerManager_, MusicLibrary* library_,
            TrackSelectionController* selectionController_, SettingsManager* settings_)
        : self{self_}
        , handler{handler_}
        , playerManager{playerManager_}
        , library{library_}
        , selectionController{selectionController_}
        , settings{settings_}
    { }

    void restoreLastPlaylist()
    {
        const int lastId = settings->value<Settings::Gui::LastPlaylistId>();
        if(lastId >= 0) {
            Playlist* playlist = handler->playlistById(lastId);
            if(!playlist) {
                playlist = handler->playlistByIndex(0);
            }
            if(playlist) {
                currentPlaylist = playlist;
                QMetaObject::invokeMethod(self, "currentPlaylistChanged", Q_ARG(Playlist*, playlist));
            }
        }
        loaded = true;
        QMetaObject::invokeMethod(self, &PlaylistController::playlistsLoaded);
    }

    void handlePlaylistUpdated(Playlist* playlist)
    {
        if(playlist) {
            histories.erase(playlist->id());
            states.erase(playlist->id());
        }
    }

    void handlePlaylistRemoved(const Playlist* playlist)
    {
        if(!playlist) {
            return;
        }

        histories.erase(playlist->id());
        states.erase(playlist->id());

        if(currentPlaylist != playlist) {
            return;
        }

        if(handler->playlistCount() == 0) {
            QObject::connect(
                handler, &PlaylistManager::playlistAdded, self,
                [this](Playlist* newPlaylist) {
                    if(newPlaylist) {
                        self->changeCurrentPlaylist(newPlaylist);
                    }
                },
                Qt::SingleShotConnection);
        }
        else {
            const int nextIndex = std::max(0, playlist->index() - 1);
            if(auto* nextPlaylist = handler->playlistByIndex(nextIndex)) {
                self->changeCurrentPlaylist(nextPlaylist);
            }
        }
    }

    template <typename Func>
    void scanTracks(const TrackList& tracks, Func&& func) const
    {
        auto* scanDialog = new QProgressDialog("Reading tracks...", "Abort", 0, 100, nullptr);
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

    void saveStates() const
    {
        QByteArray out;
        QDataStream stream(&out, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_4);

        stream << static_cast<qint32>(states.size());
        for(const auto& [playlistId, state] : states) {
            stream << playlistId;
            stream << state.topIndex;
            stream << state.scrollPos;
        }

        out = qCompress(out, 9);

        settings->fileSet(PlaylistStates, out);
    }

    void restoreStates()
    {
        QByteArray in = settings->fileValue(PlaylistStates).toByteArray();

        if(in.isEmpty()) {
            return;
        }

        in = qUncompress(in);

        QDataStream stream(&in, QIODevice::ReadOnly);
        stream.setVersion(QDataStream::Qt_6_4);

        qint32 size;
        stream >> size;
        states.clear();

        for(qint32 i{0}; i < size; ++i) {
            int playlistId;
            stream >> playlistId;
            PlaylistViewState value;
            stream >> value.topIndex;
            stream >> value.scrollPos;
            states[playlistId] = value;
        }
    }
};

PlaylistController::PlaylistController(PlaylistManager* handler, PlayerManager* playerManager, MusicLibrary* library,
                                       TrackSelectionController* selectionController, SettingsManager* settings,
                                       QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, handler, playerManager, library, selectionController, settings)}
{
    p->restoreStates();

    QObject::connect(handler, &PlaylistManager::playlistsPopulated, this, [this]() { p->restoreLastPlaylist(); });
    QObject::connect(handler, &PlaylistManager::playlistTracksChanged, this,
                     [this](Playlist* playlist) { p->handlePlaylistUpdated(playlist); });
    QObject::connect(handler, &PlaylistManager::playlistRemoved, this,
                     [this](const Playlist* playlist) { p->handlePlaylistRemoved(playlist); });
    QObject::connect(handler, &PlaylistManager::playlistRenamed, this,
                     [this](Playlist* playlist) { p->handlePlaylistUpdated(playlist); });

    QObject::connect(playerManager, &PlayerManager::currentTrackChanged, this,
                     &PlaylistController::currentTrackChanged);
    QObject::connect(playerManager, &PlayerManager::playStateChanged, this, &PlaylistController::playStateChanged);
}

PlaylistController::~PlaylistController()
{
    if(p->currentPlaylist) {
        p->settings->set<Settings::Gui::LastPlaylistId>(p->currentPlaylist->id());
        p->saveStates();
    }
}

PlaylistManager* PlaylistController::playlistHandler() const
{
    return p->handler;
}

TrackSelectionController* PlaylistController::selectionController() const
{
    return p->selectionController;
}

bool PlaylistController::playlistsHaveLoaded() const
{
    return p->loaded;
}

PlaylistList PlaylistController::playlists() const
{
    return p->handler->playlists();
}

Track PlaylistController::currentTrack() const
{
    return p->playerManager->currentTrack();
}

PlayState PlaylistController::playState() const
{
    return p->playerManager->playState();
}

Playlist* PlaylistController::currentPlaylist() const
{
    return p->currentPlaylist;
}

void PlaylistController::changeCurrentPlaylist(Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    if(std::exchange(p->currentPlaylist, playlist) == playlist) {
        p->histories.erase(playlist->id());
        p->states.erase(playlist->id());
        emit playlistHistoryChanged();
        return;
    }

    emit currentPlaylistChanged(playlist);
}

void PlaylistController::changeCurrentPlaylist(int id)
{
    if(auto* playlist = p->handler->playlistById(id)) {
        changeCurrentPlaylist(playlist);
    }
}

void PlaylistController::changePlaylistIndex(int playlistId, int index)
{
    p->handler->changePlaylistIndex(playlistId, index);
}

std::optional<PlaylistViewState> PlaylistController::playlistState(int playlistId) const
{
    if(!p->states.contains(playlistId)) {
        return {};
    }
    return p->states.at(playlistId);
}

void PlaylistController::savePlaylistState(int playlistId, const PlaylistViewState& state)
{
    if(state.scrollPos == 0 || state.topIndex < 0) {
        p->states.erase(playlistId);
    }
    else {
        p->states[playlistId] = state;
    }
}

void PlaylistController::addToHistory(QUndoCommand* command)
{
    if(!p->currentPlaylist) {
        return;
    }

    p->histories[p->currentPlaylist->id()].push(command);

    emit playlistHistoryChanged();
}

bool PlaylistController::canUndo() const
{
    if(!p->currentPlaylist || p->histories.empty()) {
        return false;
    }

    const int currentId = p->currentPlaylist->id();

    if(!p->histories.contains(currentId)) {
        return false;
    }

    return p->histories.at(currentId).canUndo();
}

bool PlaylistController::canRedo() const
{
    if(!p->currentPlaylist || p->histories.empty()) {
        return false;
    }

    const int currentId = p->currentPlaylist->id();

    if(!p->histories.contains(currentId)) {
        return false;
    }

    return p->histories.at(currentId).canRedo();
}

void PlaylistController::undoPlaylistChanges()
{
    if(!p->currentPlaylist) {
        return;
    }

    if(canUndo()) {
        const int currentId = p->currentPlaylist->id();
        p->histories.at(currentId).undo();
        emit playlistHistoryChanged();
    }
}

void PlaylistController::redoPlaylistChanges()
{
    if(!p->currentPlaylist) {
        return;
    }

    if(canRedo()) {
        const int currentId = p->currentPlaylist->id();
        p->histories.at(currentId).redo();
        emit playlistHistoryChanged();
    }
}

void PlaylistController::filesToCurrentPlaylist(const QList<QUrl>& urls)
{
    const QStringList filepaths = Utils::File::getFiles(urls, Track::supportedFileExtensions());
    if(filepaths.empty()) {
        return;
    }

    TrackList tracks;
    std::ranges::transform(filepaths, std::back_inserter(tracks), [](const QString& path) { return Track{path}; });

    p->scanTracks(tracks, [this](const TrackList& scannedTracks) {
        if(p->currentPlaylist) {
            p->handler->appendToPlaylist(p->currentPlaylist->id(), scannedTracks);
        }
    });
}

void PlaylistController::filesToNewPlaylist(const QString& playlistName, const QList<QUrl>& urls)
{
    const QStringList filepaths = Utils::File::getFiles(urls, Track::supportedFileExtensions());
    if(filepaths.empty()) {
        return;
    }

    TrackList tracks;
    std::ranges::transform(filepaths, std::back_inserter(tracks), [](const QString& path) { return Track{path}; });

    auto handleScanResult = [this, playlistName](const TrackList& scannedTracks) {
        Playlist* playlist = p->handler->playlistByName(playlistName);
        if(playlist) {
            const int indexToPlay = playlist->trackCount();
            p->handler->appendToPlaylist(playlist->id(), scannedTracks);
            playlist->changeCurrentTrack(indexToPlay);
        }
        else {
            playlist = p->handler->createPlaylist(playlistName, scannedTracks);
        }

        if(playlist) {
            changeCurrentPlaylist(playlist);
            p->handler->startPlayback(playlist->id());
        }
    };

    p->scanTracks(tracks, handleScanResult);
}

void PlaylistController::filesToTracks(const QList<QUrl>& urls, std::function<void(const TrackList&)> func)
{
    const QStringList filepaths = Utils::File::getFiles(urls, Track::supportedFileExtensions());
    if(filepaths.empty()) {
        return;
    }

    TrackList tracks;
    std::ranges::transform(filepaths, std::back_inserter(tracks), [](const QString& path) { return Track{path}; });

    p->scanTracks(tracks, func);
}

void PlaylistController::startPlayback() const
{
    if(p->currentPlaylist) {
        p->handler->startPlayback(p->currentPlaylist->id());
    }
}

bool PlaylistController::currentIsActive() const
{
    return p->currentPlaylist == p->handler->activePlaylist();
}
} // namespace Fooyin

#include "moc_playlistcontroller.cpp"
