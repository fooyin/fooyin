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

#include "filtermodel.h"

#include "filterfwd.h"
#include "filteritem.h"

#include <core/library/musiclibrary.h>
#include <core/track.h>
#include <gui/coverprovider.h>
#include <gui/guiconstants.h>
#include <gui/guiutils.h>
#include <gui/widgets/autoheaderview.h>
#include <utils/datastream.h>
#include <utils/settings/settingsmanager.h>

#include <QIODevice>
#include <QMimeData>
#include <QSize>

#include <set>
#include <utility>

namespace {
QByteArray saveTracks(Fooyin::MusicLibrary* library, Fooyin::SettingsManager* settings, const QModelIndexList& indexes)
{
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);

    Fooyin::TrackIds trackIds;
    trackIds.reserve(indexes.size());

    for(const QModelIndex& index : indexes) {
        const auto ids = index.data(Fooyin::Filters::FilterItem::TrackIdsRole).value<Fooyin::TrackIds>();
        std::ranges::copy(ids, std::back_inserter(trackIds));
    }

    stream << Fooyin::Gui::sortTrackIdsForLibraryViewerPlaylist(library, settings, trackIds);

    return result;
}

Fooyin::Filters::FilterItem* filterItem(const QModelIndex& index)
{
    return static_cast<Fooyin::Filters::FilterItem*>(index.internalPointer());
}

Fooyin::RichText summaryRichText(const QString& text, const std::vector<Fooyin::RichTextBlock>& blocks)
{
    Fooyin::RichText richText;
    if(text.isEmpty()) {
        return richText;
    }

    Fooyin::RichTextBlock block;
    block.text = text;

    if(!blocks.empty()) {
        block.format = blocks.front().format;
    }
    richText.blocks.push_back(std::move(block));

    return richText;
}

bool rowMatchesItem(const Fooyin::Filters::FilterRow& row, const Fooyin::Filters::FilterItem& item)
{
    if(row.columns != item.columns() || row.trackIds != item.trackIds()) {
        return false;
    }

    std::vector<Fooyin::RichText> richColumns;
    richColumns.reserve(item.columns().size());

    for(int column = 0; column < item.columns().size(); ++column) {
        richColumns.push_back(item.richColumn(column));
    }

    return row.richColumns == richColumns;
}
} // namespace

namespace Fooyin::Filters {
FilterSortModel::FilterSortModel(QObject* parent)
    : QSortFilterProxyModel{parent}
{ }

bool FilterSortModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const auto* leftItem  = filterItem(left);
    const auto* rightItem = filterItem(right);

    if(!leftItem || !rightItem) {
        return QSortFilterProxyModel::lessThan(left, right);
    }

    if(leftItem->isSummary() && !rightItem->isSummary()) {
        return sortOrder() == Qt::AscendingOrder;
    }
    if(!leftItem->isSummary() && rightItem->isSummary()) {
        return sortOrder() != Qt::AscendingOrder;
    }

    const auto cmp = m_collator.compare(leftItem->column(left.column()), rightItem->column(right.column()));

    if(cmp == 0) {
        return false;
    }

    return cmp < 0;
}

class FilterModelPrivate
{
public:
    explicit FilterModelPrivate(FilterModel* self, MusicLibrary* library, CoverProvider* coverProvider,
                                SettingsManager* settings);

    void addSummary();
    void removeSummary();
    void updateSummary();
    void coverUpdated(const Track& track);
    void dataUpdated(const QList<int>& roles = {}) const;
    [[nodiscard]] bool hasSummaryItem() const;
    [[nodiscard]] int rowOffset() const;
    void rebuildTrackParents();
    [[nodiscard]] TrackList tracksForIds(const TrackIds& ids) const;
    [[nodiscard]] Track trackForId(int id) const;

    FilterModel* m_self;
    SettingsManager* m_settings;
    MusicLibrary* m_library;
    CoverProvider* m_coverProvider;

    FilterItem m_summaryNode;
    std::map<RowKey, FilterItem> m_nodes;
    std::unordered_map<int, std::vector<RowKey>> m_trackParents;

