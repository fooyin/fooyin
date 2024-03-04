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
#include <core/playlist/playlisthandler.h>
#include <core/track.h>
#include <gui/guisettings.h>
#include <gui/trackselectioncontroller.h>
#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>

#include <QIODevice>
#include <QMenu>
#include <QProgressDialog>
#include <QUndoStack>

namespace Fooyin {
struct PlaylistController::Private
{
    PlaylistController* self;

    PlaylistHandler* handler;
    PlayerManager* playerManager;
    MusicLibrary* library;
    TrackSelectionController* selectionController;
    SettingsManager* settings;

    bool loaded{false};
    Playlist* currentPlaylist{nullptr};
    bool clearingQueue{false};
    bool changingTracks{false};

    std::unordered_map<int, QUndoStack> histories;
    std::unordered_map<int, PlaylistViewState> states;

    Private(PlaylistController* self_, PlaylistHandler* handler_, PlayerManager* playerManager_, MusicLibrary* library_,
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
                QMetaObject::invokeMethod(self, "currentPlaylistChanged", Q_ARG(Playlist*, nullptr),
                                          Q_ARG(Playlist*, playlist));
            }
        }

        loaded = true;
        QMetaObject::invokeMethod(self, &PlaylistController::playlistsLoaded);
    }

    void handlePlaylistAdded(Playlist* playlist)
    {
        if(playlist) {
            histories.erase(playlist->id());
            states.erase(playlist->id());
        }
    }

    void handlePlaylistTracksAdded(Playlist* playlist, const TrackList& tracks, int index) const
    {
        if(playlist == currentPlaylist) {
            QMetaObject::invokeMethod(self, "currentPlaylistTracksAdded", Q_ARG(const TrackList&, tracks),
                                      Q_ARG(int, index));
        }
    }

    void handleTracksQueued(const QueueTracks& tracks) const
    {
        std::set<int> uniqueIndexes;

        for(const auto& track : tracks) {
            if(track.playlistId == currentPlaylist->id()) {
                uniqueIndexes.emplace(track.indexInPlaylist);
            }
        }

        const std::vector<int> indexes{uniqueIndexes.cbegin(), uniqueIndexes.cend()};

        if(!indexes.empty()) {
            QMetaObject::invokeMethod(self, "currentPlaylistQueueChanged", Q_ARG(const std::vector<int>&, indexes));
        }
    }

    void handleTracksDequeued(const QueueTracks& tracks) const
    {
        if(clearingQueue) {
            return;
        }

        std::set<int> uniqueIndexes;

        for(const auto& track : tracks) {
            if(track.playlistId == currentPlaylist->id()) {
                uniqueIndexes.emplace(track.indexInPlaylist);
            }
        }

        const auto queuedTracks = playerManager->playbackQueue().indexesForPlaylist(currentPlaylist->id());
        for(const auto& trackIndex : queuedTracks | std::views::keys) {
            uniqueIndexes.emplace(trackIndex);
        }

        const std::vector<int> indexes{uniqueIndexes.cbegin(), uniqueIndexes.cend()};

        if(!indexes.empty()) {
            QMetaObject::invokeMethod(self, "currentPlaylistQueueChanged", Q_ARG(const std::vector<int>&, indexes));
        }
    }

    void handleQueueChanged(const QueueTracks& removed, const QueueTracks& added)
    {
        std::set<int> uniqueIndexes;

        auto gatherIndexes = [this, &uniqueIndexes](const QueueTracks& tracks) {
            for(const auto& track : tracks) {
                if(track.playlistId == currentPlaylist->id()) {
                    uniqueIndexes.emplace(track.indexInPlaylist);
                }
            }
        };

        gatherIndexes(removed);
        gatherIndexes(added);

        const std::vector<int> indexes{uniqueIndexes.cbegin(), uniqueIndexes.cend()};

        if(!indexes.empty()) {
            QMetaObject::invokeMethod(self, "currentPlaylistQueueChanged", Q_ARG(const std::vector<int>&, indexes));
        }
    }

    void handlePlaylistUpdated(Playlist* playlist, const std::vector<int>& indexes)
    {
        if(changingTracks) {
            return;
        }

        bool allNew{false};

        if(playlist && std::cmp_equal(indexes.size(), playlist->trackCount())) {
            allNew = true;
            histories.erase(playlist->id());
            states.erase(playlist->id());

            clearingQueue = true;
            playerManager->clearPlaylistQueue(playlist->id());
            clearingQueue = false;
        }

        if(playlist == currentPlaylist) {
            QMetaObject::invokeMethod(self, "currentPlaylistTracksChanged", Q_ARG(const std::vector<int>&, indexes),
                                      Q_ARG(bool, allNew));
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
                handler, &PlaylistHandler::playlistAdded, self,
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

    void handlePlayingTrackChanged(const PlaylistTrack& track) const
    {
        if(currentPlaylist && currentPlaylist->id() == track.playlistId) {
            QMetaObject::invokeMethod(self, "playingTrackChanged", Q_ARG(const PlaylistTrack&, track));
        }
    }

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

    void saveStates() const
    {
        QByteArray out;
        QDataStream stream(&out, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_0);

        stream << static_cast<qint32>(states.size());
        for(const auto& [playlistId, state] : states) {
            stream << playlistId;
            stream << state.topIndex;
            stream << state.scrollPos;
        }

        out = qCompress(out, 9);

        settings->fileSet(QStringLiteral("PlaylistWidget/PlaylistStates"), out);
    }

    void restoreStates()
    {
        QByteArray in = settings->fileValue(QStringLiteral("PlaylistWidget/PlaylistStates")).toByteArray();

        if(in.isEmpty()) {
            return;
        }

        in = qUncompress(in);

        QDataStream stream(&in, QIODevice::ReadOnly);
        stream.setVersion(QDataStream::Qt_6_0);

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

PlaylistController::PlaylistController(PlaylistHandler* handler, PlayerManager* playerManager, MusicLibrary* library,
                                       TrackSelectionController* selectionController, SettingsManager* settings,
                                       QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, handler, playerManager, library, selectionController, settings)}
{
    p->restoreStates();

    QObject::connect(handler, &PlaylistHandler::playlistsPopulated, this, [this]() { p->restoreLastPlaylist(); });
    QObject::connect(handler, &PlaylistHandler::playlistAdded, this,
                     [this](Playlist* playlist) { p->handlePlaylistAdded(playlist); });
    QObject::connect(
        handler, &PlaylistHandler::playlistTracksChanged, this,
        [this](Playlist* playlist, const std::vector<int>& indexes) { p->handlePlaylistUpdated(playlist, indexes); });
    QObject::connect(handler, &PlaylistHandler::playlistTracksAdded, this,
                     [this](Playlist* playlist, const TrackList& tracks, int index) {
                         p->handlePlaylistTracksAdded(playlist, tracks, index);
                     });
    QObject::connect(handler, &PlaylistHandler::playlistRemoved, this,
                     [this](const Playlist* playlist) { p->handlePlaylistRemoved(playlist); });

    QObject::connect(playerManager, &PlayerManager::playlistTrackChanged, this,
                     [this](const PlaylistTrack& track) { p->handlePlayingTrackChanged(track); });
    QObject::connect(playerManager, &PlayerManager::tracksQueued, this,
                     [this](const QueueTracks& tracks) { p->handleTracksQueued(tracks); });
    QObject::connect(
        playerManager, &PlayerManager::trackQueueChanged, this,
        [this](const QueueTracks& removed, const QueueTracks& added) { p->handleQueueChanged(removed, added); });
    QObject::connect(playerManager, &PlayerManager::tracksDequeued, this,
                     [this](const QueueTracks& tracks) { p->handleTracksDequeued(tracks); });
    QObject::connect(playerManager, &PlayerManager::playStateChanged, this, &PlaylistController::playStateChanged);
}

PlaylistController::~PlaylistController()
{
    if(p->currentPlaylist) {
        p->settings->set<Settings::Gui::LastPlaylistId>(p->currentPlaylist->id());
        p->saveStates();
    }
}

PlayerManager* PlaylistController::playerManager() const
{
    return p->playerManager;
}

PlaylistHandler* PlaylistController::playlistHandler() const
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

PlaylistTrack PlaylistController::currentTrack() const
{
    return p->playerManager->currentPlaylistTrack();
}

PlayState PlaylistController::playState() const
{
    return p->playerManager->playState();
}

void PlaylistController::aboutToChangeTracks()
{
    p->changingTracks = true;
}

void PlaylistController::changedTracks()
{
    p->changingTracks = false;
}

void PlaylistController::addPlaylistMenu(QMenu* menu)
{
    if(!p->currentPlaylist) {
        return;
    }

    auto* playlistMenu = new QMenu(tr("Playlists"), menu);

    const auto playlists = this->playlists();

    for(const auto& playlist : playlists) {
        if(playlist != p->currentPlaylist) {
            auto* switchPl = new QAction(playlist->name(), playlistMenu);
            const int id   = playlist->id();
            QObject::connect(switchPl, &QAction::triggered, playlistMenu, [this, id]() { changeCurrentPlaylist(id); });
            playlistMenu->addAction(switchPl);
        }
    }

    menu->addMenu(playlistMenu);
}

Playlist* PlaylistController::currentPlaylist() const
{
    return p->currentPlaylist;
}

int PlaylistController::currentPlaylistId() const
{
    return p->currentPlaylist ? p->currentPlaylist->id() : -1;
}

void PlaylistController::changeCurrentPlaylist(Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    auto* prevPlaylist = std::exchange(p->currentPlaylist, playlist);

    if(prevPlaylist == playlist) {
        p->histories.erase(playlist->id());
        p->states.erase(playlist->id());

        emit playlistHistoryChanged();
        return;
    }

    emit currentPlaylistChanged(prevPlaylist, playlist);
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

void PlaylistController::clearCurrentPlaylist()
{
    if(p->currentPlaylist) {
        p->handler->clearPlaylistTracks(p->currentPlaylist->id());
    }
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

void PlaylistController::filesToTracks(const QList<QUrl>& urls, const std::function<void(const TrackList&)>& func)
{
    const QStringList filepaths = Utils::File::getFiles(urls, Track::supportedFileExtensions());
    if(filepaths.empty()) {
        return;
    }

    TrackList tracks;
    std::ranges::transform(filepaths, std::back_inserter(tracks), [](const QString& path) { return Track{path}; });

    p->scanTracks(tracks, func);
}

void PlaylistController::handleTrackSelectionAction(TrackAction action)
{
    if(action == TrackAction::SendCurrentPlaylist) {
        if(p->currentPlaylist) {
            p->histories.erase(p->currentPlaylist->id());
            p->states.erase(p->currentPlaylist->id());
        }
    }
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
