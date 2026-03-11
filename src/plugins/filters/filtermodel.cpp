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
#include "filterpopulator.h"

#include <core/coresettings.h>
#include <core/library/musiclibrary.h>
#include <core/library/tracksort.h>
#include <core/track.h>
#include <gui/coverprovider.h>
#include <gui/guiconstants.h>
#include <gui/widgets/autoheaderview.h>
#include <utils/datastream.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QIODevice>
#include <QMimeData>
#include <QSize>
#include <QThread>

#include <set>
#include <utility>

namespace {
QByteArray saveTracks(const QModelIndexList& indexes)
{
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);

    Fooyin::TrackIds trackIds;
    trackIds.reserve(indexes.size());

    for(const QModelIndex& index : indexes) {
        const auto ids = index.data(Fooyin::Filters::FilterItem::TrackIdsRole).value<Fooyin::TrackIds>();
        std::ranges::copy(ids, std::back_inserter(trackIds));
    }

    Fooyin::operator<<(stream, trackIds);

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
    explicit FilterModelPrivate(FilterModel* self, LibraryManager* libraryManager, MusicLibrary* library,
                                CoverProvider* coverProvider, SettingsManager* settings);

    void beginReset();

    void addSummary();
    void removeSummary();
    void updateSummary();

    void batchFinished(PendingTreeData data);
    void populateModel(PendingTreeData& data);

    void coverUpdated(const Track& track);
    void dataUpdated(const QList<int>& roles = {}) const;

    [[nodiscard]] QStringList columnFields() const;
    void runPopulator(const TrackList& tracks);

    [[nodiscard]] TrackList tracksForIds(const TrackIds& ids) const;
    [[nodiscard]] Track trackForId(int id) const;

    FilterModel* m_self;
    SettingsManager* m_settings;
    MusicLibrary* m_library;
    TrackSorter m_sorter;

    bool m_resetting{false};
    QThread m_populatorThread;
    FilterPopulator m_populator;
    CoverProvider* m_coverProvider;

    FilterItem m_summaryNode;
    ItemKeyMap m_nodes;
    TrackIdNodeMap m_trackParents;

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

    TrackList m_tracksPendingRemoval;
};

FilterModelPrivate::FilterModelPrivate(FilterModel* self, LibraryManager* libraryManager, MusicLibrary* library,
                                       CoverProvider* coverProvider, SettingsManager* settings)
    : m_self{self}
    , m_settings{settings}
    , m_library{library}
    , m_sorter{libraryManager}
    , m_populator{libraryManager}
    , m_coverProvider{coverProvider}
    , m_decorationSize{
          CoverProvider::findThumbnailSize(m_settings->fileValue(u"Filters/IconSize", QSize{100, 100}).toSize())}
{
    m_populator.moveToThread(&m_populatorThread);

    QObject::connect(m_coverProvider, &CoverProvider::coverAdded, m_self,
                     [this](const Track& track) { coverUpdated(track); });
}

