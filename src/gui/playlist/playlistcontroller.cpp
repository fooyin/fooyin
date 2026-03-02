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

#include <core/application.h>
#include <core/coresettings.h>
#include <core/library/musiclibrary.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <core/track.h>
#include <gui/guisettings.h>
#include <gui/trackselectioncontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QIODevice>
#include <QMenu>
#include <QUndoStack>

constexpr auto LastPlaylistId = "PlaylistWidget/LastPlaylistId";
constexpr auto PlaylistStates = "PlaylistWidget/PlaylistStates";

namespace Fooyin {
class PlaylistControllerPrivate : public QObject
{
    Q_OBJECT

public:
    PlaylistControllerPrivate(PlaylistController* self, Application* app,
                              TrackSelectionController* selectionController);

    void restoreLastPlaylist();

    void handlePlaylistAdded(Playlist* playlist);
    void handlePlaylistTracksAdded(Playlist* playlist, const TrackList& tracks, int index) const;

    void handleTracksQueued(const QueueTracks& tracks) const;
    void handleTracksDequeued(const QueueTracks& tracks) const;
    void handleTracksDequeued(const PlaylistIndexes& indexes) const;
    void handleQueueChanged(const QueueTracks& removed, const QueueTracks& added);

    void handlePlaylistUpdated(Playlist* playlist, const std::vector<int>& indexes);
    void handleTracksUpdated(Playlist* playlist, const std::vector<int>& indexes) const;
    void handlePlaylistRemoved(Playlist* playlist);

    void saveStates() const;
    void restoreStates();

    PlaylistController* m_self;

    PlaylistHandler* m_handler;
    PlayerController* m_playerController;
    MusicLibrary* m_library;
    TrackSelectionController* m_selectionController;
    SettingsManager* m_settings;
    PresetRegistry* m_presetRegistry;
    PlaylistColumnRegistry* m_columnRegistry;

    bool m_loaded{false};
    Playlist* m_currentPlaylist{nullptr};
    bool m_changingTracks{false};

