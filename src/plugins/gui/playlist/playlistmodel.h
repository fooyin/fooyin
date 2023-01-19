/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include <QAbstractItemModel>
#include <QPixmap>
#include <core/library/models/trackfwd.h>

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
using PlaylistItemHash = std::unordered_map<QString, std::unique_ptr<PlaylistItem>>;

class PlaylistModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    explicit PlaylistModel(Core::Player::PlayerManager* playerManager, Core::Library::MusicLibrary* library,
                           QObject* parent = nullptr);
    ~PlaylistModel() override;

    void setupModelData();
    PlaylistItem* iterateTrack(Core::Track* track, bool discHeaders, bool splitDiscs);

    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QVariant trackData(PlaylistItem* item, int role) const;
    [[nodiscard]] QVariant albumData(PlaylistItem* item, int role) const;
    [[nodiscard]] QVariant discData(PlaylistItem* item, int role) const;

    [[nodiscard]] QString trackArtistString(Core::Track* track) const;

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    [[nodiscard]] QModelIndex parent(const QModelIndex& index) const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] QModelIndexList match(const QModelIndex& start, int role, const QVariant& value, int hits,
                                        Qt::MatchFlags flags) const override;

    void traverseIndex(const QModelIndex& start, QModelIndexList& list) const;

    void reset();
    void changeRowColours();
    void changeTrackState();
    QModelIndex indexOfItem(int id);
    QModelIndex indexOfItem(const PlaylistItem* item);

protected:
    void resetContainers();
    void beginReset();
    PlaylistItem* checkInsertKey(const QString& key, PlaylistItem::Type type, Core::LibraryItem* item,
                                 PlaylistItem* parent);
    void createAlbums(const Core::TrackPtrList& tracks);
    void filterModel(const Core::TrackPtrList& tracks);

private:
    std::unique_ptr<PlaylistItem> m_root;
    PlaylistItemHash m_nodes;
    Core::ContainerHash m_containers;
    Core::Player::PlayerManager* m_playerManager;
    Core::Library::MusicLibrary* m_library;
    Core::SettingsManager* m_settings;
    QPixmap m_playingIcon;
    QPixmap m_pausedIcon;
};
} // namespace Gui::Widgets
