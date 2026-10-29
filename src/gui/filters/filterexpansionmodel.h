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

#include "filterexpansionpopulator.h"

#include <utils/treeitem.h>
#include <utils/treemodel.h>

#include <set>

namespace Fooyin::Filters {
struct ExpansionTrackItem : TreeItem<ExpansionTrackItem>
{
    int track{-1};
    int subheader{-1};
    int firstTrack{0};
    int trackCount{0};
};

class ExpansionTrackModel : public TreeModel<ExpansionTrackItem>
{
public:
    enum ExpansionTrackRole
    {
        LayoutRole = Qt::UserRole,
        PlayingIconRole,
        SubheaderRole,
        TrackIndexRole,
        HeaderRole,
    };

    using TreeModel::TreeModel;

    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    [[nodiscard]] QStringList mimeTypes() const override;
    [[nodiscard]] QMimeData* mimeData(const QModelIndexList& indexes) const override;

    [[nodiscard]] const TrackList& tracks() const;
    [[nodiscard]] int trackIndex(const QModelIndex& index) const;
    [[nodiscard]] int firstTrack(const QModelIndex& index) const;
    [[nodiscard]] QModelIndex indexForTrack(int track) const;
    [[nodiscard]] std::set<int> selectedTracks(const QModelIndexList& indexes) const;
    [[nodiscard]] bool changesStructure(const ExpansionResult& result) const;

    void setContent(const ExpansionResult& result);
    bool updateContent(const ExpansionResult& result);

private:
    void rebuildItems();

    [[nodiscard]] const ExpansionTrackItem* item(const QModelIndex& index) const;
    ExpansionTrackItem* appendItem(ExpansionTrackItem* parent, int track, int subheader, int firstTrack,
                                   int trackCount);
    bool updateRow(ExpansionRow& previous, const ExpansionRow& row, const QModelIndex& index);
    static QSize rowSize(const ExpansionRow& row);
    static ExpansionRow headerRow(const RichTextLayout& layout);

    TrackList m_tracks;
    ExpansionRow m_header;
    ExpansionTrackItem* m_headerItem{nullptr};
    std::vector<ExpansionSubheader> m_subheaders;
    std::vector<ExpansionTrackItem*> m_subheaderItems;
    std::vector<ExpansionRow> m_rows;
    std::vector<ExpansionTrackItem> m_items;
    std::vector<ExpansionTrackItem*> m_trackItems;
};
} // namespace Fooyin::Filters