    FilterColumnList m_columns;
    bool m_showDecoration{false};
    CoverProvider::ThumbnailSize m_decorationSize;
    bool m_showLabels{true};
    Track::Cover m_coverType{Track::Cover::Front};
    std::vector<int> m_columnOrder;

    using ColumnAlignments = std::vector<Qt::Alignment>;
    mutable ColumnAlignments m_columnAlignments;

    bool m_showSummary{true};
    int m_rowHeight{0};
};

FilterModelPrivate::FilterModelPrivate(FilterModel* self, MusicLibrary* library, CoverProvider* coverProvider,
                                       SettingsManager* settings)
    : m_self{self}
    , m_settings{settings}
    , m_library{library}
    , m_coverProvider{coverProvider}
    , m_decorationSize{
          CoverProvider::findThumbnailSize(m_settings->fileValue(u"Filters/IconSize", QSize{100, 100}).toSize())}
{
    QObject::connect(m_coverProvider, &CoverProvider::coverAdded, m_self,
                     [this](const Track& track) { coverUpdated(track); });
}

void FilterModelPrivate::addSummary()
{
    m_summaryNode = FilterItem{{}, {}, m_self->rootItem()};
    m_summaryNode.setIsSummary(true);
    m_self->rootItem()->insertChild(0, &m_summaryNode);
}

void FilterModelPrivate::removeSummary()
{
    const int row = m_summaryNode.row();
    m_self->rootItem()->removeChild(row);
    m_summaryNode = {};
}

void FilterModelPrivate::updateSummary()
{
    if(!m_showSummary) {
        return;
    }

    const int columnCount = m_self->columnCount({});
    const auto children   = m_self->rootItem()->children();

    QStringList nodeColumns;
    std::vector<RichText> richColumns;
    richColumns.reserve(columnCount);

    std::vector<std::set<QString>> uniqueColumns(columnCount);
    std::vector<const std::vector<RichTextBlock>*> richTemplates(columnCount, nullptr);

    for(FilterItem* item : children) {
        if(!item || item->isSummary()) {
            continue;
        }

        for(int column{0}; column < columnCount; ++column) {
            uniqueColumns.at(column).emplace(item->column(column));
            if(!richTemplates.at(column) && !item->richColumn(column).empty()) {
                richTemplates.at(column) = &item->richColumn(column).blocks;
            }
        }
    }

    for(int column{0}; column < columnCount; ++column) {
        const QString summaryText = FilterModel::tr("All (%L1)").arg(static_cast<int>(uniqueColumns.at(column).size()));
        nodeColumns.emplace_back(summaryText);

        if(richTemplates.at(column)) {
            richColumns.push_back(summaryRichText(summaryText, *richTemplates.at(column)));
        }
        else {
            richColumns.push_back(summaryRichText(summaryText, {}));
        }
    }

    m_summaryNode.setColumns(nodeColumns);
    m_summaryNode.setRichColumns(richColumns);
}

void FilterModelPrivate::coverUpdated(const Track& track)
{
    if(!m_trackParents.contains(track.id())) {
        return;
    }

    const auto parents = m_trackParents.at(track.id());

    for(const RowKey& parentKey : parents) {
        if(m_nodes.contains(parentKey)) {
            auto* parentItem = &m_nodes.at(parentKey);

            const QModelIndex nodeIndex = m_self->indexOfItem(parentItem);
            emit m_self->dataChanged(nodeIndex, nodeIndex, {Qt::DecorationRole});
        }
    }
}

void FilterModelPrivate::dataUpdated(const QList<int>& roles) const
{
    const int rowCount    = m_self->rowCount({});
    const int columnCount = m_self->columnCount({});
    if(rowCount <= 0 || columnCount <= 0) {
        return;
    }

    const QModelIndex topLeft     = m_self->index(0, 0, {});
    const QModelIndex bottomRight = m_self->index(rowCount - 1, columnCount - 1, {});
    emit m_self->dataChanged(topLeft, bottomRight, roles);
}

bool FilterModelPrivate::hasSummaryItem() const
{
    return m_showSummary && m_self->rootItem()->childCount() > 0 && m_self->rootItem()->child(0) == &m_summaryNode;
}

