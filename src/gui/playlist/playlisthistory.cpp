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

#include "playlisthistory.h"

#include "playlistmodel.h"

#include <core/player/playermanager.h>

namespace {
std::map<int, QPersistentModelIndex> saveQueuedIndexes(Fooyin::PlayerManager* playerManager,
                                                       Fooyin::PlaylistModel* model, int playlistId)
{
    std::map<int, QPersistentModelIndex> indexes;

    const auto queuedTracks = playerManager->playbackQueue().tracks();
    for(auto i{0}; const auto& track : queuedTracks) {
        if(track.playlistId == playlistId) {
            const QModelIndex trackIndex = model->indexAtTrackIndex(track.indexInPlaylist);
            if(trackIndex.isValid()) {
                indexes.emplace(i, trackIndex);
            }
        }
        ++i;
    }

    return indexes;
}

void restoreQueuedIndexes(Fooyin::PlayerManager* playerManager, const std::map<int, QPersistentModelIndex>& indexes)
{
    auto queuedTracks = playerManager->playbackQueue().tracks();

    for(const auto& [queueIndex, modelIndex] : indexes | std::views::reverse) {
        if(modelIndex.isValid()) {
            queuedTracks[queueIndex].indexInPlaylist = modelIndex.data(Fooyin::PlaylistItem::Index).toInt();
        }
        else {
            queuedTracks.erase(queuedTracks.begin() + queueIndex);
        }
    }

    playerManager->replaceTracks(queuedTracks);
}
} // namespace

namespace Fooyin {
PlaylistCommand::PlaylistCommand(PlaylistModel* model, int playlistId)
    : QUndoCommand{nullptr}
    , m_model{model}
    , m_playlistId{playlistId}
{ }

InsertTracks::InsertTracks(PlayerManager* playerManager, PlaylistModel* model, int playlistId, TrackGroups groups)
    : PlaylistCommand{model, playlistId}
    , m_playerManager{playerManager}
    , m_trackGroups{std::move(groups)}
{ }

void InsertTracks::undo()
{
    const auto queuedIndexes = saveQueuedIndexes(m_playerManager, m_model, m_playlistId);

    m_model->removeTracks(m_trackGroups);

    restoreQueuedIndexes(m_playerManager, queuedIndexes);
}

void InsertTracks::redo()
{
    const auto queuedIndexes = saveQueuedIndexes(m_playerManager, m_model, m_playlistId);

    QObject::connect(
        m_model, &PlaylistModel::playlistTracksChanged, m_model,
        [this, queuedIndexes]() { restoreQueuedIndexes(m_playerManager, queuedIndexes); }, Qt::SingleShotConnection);

    m_model->insertTracks(m_trackGroups);
}

RemoveTracks::RemoveTracks(PlayerManager* playerManager, PlaylistModel* model, int playlistId, TrackGroups groups)
    : PlaylistCommand{model, playlistId}
    , m_playerManager{playerManager}
    , m_trackGroups{std::move(groups)}
{ }

void RemoveTracks::undo()
{
    const auto queuedIndexes = saveQueuedIndexes(m_playerManager, m_model, m_playlistId);

    QObject::connect(
        m_model, &PlaylistModel::playlistTracksChanged, m_model,
        [this, queuedIndexes]() { restoreQueuedIndexes(m_playerManager, queuedIndexes); }, Qt::SingleShotConnection);

    m_model->insertTracks(m_trackGroups);
}

void RemoveTracks::redo()
{
    const auto queuedIndexes = saveQueuedIndexes(m_playerManager, m_model, m_playlistId);

    m_model->removeTracks(m_trackGroups);

    restoreQueuedIndexes(m_playerManager, queuedIndexes);
}

MoveTracks::MoveTracks(PlayerManager* playerManager, PlaylistModel* model, int playlistId, MoveOperation operation)
    : PlaylistCommand{model, playlistId}
    , m_playerManager{playerManager}
    , m_operation{std::move(operation)}
{ }

void MoveTracks::undo()
{
    const auto queuedIndexes = saveQueuedIndexes(m_playerManager, m_model, m_playlistId);

    m_model->moveTracks(m_undoOperation);

    restoreQueuedIndexes(m_playerManager, queuedIndexes);
}

void MoveTracks::redo()
{
    const auto queuedIndexes = saveQueuedIndexes(m_playerManager, m_model, m_playlistId);

    m_undoOperation = m_model->moveTracks(m_operation);

    restoreQueuedIndexes(m_playerManager, queuedIndexes);
}
} // namespace Fooyin
