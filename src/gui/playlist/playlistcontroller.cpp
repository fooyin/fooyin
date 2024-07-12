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

#include "playlistcolumnregistry.h"
#include "presetregistry.h"

#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <core/track.h>
#include <gui/guisettings.h>
#include <gui/trackselectioncontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QIODevice>
#include <QMenu>
#include <QUndoStack>

namespace Fooyin {
struct PlaylistController::Private
{
    PlaylistController* m_self;

    PlaylistHandler* m_handler;
    PlayerController* m_playerController;
    TrackSelectionController* m_selectionController;
    PresetRegistry* m_presetRegistry;
    PlaylistColumnRegistry* m_columnRegistry;
    SettingsManager* m_settings;

    bool m_loaded{false};
    Playlist* m_currentPlaylist{nullptr};
    bool m_changingTracks{false};

    std::unordered_map<Playlist*, QUndoStack> m_histories;
    std::unordered_map<Playlist*, PlaylistViewState> m_states;
    TrackList m_clipboard;

    Private(PlaylistController* self, PlaylistHandler* handler, PlayerController* playerController,
            TrackSelectionController* selectionController, SettingsManager* settings)
        : m_self{self}
        , m_handler{handler}
        , m_playerController{playerController}
        , m_selectionController{selectionController}
        , m_presetRegistry{new PresetRegistry(settings, m_self)}
        , m_columnRegistry{new PlaylistColumnRegistry(settings, m_self)}
        , m_settings{settings}
    { }

    void restoreLastPlaylist()
    {
        const int lastId = m_settings->value<Settings::Gui::LastPlaylistId>();

        if(lastId >= 0) {
            m_currentPlaylist = m_handler->playlistByDbId(lastId);
            if(!m_currentPlaylist) {
                m_currentPlaylist = m_handler->playlistByIndex(0);
            }
        }

        m_loaded = true;
        emit m_self->playlistsLoaded();
    }

    void handlePlaylistAdded(Playlist* playlist)
    {
        if(playlist) {
            m_histories.erase(playlist);
            m_states.erase(playlist);
        }
    }

    void handlePlaylistTracksAdded(Playlist* playlist, const TrackList& tracks, int index) const
    {
        if(playlist == m_currentPlaylist) {
            emit m_self->currentPlaylistTracksAdded(tracks, index);
        }
    }

    void handleTracksQueued(const QueueTracks& tracks) const
    {
        if(!m_currentPlaylist) {
            return;
        }

        std::set<int> uniqueIndexes;

        for(const auto& track : tracks) {
            if(track.playlistId == m_currentPlaylist->id()) {
                uniqueIndexes.emplace(track.indexInPlaylist);
            }
        }

        const std::vector<int> indexes{uniqueIndexes.cbegin(), uniqueIndexes.cend()};

        if(!indexes.empty()) {
            emit m_self->currentPlaylistQueueChanged(indexes);
        }
    }

    void handleTracksDequeued(const QueueTracks& tracks) const
    {
        if(!m_currentPlaylist) {
            return;
        }

        std::set<int> uniqueIndexes;

        for(const auto& track : tracks) {
            if(track.playlistId == m_currentPlaylist->id()) {
                uniqueIndexes.emplace(track.indexInPlaylist);
            }
        }

        const auto queuedTracks = m_playerController->playbackQueue().indexesForPlaylist(m_currentPlaylist->id());
        for(const auto& trackIndex : queuedTracks | std::views::keys) {
            uniqueIndexes.emplace(trackIndex);
        }

        const std::vector<int> indexes{uniqueIndexes.cbegin(), uniqueIndexes.cend()};

        if(!indexes.empty()) {
            emit m_self->currentPlaylistQueueChanged(indexes);
        }
    }

