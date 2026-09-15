/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include <core/playlist/playlist.h>

#include <QAbstractTableModel>

class QMimeData;

namespace Fooyin {
class PlaylistInteractor;
class PlaylistHandler;
class PlayerController;

struct PlaylistSummary
{
    int trackCount{0};
    uint64_t durationMs{0};
    uint64_t totalSize{0};
    bool unknownDuration{false};
    bool unknownSize{false};
};

class PlaylistManagerModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Role
    {
        SortRole = Qt::UserRole
    };

    enum Column
    {
        Name = 0,
        Tracks,
        Duration,
        TotalSize,
        Count,
    };

    explicit PlaylistManagerModel(PlaylistInteractor* playlistInteractor, QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QStringList mimeTypes() const override;
    [[nodiscard]] bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                       const QModelIndex& parent) const override;
    [[nodiscard]] Qt::DropActions supportedDropActions() const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                      const QModelIndex& parent) override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    [[nodiscard]] Playlist* playlistForRow(int row) const;
    [[nodiscard]] QModelIndex indexForPlaylist(const Playlist* playlist, int column = 0) const;

    void populate();

    void addPlaylist(Playlist* playlist);
    void reorderPlaylist(Playlist* playlist);
    void removePlaylist(Playlist* playlist);
    void refreshPlaylist(Playlist* playlist);

private:
    [[nodiscard]] int rowForPlaylist(const Playlist* playlist) const;
    [[nodiscard]] Playlist* playlistForDropTarget(int row, const QModelIndex& parent) const;
    [[nodiscard]] bool isSupportedDrop(const QMimeData* data) const;

    PlaylistInteractor* m_playlistInteractor;
    PlaylistHandler* m_playlistHandler;
    PlayerController* m_playerController;
    PlaylistList m_playlists;
    std::vector<PlaylistSummary> m_summaries;
};
} // namespace Fooyin
