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

#include <core/trackfwd.h>
#include <utils/treeitem.h>

#include <QStringList>

namespace Fooyin::Filters {
class FilterItem;

class FilterItem : public TreeItem<FilterItem>
{
public:
    enum FilterItemRole
    {
        Tracks = Qt::UserRole,
    };

    FilterItem() = default;
    explicit FilterItem(QString key, QStringList columns, FilterItem* parent);

    [[nodiscard]] QString key() const;

    [[nodiscard]] QStringList columns() const;
    [[nodiscard]] QString column(int column) const;
    [[nodiscard]] int columnCount() const override;

    [[nodiscard]] TrackList tracks() const;
    [[nodiscard]] int trackCount() const;

    void setColumns(const QStringList& columns);

    void addTrack(const Track& track);
    void addTracks(const TrackList& tracks);
    void removeTrack(const Track& track);
    void sortTracks();

    void sortChildren(int column, Qt::SortOrder order);

private:
    QString m_key;
    QStringList m_columns;
    TrackList m_tracks;
};
} // namespace Fooyin::Filters
