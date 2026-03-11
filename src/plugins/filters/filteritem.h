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

#include <core/track.h>
#include <gui/scripting/richtext.h>
#include <utils/crypto.h>
#include <utils/treeitem.h>

#include <QStringList>

namespace Fooyin {
class MusicLibrary;
class TrackSorter;
struct ParsedScript;

namespace Filters {
class FilterItem;

struct IconCaptionLine
{
    RichText text;
    Qt::Alignment alignment{Qt::AlignLeft};

    bool operator==(const IconCaptionLine& other) const
    {
        return std::tie(text, alignment) == std::tie(other.text, other.alignment);
    }
};

using IconCaptionLineList = std::vector<IconCaptionLine>;

class FilterItem : public TreeItem<FilterItem>
{
public:
    enum FilterItemRole
    {
        Tracks = Qt::UserRole,
        TrackIdsRole,
        Key,
        IsSummary,
        IconLabel,
        IconCaptionLines,
        RichColumn
    };

    FilterItem() = default;
    FilterItem(Md5Hash key, QStringList columns, FilterItem* parent);

    [[nodiscard]] Md5Hash key() const;

    [[nodiscard]] const QStringList& columns() const;
    [[nodiscard]] QString column(int column) const;
    [[nodiscard]] const RichText& richColumn(int column) const;
    [[nodiscard]] QString iconLabel(const std::vector<int>& columnOrder) const;
    [[nodiscard]] IconCaptionLineList iconCaptionLines(const std::vector<int>& columnOrder,
                                                       const std::vector<Qt::Alignment>& columnAlignments) const;

    [[nodiscard]] const TrackIds& trackIds() const;
    [[nodiscard]] int trackCount() const;
    [[nodiscard]] int firstTrackId() const;

    void setColumns(const QStringList& columns);
    void setRichColumns(const std::vector<RichText>& columns);
    void removeColumn(int column);

    [[nodiscard]] bool isSummary() const;
    void setIsSummary(bool isSummary);

    void addTrack(const Track& track);
    void addTracks(const TrackIds& trackIds);
    void removeTrack(const Track& track);
    void sortTracks(MusicLibrary* library, TrackSorter& sorter, const ParsedScript& script);

private:
    void invalidateIconCaches();
    void updateIconCaptionColumns(const std::vector<int>& columnOrder) const;

    Md5Hash m_key;
    QStringList m_columns;
    std::vector<RichText> m_richColumns;
    TrackIds m_trackIds;
    bool m_isSummary;

    mutable std::vector<int> m_cachedIconLabelOrder;
    mutable std::vector<int> m_cachedIconCaptionOrder;
    mutable std::vector<int> m_cachedIconCaptionColumns;
    mutable QString m_cachedIconLabel;
    mutable bool m_iconLabelCacheValid{false};
    mutable bool m_iconCaptionColumnsCacheValid{false};
};
} // namespace Filters
} // namespace Fooyin

Q_DECLARE_METATYPE(Fooyin::Filters::IconCaptionLine)
Q_DECLARE_METATYPE(Fooyin::Filters::IconCaptionLineList)
