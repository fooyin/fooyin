/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#pragma once

#include "playlistcolumn.h"
#include "playlistitem.h"
#include "playlistpreset.h"

#include <core/player/playbackqueue.h>
#include <core/scripting/scripttypes.h>

#include <core/track.h>
#include <utils/crypto.h>
#include <utils/id.h>
#include <utils/worker.h>

#include <memory>
#include <ranges>
#include <stop_token>

namespace Fooyin {
class PlaylistPopulatorPrivate;

using ItemList       = std::vector<PlaylistItem>;
using TrackItemMap   = std::unordered_map<PlaylistTrack, PlaylistItem, PlaylistTrack::PlaylistTrackHash>;
using ItemKeyMap     = std::unordered_map<UId, PlaylistItem, UId::UIdHash>;
using NodeKeyMap     = std::unordered_map<UId, std::vector<UId>, UId::UIdHash>;
using TrackIdNodeMap = std::unordered_map<int, std::vector<UId>>;
using IndexGroupMap  = std::map<int, std::vector<UId>>;

struct PlaylistPopulationRequest
{
    uint64_t generation{0};
    std::stop_token stopToken;
    UId playlistId;
    int trackCount{0};
    PlaylistPreset preset;
    PlaylistColumnList columns;
    QFont font;
    int preloadCount{2000};
    ScriptContextData scriptData;
    ScriptEvaluationOptions scriptOptions;
    PlaylistTrack playingTrack;
    int stopAfterIndex{-1};
    PlaylistTrackIndexes queueIndexes;
    int queueTotal{0};
};

struct HeaderUpdateRequest
{
    PlaylistItem item;
    TrackList tracks;
};
using HeaderUpdateList = std::vector<HeaderUpdateRequest>;

struct PendingData
{
    uint64_t generation{0};
    UId playlistId;
    ItemKeyMap items;
    ItemKeyMap updatedItems;
    NodeKeyMap nodes;
    std::vector<UId> containerOrder;
    TrackIdNodeMap trackParents;

    QString parent;
    int row{-1};

    IndexGroupMap indexNodes;

    void rebindParents(PlaylistItem* root = nullptr)
    {
        for(const auto& [parentKey, children] : nodes) {
            PlaylistItem* parentItem{nullptr};
            if(parentKey.isNull()) {
                parentItem = root;
            }
            else if(const auto it = items.find(parentKey); it != items.end()) {
                parentItem = &it->second;
            }
            for(const auto& childKey : children) {
                items.at(childKey).setParent(parentItem);
            }
        }
        for(auto& item : updatedItems | std::views::values) {
            item.setParent(nullptr);
        }
    }

    void clear()
    {
        generation = 0;
        playlistId = {};
        items.clear();
        updatedItems.clear();
        nodes.clear();
        containerOrder.clear();
        trackParents.clear();
        row = -1;
        indexNodes.clear();
    }
};
using PendingDataPtr = std::shared_ptr<PendingData>;

class PlaylistPopulator : public Worker
{
    Q_OBJECT

public:
    explicit PlaylistPopulator(QObject* parent = nullptr);
    ~PlaylistPopulator() override;

    void run(const PlaylistPopulationRequest& request, const PlaylistTrackList& tracks);
    void runTracks(const PlaylistPopulationRequest& request, const std::map<int, PlaylistTrackList>& tracks);
    void updateTracks(const PlaylistPopulationRequest& request, const std::set<int>& columnsToUpdate,
                      const TrackItemMap& tracks);
    void updateHeaders(const PlaylistPopulationRequest& request, const HeaderUpdateList& headers);

Q_SIGNALS:
    void populationFinished(uint64_t generation);
    void populated(Fooyin::PendingDataPtr data);
    void populatedTrackGroup(Fooyin::PendingDataPtr data);
    void tracksUpdated(uint64_t generation, const Fooyin::ItemList& tracks, const std::set<int>& columnsUpdated);
    void headersUpdated(uint64_t generation, const Fooyin::ItemKeyMap& headers);

private:
    std::unique_ptr<PlaylistPopulatorPrivate> p;
};
} // namespace Fooyin

Q_DECLARE_METATYPE(Fooyin::PendingDataPtr)
