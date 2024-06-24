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

#pragma once

#include "filterfwd.h"
#include "filteritem.h"

#include <core/track.h>
#include <utils/treemodel.h>

namespace Fooyin {
class SettingsManager;

namespace Filters {
struct FilterOptions;

class FilterModel : public TreeModel<FilterItem>
{
    Q_OBJECT

public:
    explicit FilterModel(SettingsManager* settings, QObject* parent = nullptr);
    ~FilterModel() override;

    [[nodiscard]] int sortColumn() const;
    [[nodiscard]] Qt::SortOrder sortOrder() const;
    void sortOnColumn(int column, Qt::SortOrder order);
    [[nodiscard]] bool showSummary() const;

    void setFont(const QString& font);
    void setColour(const QColor& colour);
    void setRowHeight(int height);
    void setShowSummary(bool show);
    void setShowDecoration(bool show);

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    bool setHeaderData(int section, Qt::Orientation orientation, const QVariant& value, int role) override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

    [[nodiscard]] QStringList mimeTypes() const override;
    [[nodiscard]] Qt::DropActions supportedDragActions() const override;
    [[nodiscard]] QMimeData* mimeData(const QModelIndexList& indexes) const override;

    [[nodiscard]] Qt::Alignment columnAlignment(int column) const;
    void changeColumnAlignment(int column, Qt::Alignment alignment);
    void resetColumnAlignment(int column);
    void resetColumnAlignments();

    [[nodiscard]] QModelIndexList indexesForValues(const QStringList& values, int column = 0) const;

    void addTracks(const TrackList& tracks);
    void updateTracks(const TrackList& tracks);
    void refreshTracks(const TrackList& tracks);
    void removeTracks(const TrackList& tracks);
    bool removeColumn(int column);

    void reset(const FilterColumnList& columns, const TrackList& tracks);

signals:
    void modelUpdated();

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Filters
} // namespace Fooyin