int FilterModelPrivate::rowOffset() const
{
    return hasSummaryItem() ? 1 : 0;
}

void FilterModelPrivate::rebuildTrackParents()
{
    m_trackParents.clear();

    const int offset = rowOffset();
    for(int row{offset}; row < m_self->rootItem()->childCount(); ++row) {
        const auto* item = m_self->rootItem()->child(row);
        if(!item || item->isSummary()) {
            continue;
        }

        for(const int trackId : item->trackIds()) {
            m_trackParents[trackId].push_back(item->key());
        }
    }
}

TrackList FilterModelPrivate::tracksForIds(const TrackIds& ids) const
{
    return m_library ? m_library->tracksForIds(ids) : TrackList{};
}

Track FilterModelPrivate::trackForId(int id) const
{
    return m_library ? m_library->trackForId(id) : Track{};
}

FilterModel::FilterModel(MusicLibrary* library, CoverProvider* coverProvider, SettingsManager* settings,
                         QObject* parent)
    : TreeModel{parent}
    , p{std::make_unique<FilterModelPrivate>(this, library, coverProvider, settings)}
{ }

FilterModel::~FilterModel() = default;

bool FilterModel::showSummary() const
{
    return p->m_showSummary;
}

Track::Cover FilterModel::coverType() const
{
    return p->m_coverType;
}

void FilterModel::setRowHeight(int height)
{
    p->m_rowHeight = height;
    p->dataUpdated();
}

void FilterModel::setIconSize(const QSize& size)
{
    if(!size.isValid()) {
        return;
    }

    const auto thumbnailSize = CoverProvider::findThumbnailSize(size);
    if(std::exchange(p->m_decorationSize, thumbnailSize) == thumbnailSize) {
        return;
    }

    p->dataUpdated({Qt::DecorationRole});
}

void FilterModel::setShowSummary(bool show)
{
    const bool prev = std::exchange(p->m_showSummary, show);
    if(prev == show) {
        return;
    }

    if(show) {
        beginInsertRows({}, 0, 0);
        p->addSummary();
        p->updateSummary();
        endInsertRows();
    }
    else {
        const int row = p->m_summaryNode.row();
        beginRemoveRows({}, row, row);
        p->removeSummary();
        endRemoveRows();
    }
}

void FilterModel::setShowDecoration(bool show)
{
    if(std::exchange(p->m_showDecoration, show) != show) {
        p->dataUpdated({Qt::DecorationRole, Qt::SizeHintRole});
    }
}

void FilterModel::setShowLabels(bool show)
{
    if(std::exchange(p->m_showLabels, show) != show) {
        p->dataUpdated(
            {Qt::DisplayRole, Qt::ToolTipRole, Qt::SizeHintRole, FilterItem::IconLabel, FilterItem::IconCaptionLines});
    }
}

void FilterModel::setCoverType(Track::Cover type)
{
    if(std::exchange(p->m_coverType, type) != type) {
        p->dataUpdated({Qt::DecorationRole});
    }
}

void FilterModel::setColumnOrder(const std::vector<int>& order)
{
    if(std::exchange(p->m_columnOrder, order) == order) {
        return;
    }

    p->dataUpdated(
        {Qt::DisplayRole, Qt::ToolTipRole, Qt::SizeHintRole, FilterItem::IconLabel, FilterItem::IconCaptionLines});
}

Qt::ItemFlags FilterModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);
    defaultFlags |= Qt::ItemIsDragEnabled;

    return defaultFlags;
}

QVariant FilterModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return Qt::AlignCenter;
    }

    if(orientation == Qt::Orientation::Vertical) {
        return {};
    }

    if(role == AutoHeaderView::SectionAlignment) {
        return columnAlignment(section).toInt();
    }

    if(role == Qt::DisplayRole) {
        if(section < 0 || std::cmp_greater_equal(section, p->m_columns.size())) {
            return tr("Filter");
        }
        return p->m_columns.at(section).name;
    }

    return {};
}

bool FilterModel::setHeaderData(int section, Qt::Orientation /*orientation*/, const QVariant& value, int role)
{
    if(role != AutoHeaderView::SectionAlignment) {
        return false;
    }

    if(section < 0 || section >= columnCount({})) {
        return {};
    }

    changeColumnAlignment(section, value.value<Qt::Alignment>());

    p->dataUpdated({Qt::TextAlignmentRole, FilterItem::IconCaptionLines});

    return true;
}

