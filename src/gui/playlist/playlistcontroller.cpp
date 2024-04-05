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
#include <core/player/playercontroller.h>
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
    PlayerController* playerController;
    TrackSelectionController* selectionController;
    SettingsManager* settings;

    bool loaded{false};
    Playlist* currentPlaylist{nullptr};
    bool clearingQueue{false};
    bool changingTracks{false};

    std::unordered_map<Playlist*, QUndoStack> histories;
    std::unordered_map<Playlist*, PlaylistViewState> states;

    Private(PlaylistController* self_, PlaylistHandler* handler_, PlayerController* playerController_,
            TrackSelectionController* selectionController_, SettingsManager* settings_)
        : self{self_}
        , handler{handler_}
        , playerController{playerController_}
        , selectionController{selectionController_}
        , settings{settings_}
    { }

    void restoreLastPlaylist()
    {
        const int lastId = settings->value<Settings::Gui::LastPlaylistId>();

        if(lastId >= 0) {
            currentPlaylist = handler->playlistByDbId(lastId);
            if(!currentPlaylist) {
                currentPlaylist = handler->playlistByIndex(0);
            }
        }

        loaded = true;
        emit self->playlistsLoaded();
    }

    void handlePlaylistAdded(Playlist* playlist)
    {
        if(playlist) {
            histories.erase(playlist);
            states.erase(playlist);
        }
    }

    void handlePlaylistTracksAdded(Playlist* playlist, const TrackList& tracks, int index) const
    {
        if(playlist == currentPlaylist) {
            emit self->currentPlaylistTracksAdded(tracks, index);
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
            emit self->currentPlaylistQueueChanged(indexes);
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

        const auto queuedTracks = playerController->playbackQueue().indexesForPlaylist(currentPlaylist->id());
        for(const auto& trackIndex : queuedTracks | std::views::keys) {
            uniqueIndexes.emplace(trackIndex);
        }

        const std::vector<int> indexes{uniqueIndexes.cbegin(), uniqueIndexes.cend()};

        if(!indexes.empty()) {
            emit self->currentPlaylistQueueChanged(indexes);
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
            emit self->currentPlaylistQueueChanged(indexes);
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
            histories.erase(playlist);
            states.erase(playlist);

            clearingQueue = true;
            playerController->clearPlaylistQueue(playlist->id());
            clearingQueue = false;
        }

        if(playlist == currentPlaylist) {
            emit self->currentPlaylistTracksChanged(indexes, allNew);
        }
    }

    void handlePlaylistRemoved(Playlist* playlist)
    {
        if(!playlist) {
            return;
        }

        histories.erase(playlist);
        states.erase(playlist);

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
            emit self->playingTrackChanged(track);
        }
    }

    void saveStates() const
    {
        QByteArray out;
        QDataStream stream(&out, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_0);

        stream << static_cast<qint32>(states.size());
        for(const auto& [playlist, state] : states) {
            if(playlist) {
                stream << playlist->dbId();
                stream << state.topIndex;
                stream << state.scrollPos;
            }
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
            int dbId;
            stream >> dbId;

            PlaylistViewState value;
            stream >> value.topIndex;
            stream >> value.scrollPos;

            if(auto* playlist = handler->playlistByDbId(dbId)) {
                states[playlist] = value;
            }
        }
    }
};

PlaylistController::PlaylistController(PlaylistHandler* handler, PlayerController* playerController,
                                       TrackSelectionController* selectionController, SettingsManager* settings,
                                       QObject* parent)
    : QObject{parent}
    , p{std::make_unique<Private>(this, handler, playerController, selectionController, settings)}
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
                     [this](Playlist* playlist) { p->handlePlaylistRemoved(playlist); });

    QObject::connect(playerController, &PlayerController::playlistTrackChanged, this,
                     [this](const PlaylistTrack& track) { p->handlePlayingTrackChanged(track); });
    QObject::connect(playerController, &PlayerController::tracksQueued, this,
                     [this](const QueueTracks& tracks) { p->handleTracksQueued(tracks); });
    QObject::connect(
        playerController, &PlayerController::trackQueueChanged, this,
        [this](const QueueTracks& removed, const QueueTracks& added) { p->handleQueueChanged(removed, added); });
    QObject::connect(playerController, &PlayerController::tracksDequeued, this,
                     [this](const QueueTracks& tracks) { p->handleTracksDequeued(tracks); });
    QObject::connect(playerController, &PlayerController::playStateChanged, this,
                     &PlaylistController::playStateChanged);
}

