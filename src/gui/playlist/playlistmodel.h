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

#include <core/models/trackfwd.h>

#include <QAbstractItemModel>

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
struct PlaylistPreset;

class PlaylistModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    PlaylistModel(Core::Player::PlayerManager* playerManager, Utils::SettingsManager* settings,
                  QObject* parent = nullptr);
    ~PlaylistModel() override;

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    [[nodiscard]] QModelIndex parent(const QModelIndex& index) const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QModelIndex indexOfItem(const PlaylistItem& item);

    [[nodiscard]] QModelIndex matchTrack(int id) const;

    void reset(const Core::Playlist::Playlist&);

    void changeTrackState();

    void changePreset(const PlaylistPreset& preset);

    [[nodiscard]] QModelIndex indexForTrack(const Core::Track& track) const;
    [[nodiscard]] QModelIndex indexForItem(PlaylistItem* item) const;

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Gui::Widgets::Playlist
} // namespace Fy