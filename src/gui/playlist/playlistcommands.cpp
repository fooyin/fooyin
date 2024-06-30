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

#include "playlistcommands.h"

#include "playlist/playlistcontroller.h"
#include "playlist/playlistwidget_p.h"
#include "playlistmodel.h"

#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>

namespace {
std::map<int, QPersistentModelIndex> saveQueuedIndexes(Fooyin::PlayerController* playerController,
                                                       Fooyin::PlaylistModel* model, const Fooyin::Id& playlistId)
{
    std::map<int, QPersistentModelIndex> indexes;

    const auto queuedTracks = playerController->playbackQueue().tracks();

    for(auto i{0}; const auto& track : queuedTracks) {
        if(track.playlistId == playlistId) {
            const QModelIndex trackIndex = model->indexAtPlaylistIndex(track.indexInPlaylist);
            if(trackIndex.isValid()) {
                indexes.emplace(i, trackIndex);
            }
        }
        ++i;
    }

    return indexes;
}

void restoreQueuedIndexes(Fooyin::PlayerController* playerController,
                          const std::map<int, QPersistentModelIndex>& indexes)
{
    if(indexes.empty()) {
        return;
    }

    auto queuedTracks = playerController->playbackQueue().tracks();

    for(const auto& [queueIndex, modelIndex] : indexes | std::views::reverse) {
        if(modelIndex.isValid()) {
            queuedTracks[queueIndex].indexInPlaylist = modelIndex.data(Fooyin::PlaylistItem::Index).toInt();
        }
        else {
            queuedTracks.erase(queuedTracks.begin() + queueIndex);
        }
    }

    playerController->replaceTracks(queuedTracks);
}
} // namespace

namespace Fooyin {
PlaylistCommand::PlaylistCommand(PlayerController* playerController, PlaylistModel* model, const Id& playlistId)
    : QUndoCommand{nullptr}
    , m_playerController{playerController}
    , m_model{model}
    , m_playlistId{playlistId}
{ }

InsertTracks::InsertTracks(PlayerController* playerController, PlaylistModel* model, const Id& playlistId,
                           TrackGroups groups)
    : PlaylistCommand{playerController, model, playlistId}
    , m_trackGroups{std::move(groups)}
{ }

void InsertTracks::undo()
{
    const auto queuedIndexes = saveQueuedIndexes(m_playerController, m_model, m_playlistId);

    m_model->removeTracks(m_trackGroups);

    restoreQueuedIndexes(m_playerController, queuedIndexes);
}

void InsertTracks::redo()
{
    const auto queuedIndexes = saveQueuedIndexes(m_playerController, m_model, m_playlistId);

    if(!queuedIndexes.empty()) {
        auto* playerController = m_playerController;
        QObject::connect(
            m_model, &PlaylistModel::playlistTracksChanged, m_model,
            [playerController, queuedIndexes]() { restoreQueuedIndexes(playerController, queuedIndexes); },
            Qt::SingleShotConnection);
    }

    m_model->insertTracks(m_trackGroups);
}

RemoveTracks::RemoveTracks(PlayerController* playerController, PlaylistModel* model, const Id& playlistId,
                           TrackGroups groups)
    : PlaylistCommand{playerController, model, playlistId}
    , m_trackGroups{std::move(groups)}
{ }

void RemoveTracks::undo()
{
    const auto queuedIndexes = saveQueuedIndexes(m_playerController, m_model, m_playlistId);

    if(!queuedIndexes.empty()) {
        auto* playerController = m_playerController;
        QObject::connect(
            m_model, &PlaylistModel::playlistTracksChanged, m_model,
            [playerController, queuedIndexes]() { restoreQueuedIndexes(playerController, queuedIndexes); },
            Qt::SingleShotConnection);
    }

    m_model->insertTracks(m_trackGroups);
}

void RemoveTracks::redo()
{
    const auto queuedIndexes = saveQueuedIndexes(m_playerController, m_model, m_playlistId);

    m_model->removeTracks(m_trackGroups);

    restoreQueuedIndexes(m_playerController, queuedIndexes);
}

MoveTracks::MoveTracks(PlayerController* playerController, PlaylistModel* model, const Id& playlistId,
                       MoveOperation operation)
    : PlaylistCommand{playerController, model, playlistId}
    , m_operation{std::move(operation)}
{ }

void MoveTracks::undo()
{
    const auto queuedIndexes = saveQueuedIndexes(m_playerController, m_model, m_playlistId);

    m_model->moveTracks(m_undoOperation);

    restoreQueuedIndexes(m_playerController, queuedIndexes);
}

void MoveTracks::redo()
{
    const auto queuedIndexes = saveQueuedIndexes(m_playerController, m_model, m_playlistId);

    m_undoOperation = m_model->moveTracks(m_operation);

    restoreQueuedIndexes(m_playerController, queuedIndexes);
}

ResetTracks::ResetTracks(PlayerController* playerController, PlaylistModel* model, const Id& playlistId,
                         TrackList oldTracks, TrackList newTracks)
    : PlaylistCommand{playerController, model, playlistId}
    , m_oldTracks{std::move(oldTracks)}
    , m_newTracks{std::move(newTracks)}
{ }

void ResetTracks::undo()
{
    const auto queuedIndexes = saveQueuedIndexes(m_playerController, m_model, m_playlistId);

    auto* model            = m_model;
    auto* playerController = m_playerController;
    QObject::connect(
        model, &PlaylistModel::playlistLoaded, model,
        [model, playerController, queuedIndexes]() {
            model->tracksChanged();
            restoreQueuedIndexes(playerController, queuedIndexes);
        },
        Qt::SingleShotConnection);

    m_model->tracksAboutToBeChanged();
    m_model->reset(m_oldTracks);
}

void ResetTracks::redo()
{
    const auto queuedIndexes = saveQueuedIndexes(m_playerController, m_model, m_playlistId);

    auto* model            = m_model;
    auto* playerController = m_playerController;
    QObject::connect(
        model, &PlaylistModel::playlistLoaded, model,
        [model, playerController, queuedIndexes]() {
            model->tracksChanged();
            restoreQueuedIndexes(playerController, queuedIndexes);
        },
        Qt::SingleShotConnection);

    m_model->tracksAboutToBeChanged();
    m_model->reset(m_newTracks);
}
} // namespace Fooyin
