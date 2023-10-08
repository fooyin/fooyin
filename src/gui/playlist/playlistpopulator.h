/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "playlistitem.h"
#include "playlistitemmodels.h"

#include <core/track.h>
#include <utils/worker.h>

namespace Fy::Gui::Widgets::Playlist {
struct PlaylistPreset;

using ItemList        = std::vector<PlaylistItem>;
using ItemKeyMap      = std::unordered_map<QString, PlaylistItem>;
using ContainerKeyMap = std::unordered_map<QString, Container*>;
using NodeKeyMap      = std::unordered_map<QString, std::vector<QString>>;

struct PendingData
{
    ItemKeyMap items;
    NodeKeyMap nodes;

    void clear()
    {
        items.clear();
        nodes.clear();
    }
};

class PlaylistPopulator : public Utils::Worker
{
    Q_OBJECT

public:
    explicit PlaylistPopulator(QObject* parent = nullptr);
    ~PlaylistPopulator() override;

    void run(const PlaylistPreset& preset, const Core::TrackList& tracks);
    void updateHeaders(const ItemList& items);

signals:
    void populated(PendingData data);
    void headersUpdated(ItemKeyMap headers);

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fy::Gui::Widgets::Playlist