QVariant FilterModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    const auto* item = itemForIndex(index);
    const int col    = index.column();

    switch(role) {
        case Qt::DisplayRole:
        case Qt::ToolTipRole: {
            if(p->m_showLabels) {
                return item->column(col);
            }
            break;
        }
        case FilterItem::Tracks:
            return QVariant::fromValue(p->tracksForIds(item->trackIds()));
        case FilterItem::TrackIdsRole:
            return QVariant::fromValue(item->trackIds());
        case FilterItem::Key:
            return QVariant::fromValue(item->key());
        case FilterItem::IsSummary:
            return item->isSummary();
        case FilterItem::IconLabel:
            if(p->m_showLabels) {
                return item->iconLabel(p->m_columnOrder);
            }
            break;
        case FilterItem::IconCaptionLines:
            if(p->m_showLabels) {
                return QVariant::fromValue(item->iconCaptionLines(p->m_columnOrder, p->m_columnAlignments));
            }
            break;
        case FilterItem::RichColumn:
            return item->richColumn(col);
        case Qt::DecorationRole:
            if(p->m_showDecoration) {
                if(const int trackId = item->firstTrackId(); trackId >= 0) {
                    return p->m_coverProvider->trackCoverThumbnail(p->trackForId(trackId), p->m_decorationSize,
                                                                   p->m_coverType);
                }
                return p->m_coverProvider->trackCoverThumbnail({}, p->m_decorationSize, p->m_coverType);
            }
            break;
        case Qt::SizeHintRole:
            return QSize{0, p->m_rowHeight};
        case Qt::TextAlignmentRole:
            return QVariant::fromValue(Qt::AlignVCenter | columnAlignment(col));
        default:
            break;
    }

    return {};
}

int FilterModel::columnCount(const QModelIndex& /*parent*/) const
{
    return static_cast<int>(p->m_columns.size());
}

QStringList FilterModel::mimeTypes() const
{
    return {QString::fromLatin1(Constants::Mime::TrackIds)};
}

Qt::DropActions FilterModel::supportedDragActions() const
{
    return Qt::CopyAction;
}

QMimeData* FilterModel::mimeData(const QModelIndexList& indexes) const
{
    auto* mimeData = new QMimeData();
    mimeData->setData(QString::fromLatin1(Constants::Mime::TrackIds), saveTracks(p->m_library, p->m_settings, indexes));
    return mimeData;
}

Qt::Alignment FilterModel::columnAlignment(int column) const
{
    if(column < 0 || std::cmp_greater_equal(column, p->m_columns.size())) {
        return Qt::AlignLeft;
    }

    if(std::cmp_greater_equal(column, p->m_columnAlignments.size())) {
        p->m_columnAlignments.resize(column + 1, Qt::AlignLeft);
    }

    return p->m_columnAlignments.at(column);
}

void FilterModel::changeColumnAlignment(int column, Qt::Alignment alignment)
{
    if(std::cmp_greater_equal(column, p->m_columnAlignments.size())) {
        p->m_columnAlignments.resize(column + 1, Qt::AlignLeft);
    }
    p->m_columnAlignments[column] = alignment;
}

void FilterModel::resetColumnAlignment(int column)
{
    if(column < 0 || std::cmp_greater_equal(column, p->m_columnAlignments.size())) {
        return;
    }

    p->m_columnAlignments.erase(p->m_columnAlignments.begin() + column);
}

void FilterModel::resetColumnAlignments()
{
    p->m_columnAlignments.clear();
}

QModelIndexList FilterModel::indexesForKeys(const std::vector<Md5Hash>& keys) const
{
    QModelIndexList indexes;

    const std::set<Md5Hash> uniqueKeys{keys.cbegin(), keys.cend()};
    const auto rows = rootItem()->children();

    for(const auto* child : rows) {
        if(uniqueKeys.contains(child->key())) {
            indexes.append(indexOfItem(child));
        }
    }

    return indexes;
}

