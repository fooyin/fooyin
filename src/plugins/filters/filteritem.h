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

#include <core/trackfwd.h>
#include <utils/treeitem.h>

#include <QObject>

namespace Fooyin::Filters {
class FilterItem;

class FilterItem : public TreeItem<FilterItem>
{
public:
    enum FilterItemRole
    {
        Title = Qt::UserRole,
        Tracks,
        Sorting,
        AllNode
    };

    FilterItem() = default;
    explicit FilterItem(QString title, QString sortTitle, FilterItem* parent, bool isAllNode = false);

    [[nodiscard]] QString title() const;
    [[nodiscard]] QString sortTitle() const;
    [[nodiscard]] TrackList tracks() const;
    [[nodiscard]] int trackCount() const;

    void setTitle(const QString& title);

    void addTrack(const Track& track);
    void addTracks(const TrackList& tracks);
    void removeTrack(const Track& track);

    [[nodiscard]] bool isAllNode() const;

    void sortChildren(Qt::SortOrder order);

private:
    QString m_title;
    QString m_sortTitle;
    TrackList m_tracks;
    bool m_isAllNode;
};
} // namespace Fooyin::Filters