void FilterModelPrivate::beginReset()
{
    m_self->resetRoot();
    m_nodes.clear();
    m_trackParents.clear();

    if(m_showSummary) {
        addSummary();
        updateSummary();
    }
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
        const QString summaryText = QString{FilterModel::tr("All (%1 %2s)")}
                                        .arg(static_cast<int>(uniqueColumns.at(column).size()))
                                        .arg(m_columns.at(column).name.toLower());
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

void FilterModelPrivate::batchFinished(PendingTreeData data)
{
    if(m_nodes.empty()) {
        m_resetting = true;
    }

    if(m_resetting) {
        m_self->beginResetModel();
        beginReset();
    }

    if(!m_tracksPendingRemoval.empty()) {
        m_self->removeTracks(m_tracksPendingRemoval);
    }

    populateModel(data);

    if(m_resetting) {
        m_self->endResetModel();
    }
    m_resetting = false;

    QMetaObject::invokeMethod(m_self, &FilterModel::modelUpdated);
    m_self->invalidateData();
}

void FilterModelPrivate::populateModel(PendingTreeData& data)
{
    const QString sortScript    = m_settings->value<Settings::Core::LibrarySortScript>();
    const auto parsedSortScript = sortScript.isEmpty() ? ParsedScript{} : m_sorter.parseSortScript(sortScript);

    std::vector<FilterItem> newItems;
    newItems.reserve(data.items.size());

    for(auto& [key, item] : data.items) {
        if(m_nodes.contains(key)) {
            auto& node = m_nodes.at(key);
            node.addTracks(item.trackIds());
            if(!sortScript.isEmpty()) {
                node.sortTracks(m_library, m_sorter, parsedSortScript);
            }
        }
        else {
            newItems.push_back(std::move(item));
        }
    }

    if(!newItems.empty()) {
        auto* parent   = m_self->rootItem();
        const int row  = parent->childCount();
        const int last = row + static_cast<int>(newItems.size()) - 1;

        if(!m_resetting) {
            m_self->beginInsertRows({}, row, last);
        }

        for(auto& item : newItems) {
            FilterItem* child = &m_nodes.emplace(item.key(), std::move(item)).first->second;
            parent->appendChild(child);
        }

        if(!m_resetting) {
            m_self->endInsertRows();
        }
    }

    m_trackParents.merge(data.trackParents);

    updateSummary();
}

void FilterModelPrivate::coverUpdated(const Track& track)
{
    if(!m_trackParents.contains(track.id())) {
        return;
    }

    const auto parents = m_trackParents.at(track.id());

    for(const auto& parentKey : parents) {
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

QStringList FilterModelPrivate::columnFields() const
{
    QStringList fields;
    fields.reserve(m_columns.size());
    std::ranges::transform(m_columns, std::back_inserter(fields), [](const auto& column) { return column.field; });
    return fields;
}

void FilterModelPrivate::runPopulator(const TrackList& tracks)
{
    m_populatorThread.start();

    QMetaObject::invokeMethod(&m_populator, [this, fields = columnFields(), tracks] {
        m_populator.run(fields, tracks, m_settings->value<Settings::Core::UseVariousForCompilations>());
    });
}

TrackList FilterModelPrivate::tracksForIds(const TrackIds& ids) const
{
    return m_library ? m_library->tracksForIds(ids) : TrackList{};
}

Track FilterModelPrivate::trackForId(int id) const
{
    return m_library ? m_library->trackForId(id) : Track{};
}

FilterModel::FilterModel(LibraryManager* libraryManager, MusicLibrary* library, CoverProvider* coverProvider,
                         SettingsManager* settings, QObject* parent)
    : TreeModel{parent}
    , p{std::make_unique<FilterModelPrivate>(this, libraryManager, library, coverProvider, settings)}
{
    qRegisterMetaType<PendingTreeDataPtr>("Fooyin::Filters::PendingTreeDataPtr");

    QObject::connect(&p->m_populator, &FilterPopulator::populated, this, [this](const PendingTreeDataPtr& data) {
        if(data) {
            p->batchFinished(*data);
        }
    });

    QObject::connect(&p->m_populator, &Worker::finished, this, [this]() {
        p->m_populator.stopThread();
        p->m_populatorThread.quit();
    });
}

FilterModel::~FilterModel()
{
    p->m_populator.stopThread();
    p->m_populatorThread.quit();
    p->m_populatorThread.wait();
}

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

    p->m_decorationSize = CoverProvider::findThumbnailSize(size);
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
        return (Qt::AlignHCenter);
    }

    if(orientation == Qt::Orientation::Vertical) {
        return {};
    }

    if(role == AutoHeaderView::SectionAlignment) {
        return columnAlignment(section).toInt();
    }

    if(role == Qt::DisplayRole) {
        if(section < 0 || section >= static_cast<int>(p->m_columns.size())) {
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
    mimeData->setData(QString::fromLatin1(Constants::Mime::TrackIds), saveTracks(indexes));
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
    for(const auto& child : rows) {
        if(uniqueKeys.contains(child->key())) {
            indexes.append(indexOfItem(child));
        }
    }

    return indexes;
}

void FilterModel::addTracks(const TrackList& tracks)
{
    TrackList tracksToAdd;
    std::ranges::copy_if(tracks, std::back_inserter(tracksToAdd),
                         [this](const Track& track) { return !p->m_trackParents.contains(track.id()); });

    if(tracksToAdd.empty()) {
        return;
    }

    p->runPopulator(tracksToAdd);
}

void FilterModel::updateTracks(const TrackList& tracks)
{
    TrackList tracksToUpdate;
    std::ranges::copy_if(tracks, std::back_inserter(tracksToUpdate),
                         [this](const Track& track) { return p->m_trackParents.contains(track.id()); });

    if(tracksToUpdate.empty()) {
        emit modelUpdated();
        addTracks(tracks);
        return;
    }

    p->m_tracksPendingRemoval = tracksToUpdate;

    p->runPopulator(tracksToUpdate);

    addTracks(tracks);
}

void FilterModel::refreshTracks(const TrackList& tracks)
{
    for(const Track& track : tracks) {
        if(!p->m_trackParents.contains(track.id())) {
            continue;
        }
    }

    if(!tracks.empty()) {
        p->dataUpdated({FilterItem::Tracks, Qt::DecorationRole});
    }
}

void FilterModel::removeTracks(const TrackList& tracks)
{
    std::set<FilterItem*> items;

    for(const Track& track : tracks) {
        const int id = track.id();
        if(p->m_trackParents.contains(id)) {
            const auto trackNodes = p->m_trackParents[id];
            for(const auto& node : trackNodes) {
                FilterItem* item = &p->m_nodes[node];
                item->removeTrack(track);
                items.emplace(item);
            }
            p->m_trackParents.erase(id);
        }
    }

    std::vector<std::pair<int, Md5Hash>> rowsToRemove;
    rowsToRemove.reserve(items.size());

    auto* parent = rootItem();

    for(FilterItem* item : items) {
        if(item->trackCount() == 0) {
            rowsToRemove.emplace_back(item->row(), item->key());
        }
    }

    std::ranges::sort(rowsToRemove, std::greater{}, &std::pair<int, Md5Hash>::first);

    for(const auto& [row, key] : rowsToRemove) {
        beginRemoveRows({}, row, row);
        parent->removeChild(row);
        endRemoveRows();

        p->m_nodes.erase(key);
    }

    if(!rowsToRemove.empty()) {
        parent->resetChildren();
    }

    p->updateSummary();
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

    endRemoveColumns();

    return true;
}

void FilterModel::reset(const FilterColumnList& columns, const TrackList& tracks)
{
    if(p->m_populatorThread.isRunning()) {
        p->m_populator.stopThread();
    }
    else {
        p->m_populatorThread.start();
    }

    p->m_columns = columns;

    if(tracks.empty()) {
        beginResetModel();
        p->beginReset();
        endResetModel();
        return;
    }

    p->m_resetting = true;
    p->runPopulator(tracks);
}
} // namespace Fooyin::Filters