PlaylistController::~PlaylistController()
{
    if(p->currentPlaylist) {
        p->settings->set<Settings::Gui::LastPlaylistId>(p->currentPlaylist->dbId());
        p->saveStates();
    }
}

PlayerController* PlaylistController::playerController() const
{
    return p->playerController;
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
    return p->playerController->currentPlaylistTrack();
}

PlayState PlaylistController::playState() const
{
    return p->playerController->playState();
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
            const Id id    = playlist->id();
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

Id PlaylistController::currentPlaylistId() const
{
    return p->currentPlaylist ? p->currentPlaylist->id() : Id{};
}

void PlaylistController::changeCurrentPlaylist(Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    auto* prevPlaylist = std::exchange(p->currentPlaylist, playlist);

    if(prevPlaylist == playlist) {
        p->histories.erase(playlist);
        p->states.erase(playlist);

        emit playlistHistoryChanged();
        return;
    }

    emit currentPlaylistChanged(prevPlaylist, playlist);
}

void PlaylistController::changeCurrentPlaylist(const Id& id)
{
    if(auto* playlist = p->handler->playlistById(id)) {
        changeCurrentPlaylist(playlist);
    }
}

void PlaylistController::changePlaylistIndex(const Id& playlistId, int index)
{
    p->handler->changePlaylistIndex(playlistId, index);
}

void PlaylistController::clearCurrentPlaylist()
{
    if(p->currentPlaylist) {
        p->handler->clearPlaylistTracks(p->currentPlaylist->id());
    }
}

std::optional<PlaylistViewState> PlaylistController::playlistState(Playlist* playlist) const
{
    if(!playlist || !p->states.contains(playlist)) {
        return {};
    }

    return p->states.at(playlist);
}

void PlaylistController::savePlaylistState(Playlist* playlist, const PlaylistViewState& state)
{
    if(!playlist) {
        return;
    }

    if(state.scrollPos == 0 || state.topIndex < 0) {
        p->states.erase(playlist);
    }
    else {
        p->states[playlist] = state;
    }
}

void PlaylistController::addToHistory(QUndoCommand* command)
{
    if(!p->currentPlaylist) {
        return;
    }

    p->histories[p->currentPlaylist].push(command);

    emit playlistHistoryChanged();
}

bool PlaylistController::canUndo() const
{
    if(!p->currentPlaylist || p->histories.empty()) {
        return false;
    }

    if(!p->histories.contains(p->currentPlaylist)) {
        return false;
    }

    return p->histories.at(p->currentPlaylist).canUndo();
}

bool PlaylistController::canRedo() const
{
    if(!p->currentPlaylist || p->histories.empty()) {
        return false;
    }

    if(!p->histories.contains(p->currentPlaylist)) {
        return false;
    }

    return p->histories.at(p->currentPlaylist).canRedo();
}

void PlaylistController::undoPlaylistChanges()
{
    if(!p->currentPlaylist) {
        return;
    }

    if(canUndo()) {
        p->histories.at(p->currentPlaylist).undo();
        emit playlistHistoryChanged();
    }
}

void PlaylistController::redoPlaylistChanges()
{
    if(!p->currentPlaylist) {
        return;
    }

    if(canRedo()) {
        p->histories.at(p->currentPlaylist).redo();
        emit playlistHistoryChanged();
    }
}

void PlaylistController::handleTrackSelectionAction(TrackAction action)
{
    if(action == TrackAction::SendCurrentPlaylist) {
        if(p->currentPlaylist) {
            p->histories.erase(p->currentPlaylist);
            p->states.erase(p->currentPlaylist);
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