bool FilterModel::removeColumn(int column)
{
    if(column < 0 || std::cmp_greater_equal(column, p->m_columns.size())) {
        return false;
    }

    beginRemoveColumns({}, column, column);

    p->m_columns.erase(p->m_columns.cbegin() + column);
    resetColumnAlignment(column);

    for(auto& [_, node] : p->m_nodes) {
        node.removeColumn(column);
    }

    if(p->m_showSummary) {
        p->m_summaryNode.removeColumn(column);
    }

    endRemoveColumns();

    return true;
}

void FilterModel::setRows(const FilterColumnList& columns, const FilterRowList& rows)
{
    const auto resetRows = [this, &columns, &rows]() {
        beginResetModel();

        p->m_columns = columns;
        resetRoot();
        p->m_nodes.clear();
        p->m_trackParents.clear();

        if(p->m_showSummary) {
            p->addSummary();
        }

        auto* parent = rootItem();

        for(const FilterRow& row : rows) {
            auto [it, _] = p->m_nodes.emplace(row.key, FilterItem{row.key, row.columns, parent});
            auto& item   = it->second;

            item.setRichColumns(row.richColumns);
            item.setTrackIds(row.trackIds);
            parent->appendChild(&item);
        }

        p->rebuildTrackParents();
        p->updateSummary();

        endResetModel();
    };

    if(p->m_columns != columns || rootItem()->childCount() == 0) {
        resetRows();
        return;
    }

    const int rowOffset   = p->rowOffset();
    const int columnCount = static_cast<int>(columns.size());

    std::set<RowKey> desiredKeys;
    for(const FilterRow& row : rows) {
        desiredKeys.emplace(row.key);
    }

    auto* parent = rootItem();

    for(int dataRow = parent->childCount() - rowOffset - 1; dataRow >= 0; --dataRow) {
        auto* item = parent->child(rowOffset + dataRow);
        if(!item || item->isSummary() || desiredKeys.contains(item->key())) {
            continue;
        }

        beginRemoveRows({}, rowOffset + dataRow, rowOffset + dataRow);
        parent->removeChild(rowOffset + dataRow);
        p->m_nodes.erase(item->key());
        endRemoveRows();
    }

    for(int rowIndex{0}; std::cmp_less(rowIndex, rows.size()); ++rowIndex) {
        const FilterRow& row = rows.at(rowIndex);
        auto* item           = parent->child(rowOffset + rowIndex);

        if(item && !item->isSummary() && item->key() == row.key) {
            continue;
        }

        beginInsertRows({}, rowOffset + rowIndex, rowOffset + rowIndex);
        auto [it, _]  = p->m_nodes.emplace(row.key, FilterItem{row.key, row.columns, parent});
        auto& newItem = it->second;
        newItem.setRichColumns(row.richColumns);
        newItem.setTrackIds(row.trackIds);
        parent->insertChild(rowOffset + rowIndex, &newItem);
        endInsertRows();
    }

    parent->resetChildren();

    for(int rowIndex{0}; std::cmp_less(rowIndex, rows.size()); ++rowIndex) {
        const FilterRow& row = rows.at(rowIndex);
        auto* item           = parent->child(rowOffset + rowIndex);

        if(!item || item->isSummary() || item->key() != row.key) {
            resetRows();
            return;
        }

        if(rowMatchesItem(row, *item)) {
            continue;
        }

        item->setColumns(row.columns);
        item->setRichColumns(row.richColumns);
        item->setTrackIds(row.trackIds);

        if(columnCount > 0) {
            const QModelIndex topLeft     = indexOfItem(item);
            const QModelIndex bottomRight = index(topLeft.row(), columnCount - 1, {});
            emit dataChanged(topLeft, bottomRight);
        }
    }

    p->rebuildTrackParents();

    if(p->hasSummaryItem() && columnCount > 0) {
        p->updateSummary();
        parent->resetChildren();

        const QModelIndex topLeft     = indexOfItem(&p->m_summaryNode);
        const QModelIndex bottomRight = index(topLeft.row(), columnCount - 1, {});
        emit dataChanged(topLeft, bottomRight);
    }
}
} // namespace Fooyin::Filters
