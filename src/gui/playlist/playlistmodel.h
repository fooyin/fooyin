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

#include <core/library/coverprovider.h>
#include <core/models/album.h>
#include <core/models/container.h>
#include <core/models/trackfwd.h>

#include <utils/treemodel.h>

#include <QAbstractItemModel>
#include <QPixmap>

namespace Fy {
namespace Utils {
class SettingsManager;
}

namespace Core {
namespace Player {
class PlayerManager;
}

namespace Playlist {
class Playlist;
} // namespace Playlist
} // namespace Core

namespace Gui::Widgets::Playlist {
enum Role
{
    Type = Qt::UserRole + 20,
    Mode = Qt::UserRole + 21,
};

class PlaylistModel : public Utils::TreeModel<PlaylistItem>
{
    Q_OBJECT

public:
    PlaylistModel(Core::Player::PlayerManager* playerManager, Utils::SettingsManager* settings,
                  QObject* parent = nullptr);
    ~PlaylistModel() override;

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QModelIndex matchTrack(int id) const;

    void reset(Core::Playlist::Playlist* playlist);
    void setupModelData(const Core::Playlist::Playlist* playlist);
    void changeTrackState();

    [[nodiscard]] QModelIndex indexForTrack(const Core::Track& track) const;
    [[nodiscard]] QModelIndex indexForItem(PlaylistItem* item) const;

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Gui::Widgets::Playlist
} // namespace Fy
