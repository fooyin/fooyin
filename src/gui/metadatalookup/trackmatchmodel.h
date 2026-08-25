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

#include "metadatamatcher.h"

#include <QAbstractTableModel>

namespace Fooyin {
class RetrievedTrackModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    using QAbstractTableModel::QAbstractTableModel;

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                      int role = Qt::DisplayRole) const override;

    void setRelease(const Release& release);

private:
    Release m_release;
    std::vector<const ReleaseTrack*> m_tracks;
};

class TrackMatchModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit TrackMatchModel(TrackList tracks, QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                      int role = Qt::DisplayRole) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QStringList mimeTypes() const override;
    [[nodiscard]] QMimeData* mimeData(const QModelIndexList& indexes) const override;
    [[nodiscard]] bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                       const QModelIndex& parent) const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                      const QModelIndex& parent) override;
    [[nodiscard]] Qt::DropActions supportedDropActions() const override;
    [[nodiscard]] Qt::DropActions supportedDragActions() const override;

    [[nodiscard]] const Release& release() const;
    void setRelease(Release release, bool matchByPosition = false);

    bool moveTrack(int sourceRow, int targetRow);

    [[nodiscard]] bool hasTrackAt(int row) const;
    [[nodiscard]] bool hasUnresolved() const;

    [[nodiscard]] const std::vector<TrackMatch>& matches() const;
    [[nodiscard]] std::vector<const ReleaseTrack*> remoteTracks() const;

Q_SIGNALS:
    void mappingsChanged();

private:
    void buildAutomaticOrder();
    void applyOrderAsMatches();

    TrackList m_tracks;
    Release m_release;
    std::vector<TrackMatch> m_automaticMatches;
    std::vector<TrackMatch> m_matches;
    std::vector<std::optional<size_t>> m_order;
    size_t m_remoteCount;
    bool m_matchByPosition{false};
};
} // namespace Fooyin
