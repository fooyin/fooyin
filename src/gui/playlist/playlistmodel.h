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

#include <utils/treemodel.h>

#include <QAbstractItemModel>
#include <QPixmap>

namespace Core {
class SettingsManager;

namespace Player {
class PlayerManager;
}

namespace Library {
class MusicLibrary;
}
} // namespace Core

namespace Gui::Widgets {
namespace Playlist {
enum Role
{
    Type = Qt::UserRole + 20,
    Mode = Qt::UserRole + 21,
};
}

class PlaylistModel : public Utils::TreeModel<PlaylistItem>
{
    Q_OBJECT

public:
    explicit PlaylistModel(Core::Player::PlayerManager* playerManager, Core::Library::MusicLibrary* library,
                           Core::SettingsManager* settings, QObject* parent = nullptr);
    ~PlaylistModel() override = default;

    void setupModelData();
    PlaylistItem* iterateTrack(Core::Track* track, bool discHeaders, bool splitDiscs);

    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QVariant trackData(PlaylistItem* item, int role) const;
    [[nodiscard]] QVariant albumData(PlaylistItem* item, int role) const;
    [[nodiscard]] QVariant discData(PlaylistItem* item, int role) const;

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] QModelIndexList match(const QModelIndex& start, int role, const QVariant& value, int hits,
                                        Qt::MatchFlags flags) const override;

    void traverseIndex(const QModelIndex& start, QModelIndexList& list) const;

    void reset();
    void changeRowColours();
    void changeTrackState();
    QModelIndex indexOfItem(int id);

protected:
    void resetContainers();
    void beginReset();
    PlaylistItem* checkInsertKey(const QString& key, PlaylistItem::Type type, Core::LibraryItem* item,
                                 PlaylistItem* parent);
    void createAlbums(const Core::TrackPtrList& tracks);
    void filterModel(const Core::TrackPtrList& tracks);

private:
    using PlaylistItemHash = std::unordered_map<QString, std::unique_ptr<PlaylistItem>>;

    Core::Player::PlayerManager* m_playerManager;
    Core::Library::MusicLibrary* m_library;
    Core::SettingsManager* m_settings;

    PlaylistItemHash m_nodes;
    Core::ContainerHash m_containers;
    QPixmap m_playingIcon;
    QPixmap m_pausedIcon;
};
} // namespace Gui::Widgets