    void handleTracksDequeued(const PlaylistIndexes& indexes) const
    {
        if(!m_currentPlaylist) {
            return;
        }

        std::set<int> uniqueIndexes;

        if(indexes.contains(m_currentPlaylist->id())) {
            const auto playlistIndexes = indexes.at(m_currentPlaylist->id());
            for(const auto& trackIndex : playlistIndexes) {
                uniqueIndexes.emplace(trackIndex);
            }
        }

        const auto queuedTracks = m_playerController->playbackQueue().indexesForPlaylist(m_currentPlaylist->id());
        for(const auto& trackIndex : queuedTracks | std::views::keys) {
            uniqueIndexes.emplace(trackIndex);
        }

        const std::vector<int> playlistIndexes{uniqueIndexes.cbegin(), uniqueIndexes.cend()};

        if(!indexes.empty()) {
            emit m_self->currentPlaylistQueueChanged(playlistIndexes);
        }
    }

    void handleQueueChanged(const QueueTracks& removed, const QueueTracks& added)
    {
        if(!m_currentPlaylist) {
            return;
        }

        std::set<int> uniqueIndexes;

        auto gatherIndexes = [this, &uniqueIndexes](const QueueTracks& tracks) {
            for(const auto& track : tracks) {
                if(track.playlistId == m_currentPlaylist->id()) {
                    uniqueIndexes.emplace(track.indexInPlaylist);
                }
            }
        };

        gatherIndexes(removed);
        gatherIndexes(added);

        const std::vector<int> indexes{uniqueIndexes.cbegin(), uniqueIndexes.cend()};

        if(!indexes.empty()) {
            emit m_self->currentPlaylistQueueChanged(indexes);
        }
    }

    void handlePlaylistUpdated(Playlist* playlist, const std::vector<int>& indexes)
    {
        if(m_changingTracks) {
            return;
        }

        bool allNew{false};

        if(playlist && std::cmp_equal(indexes.size(), playlist->trackCount())) {
            allNew = true;
            m_histories.erase(playlist);
            m_states.erase(playlist);

            auto queueTracks = m_playerController->playbackQueue().tracks();
            for(auto& track : queueTracks) {
                if(track.playlistId == playlist->id()) {
                    track.playlistId      = {};
                    track.indexInPlaylist = -1;
                }
            }
            m_playerController->replaceTracks(queueTracks);
        }

        if(playlist == m_currentPlaylist) {
            emit m_self->currentPlaylistTracksChanged(indexes, allNew);
        }
    }

    void handleTracksPlayed(Playlist* playlist, const std::vector<int>& indexes) const
    {
        if(m_changingTracks) {
            return;
        }

        if(playlist == m_currentPlaylist) {
            emit m_self->currentPlaylistTracksPlayed(indexes);
        }
    }

    void handlePlaylistRemoved(Playlist* playlist)
    {
        if(!playlist) {
            return;
        }

        m_histories.erase(playlist);
        m_states.erase(playlist);

        if(m_currentPlaylist != playlist) {
            return;
        }

        if(m_handler->playlistCount() == 0) {
            QObject::connect(
                m_handler, &PlaylistHandler::playlistAdded, m_self,
                [this](Playlist* newPlaylist) {
                    if(newPlaylist) {
                        m_self->changeCurrentPlaylist(newPlaylist);
                    }
                },
                Qt::SingleShotConnection);
        }
        else {
            const int nextIndex = std::max(0, playlist->index() - 1);
            if(auto* nextPlaylist = m_handler->playlistByIndex(nextIndex)) {
                m_self->changeCurrentPlaylist(nextPlaylist);
            }
        }
    }

    void saveStates() const
    {
        QByteArray out;
        QDataStream stream(&out, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_0);

        stream << static_cast<qint32>(m_states.size());
        for(const auto& [playlist, state] : m_states) {
            if(playlist) {
                stream << playlist->dbId();
                stream << state.topIndex;
                stream << state.scrollPos;
            }
        }

        out = qCompress(out, 9);

        m_settings->fileSet(QStringLiteral("PlaylistWidget/PlaylistStates"), out);
    }