    std::unordered_map<Playlist*, QUndoStack> m_histories;
    std::unordered_map<Playlist*, PlaylistViewState> m_states;
    std::unordered_map<Playlist*, QString> m_currentSearches;
    TrackList m_clipboard;
};

PlaylistControllerPrivate::PlaylistControllerPrivate(PlaylistController* self, Application* app,
                                                     TrackSelectionController* selectionController)
    : m_self{self}
    , m_handler{app->playlistHandler()}
    , m_playerController{app->playerController()}
    , m_library{app->library()}
    , m_selectionController{selectionController}
    , m_settings{app->settingsManager()}
    , m_presetRegistry{new PresetRegistry(m_settings, m_self)}
    , m_columnRegistry{new PlaylistColumnRegistry(m_settings, m_self)}
{
    QObject::connect(m_handler, &PlaylistHandler::playlistsPopulated, this,
                     &PlaylistControllerPrivate::restoreLastPlaylist);
    QObject::connect(m_handler, &PlaylistHandler::playlistAdded, this, &PlaylistControllerPrivate::handlePlaylistAdded);
    QObject::connect(m_handler, &PlaylistHandler::tracksChanged, this,
                     &PlaylistControllerPrivate::handlePlaylistUpdated);
    QObject::connect(m_handler, &PlaylistHandler::tracksUpdated, this, &PlaylistControllerPrivate::handleTracksUpdated);
    QObject::connect(m_handler, &PlaylistHandler::tracksAdded, this,
                     &PlaylistControllerPrivate::handlePlaylistTracksAdded);
    QObject::connect(m_handler, &PlaylistHandler::playlistRemoved, this,
                     &PlaylistControllerPrivate::handlePlaylistRemoved);

    QObject::connect(m_playerController, &PlayerController::playlistTrackChanged, m_self,
                     &PlaylistController::playingTrackChanged);
    QObject::connect(m_playerController, &PlayerController::tracksQueued, this,
                     &PlaylistControllerPrivate::handleTracksQueued);
    QObject::connect(m_playerController, &PlayerController::trackQueueChanged, this,
                     &PlaylistControllerPrivate::handleQueueChanged);
    QObject::connect(m_playerController, &PlayerController::tracksDequeued, this,
                     [this](const QueueTracks& tracks) { handleTracksDequeued(tracks); });
    QObject::connect(m_playerController, &PlayerController::trackIndexesDequeued, this,
                     [this](const PlaylistIndexes& indexes) { handleTracksDequeued(indexes); });
    QObject::connect(m_playerController, &PlayerController::playStateChanged, m_self,
                     &PlaylistController::playStateChanged);
}

void PlaylistControllerPrivate::restoreLastPlaylist()
{
    const FyStateSettings stateSettings;
    const int lastId = stateSettings.value(LastPlaylistId).toInt();

    if(lastId >= 0) {
        m_currentPlaylist = m_handler->playlistByDbId(lastId);
        if(!m_currentPlaylist) {
            m_currentPlaylist = m_handler->playlistByIndex(0);
        }
    }

    if(m_currentPlaylist && !m_handler->activePlaylist()
       && m_playerController->playState() == Player::PlayState::Stopped) {
        m_handler->changeActivePlaylist(m_currentPlaylist);
    }

    m_loaded = true;
    emit m_self->playlistsLoaded();
}

void PlaylistControllerPrivate::handlePlaylistAdded(Playlist* playlist)
{
    if(playlist) {
        m_histories.erase(playlist);
        m_states.erase(playlist);
    }
}

void PlaylistControllerPrivate::handlePlaylistTracksAdded(Playlist* playlist, const TrackList& tracks, int index) const
{
    if(playlist == m_currentPlaylist) {
        emit m_self->currentPlaylistTracksAdded(tracks, index);
    }
}

void PlaylistControllerPrivate::handleTracksQueued(const QueueTracks& tracks) const
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

void PlaylistControllerPrivate::handleTracksDequeued(const QueueTracks& tracks) const
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

void PlaylistControllerPrivate::handleTracksDequeued(const PlaylistIndexes& indexes) const
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

void PlaylistControllerPrivate::handleQueueChanged(const QueueTracks& removed, const QueueTracks& added)
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

void PlaylistControllerPrivate::handlePlaylistUpdated(Playlist* playlist, const std::vector<int>& indexes)
{
    if(m_changingTracks) {
        return;
    }

    bool allNew{false};

    if(playlist && (indexes.empty() || std::cmp_equal(indexes.size(), playlist->trackCount()))) {
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

void PlaylistControllerPrivate::handleTracksUpdated(Playlist* playlist, const std::vector<int>& indexes) const
{
    if(m_changingTracks) {
        return;
    }

    if(playlist == m_currentPlaylist) {
        emit m_self->currentPlaylistTracksPlayed(indexes);
    }
}

void PlaylistControllerPrivate::handlePlaylistRemoved(Playlist* playlist)
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

void PlaylistControllerPrivate::saveStates() const
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

    FyStateSettings stateSettings;
    stateSettings.setValue(PlaylistStates, out);
    stateSettings.setValue(LastPlaylistId, m_currentPlaylist->dbId());
}

void PlaylistControllerPrivate::restoreStates()
{
    const FyStateSettings stateSettings;
    QByteArray in = stateSettings.value(PlaylistStates).toByteArray();

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

PlaylistController::PlaylistController(Application* app, TrackSelectionController* selectionController, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<PlaylistControllerPrivate>(this, app, selectionController)}
{
    p->restoreStates();
}

PlaylistController::~PlaylistController()
{
    if(p->m_currentPlaylist) {
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

Player::PlayState PlaylistController::playState() const
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

void PlaylistController::startPlayback() const
{
    if(p->m_currentPlaylist) {
        p->m_playerController->startPlayback(p->m_currentPlaylist->id());
    }
}

void PlaylistController::showNowPlaying()
{
    emit showCurrentTrack();
}

void PlaylistController::selectTrackIds(const TrackIds& ids)
{
    emit selectTracks(ids);
}

void PlaylistController::focusPlaylist()
{
    emit requestPlaylistFocus();
}

bool PlaylistController::currentIsActive() const
{
    return p->m_currentPlaylist == p->m_handler->activePlaylist();
}

bool PlaylistController::currentIsAuto() const
{
    return p->m_currentPlaylist && p->m_currentPlaylist->isAutoPlaylist();
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

    if(!p->m_handler->activePlaylist() && p->m_playerController->playState() == Player::PlayState::Stopped) {
        p->m_handler->changeActivePlaylist(playlist);
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

void PlaylistController::filterCurrentPlaylist(const PlaylistTrackList& tracks)
{
    if(currentPlaylist()) {
        emit filterTracks(tracks);
    }
}

void PlaylistController::clearCurrentPlaylist()
{
    if(p->m_currentPlaylist) {
        p->m_handler->clearPlaylistTracks(p->m_currentPlaylist->id());
    }
}

QString PlaylistController::currentSearch(Playlist* playlist) const
{
    if(!playlist || !p->m_currentSearches.contains(playlist)) {
        return {};
    }

    return p->m_currentSearches.at(playlist);
}

void PlaylistController::setSearch(Playlist* playlist, const QString& search)
{
    if(!playlist) {
        return;
    }

    p->m_currentSearches[playlist] = search;
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
} // namespace Fooyin

#include "moc_playlistcontroller.cpp"
#include "playlistcontroller.moc"