    void restoreStates()
    {
        QByteArray in = m_settings->fileValue(QStringLiteral("PlaylistWidget/PlaylistStates")).toByteArray();

        if(in.isEmpty()) {
            return;
        }

        in = qUncompress(in);

        QDataStream stream(&in, QIODevice::ReadOnly);
        stream.setVersion(QDataStream::Qt_6_0);

        qint32 size;
        stream >> size;
        m_states.clear();

        for(qint32 i{0}; i < size; ++i) {
            int dbId;
            stream >> dbId;

            PlaylistViewState value;
            stream >> value.topIndex;
            stream >> value.scrollPos;

            if(auto* playlist = m_handler->playlistByDbId(dbId)) {
                m_states[playlist] = value;
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
    QObject::connect(
        handler, &PlaylistHandler::playlistTracksPlayed, this,
        [this](Playlist* playlist, const std::vector<int>& indexes) { p->handleTracksPlayed(playlist, indexes); });
    QObject::connect(handler, &PlaylistHandler::playlistTracksAdded, this,
                     [this](Playlist* playlist, const TrackList& tracks, int index) {
                         p->handlePlaylistTracksAdded(playlist, tracks, index);
                     });
    QObject::connect(handler, &PlaylistHandler::playlistRemoved, this,
                     [this](Playlist* playlist) { p->handlePlaylistRemoved(playlist); });

    QObject::connect(playerController, &PlayerController::playlistTrackChanged, this,
                     &PlaylistController::playingTrackChanged);
    QObject::connect(playerController, &PlayerController::tracksQueued, this,
                     [this](const QueueTracks& tracks) { p->handleTracksQueued(tracks); });
    QObject::connect(
        playerController, &PlayerController::trackQueueChanged, this,
        [this](const QueueTracks& removed, const QueueTracks& added) { p->handleQueueChanged(removed, added); });
    QObject::connect(playerController, &PlayerController::tracksDequeued, this,
                     [this](const QueueTracks& tracks) { p->handleTracksDequeued(tracks); });
    QObject::connect(playerController, &PlayerController::trackIndexesDequeued, this,
                     [this](const PlaylistIndexes& indexes) { p->handleTracksDequeued(indexes); });
    QObject::connect(playerController, &PlayerController::playStateChanged, this,
                     &PlaylistController::playStateChanged);
}

PlaylistController::~PlaylistController()
{
    if(p->m_currentPlaylist) {
        p->m_settings->set<Settings::Gui::LastPlaylistId>(p->m_currentPlaylist->dbId());
        p->saveStates();
    }
}

PlayerController* PlaylistController::playerController() const
{
    return p->m_playerController;
}

PlaylistHandler* PlaylistController::playlistHandler() const
{
    return p->m_handler;
}

TrackSelectionController* PlaylistController::selectionController() const
{
    return p->m_selectionController;
}

PresetRegistry* PlaylistController::presetRegistry() const
{
    return p->m_presetRegistry;
}

PlaylistColumnRegistry* PlaylistController::columnRegistry() const
{
    return p->m_columnRegistry;
}

bool PlaylistController::playlistsHaveLoaded() const
{
    return p->m_loaded;
}

PlaylistList PlaylistController::playlists() const
{
    return p->m_handler->playlists();
}

PlaylistTrack PlaylistController::currentTrack() const
{
    return p->m_playerController->currentPlaylistTrack();
}

PlayState PlaylistController::playState() const
{
    return p->m_playerController->playState();
}

void PlaylistController::aboutToChangeTracks()
{
    p->m_changingTracks = true;
}

void PlaylistController::changedTracks()
{
    p->m_changingTracks = false;
}

void PlaylistController::addPlaylistMenu(QMenu* menu)
{
    if(!p->m_currentPlaylist) {
        return;
    }

    auto* playlistMenu = new QMenu(tr("Playlists"), menu);

    const auto playlists = this->playlists();

    for(const auto& playlist : playlists) {
        if(playlist != p->m_currentPlaylist) {
            auto* switchPl = new QAction(playlist->name(), playlistMenu);
            const UId id   = playlist->id();
            QObject::connect(switchPl, &QAction::triggered, playlistMenu, [this, id]() { changeCurrentPlaylist(id); });
            playlistMenu->addAction(switchPl);
        }
    }

    menu->addMenu(playlistMenu);
}

Playlist* PlaylistController::currentPlaylist() const
{
    return p->m_currentPlaylist;
}

UId PlaylistController::currentPlaylistId() const
{
    return p->m_currentPlaylist ? p->m_currentPlaylist->id() : UId{};
}

void PlaylistController::changeCurrentPlaylist(Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    auto* prevPlaylist = std::exchange(p->m_currentPlaylist, playlist);

    if(prevPlaylist == playlist) {
        p->m_histories.erase(playlist);
        p->m_states.erase(playlist);

        emit playlistHistoryChanged();
        return;
    }

    emit currentPlaylistChanged(prevPlaylist, playlist);
}

void PlaylistController::changeCurrentPlaylist(const UId& id)
{
    if(auto* playlist = p->m_handler->playlistById(id)) {
        changeCurrentPlaylist(playlist);
    }
}

void PlaylistController::changePlaylistIndex(const UId& playlistId, int index)
{
    p->m_handler->changePlaylistIndex(playlistId, index);
}

void PlaylistController::clearCurrentPlaylist()
{
    if(p->m_currentPlaylist) {
        p->m_handler->clearPlaylistTracks(p->m_currentPlaylist->id());
    }
}

std::optional<PlaylistViewState> PlaylistController::playlistState(Playlist* playlist) const
{
    if(!playlist || !p->m_states.contains(playlist)) {
        return {};
    }

    return p->m_states.at(playlist);
}

void PlaylistController::savePlaylistState(Playlist* playlist, const PlaylistViewState& state)
{
    if(!playlist) {
        return;
    }

    if(state.scrollPos == 0 || state.topIndex < 0) {
        p->m_states.erase(playlist);
    }
    else {
        p->m_states[playlist] = state;
    }
}

void PlaylistController::addToHistory(QUndoCommand* command)
{
    if(!command || !p->m_currentPlaylist) {
        return;
    }

    p->m_histories[p->m_currentPlaylist].push(command);

    emit playlistHistoryChanged();
}

bool PlaylistController::canUndo() const
{
    if(!p->m_currentPlaylist || p->m_histories.empty()) {
        return false;
    }

    if(!p->m_histories.contains(p->m_currentPlaylist)) {
        return false;
    }

    return p->m_histories.at(p->m_currentPlaylist).canUndo();
}

bool PlaylistController::canRedo() const
{
    if(!p->m_currentPlaylist || p->m_histories.empty()) {
        return false;
    }

    if(!p->m_histories.contains(p->m_currentPlaylist)) {
        return false;
    }

    return p->m_histories.at(p->m_currentPlaylist).canRedo();
}

void PlaylistController::undoPlaylistChanges()
{
    if(!p->m_currentPlaylist) {
        return;
    }

    if(canUndo()) {
        p->m_histories.at(p->m_currentPlaylist).undo();
        emit playlistHistoryChanged();
    }
}

void PlaylistController::redoPlaylistChanges()
{
    if(!p->m_currentPlaylist) {
        return;
    }

    if(canRedo()) {
        p->m_histories.at(p->m_currentPlaylist).redo();
        emit playlistHistoryChanged();
    }
}

void PlaylistController::clearHistory()
{
    p->m_histories.clear();
}

bool PlaylistController::clipboardEmpty() const
{
    return p->m_clipboard.empty();
}

TrackList PlaylistController::clipboard() const
{
    return p->m_clipboard;
}

void PlaylistController::setClipboard(const TrackList& tracks)
{
    p->m_clipboard = tracks;
    emit clipboardChanged();
}

void PlaylistController::handleTrackSelectionAction(TrackAction action)
{
    if(action == TrackAction::SendCurrentPlaylist) {
        if(p->m_currentPlaylist) {
            p->m_histories.erase(p->m_currentPlaylist);
            p->m_states.erase(p->m_currentPlaylist);
        }
    }
}

void PlaylistController::startPlayback() const
{
    if(p->m_currentPlaylist) {
        p->m_handler->startPlayback(p->m_currentPlaylist->id());
    }
}

void PlaylistController::showNowPlaying()
{
    emit showCurrentTrack();
}

bool PlaylistController::currentIsActive() const
{
    return p->m_currentPlaylist == p->m_handler->activePlaylist();
}
} // namespace Fooyin

#include "moc_playlistcontroller.cpp"
