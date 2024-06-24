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

#include <core/track.h>
#include <gui/coverprovider.h>
#include <gui/guiconstants.h>
#include <utils/widgets/autoheaderview.h>

#include <QColor>
#include <QFont>
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
        const auto tracks = index.data(Fooyin::Filters::FilterItem::Tracks).value<Fooyin::TrackList>();
        std::ranges::transform(std::as_const(tracks), std::back_inserter(trackIds),
                               [](const Fooyin::Track& track) { return track.id(); });
    }

    stream << trackIds;

    return result;
}
} // namespace

namespace Fooyin::Filters {
struct FilterModel::Private
{
    FilterModel* m_self;

    bool m_resetting{false};

    QThread m_populatorThread;
    FilterPopulator m_populator;
    CoverProvider m_coverProvider;

    FilterItem m_summaryNode;
    ItemKeyMap m_nodes;
    TrackIdNodeMap m_trackParents;

    FilterColumnList m_columns;
    int m_sortColumn{0};
    Qt::SortOrder m_sortOrder{Qt::AscendingOrder};
    bool m_showDecoration{false};

    using ColumnAlignments = std::vector<Qt::Alignment>;
    mutable ColumnAlignments m_columnAlignments;

    bool m_showSummary{true};
    int m_rowHeight{0};
    QFont m_font;
    QColor m_colour;

    TrackList m_tracksPendingRemoval;

    explicit Private(FilterModel* self, SettingsManager* settings)
        : m_self{self}
        , m_coverProvider{settings}
    {
        m_populator.moveToThread(&m_populatorThread);

        QObject::connect(&m_coverProvider, &CoverProvider::coverAdded, m_self,
                         [this](const Track& track) { coverUpdated(track); });
    }

    void beginReset()
    {
        m_self->resetRoot();
        m_nodes.clear();
        m_trackParents.clear();

        if(m_showSummary) {
            addSummary();
        }
    }

    void addSummary()
    {
        m_summaryNode = FilterItem{QStringLiteral(""), {}, m_self->rootItem()};
        m_summaryNode.setIsSummary(true);
        m_self->rootItem()->insertChild(0, &m_summaryNode);
    }

    void removeSummary()
    {
        const int row = m_summaryNode.row();
        m_self->rootItem()->removeChild(row);
        m_summaryNode = {};
    }

    void batchFinished(PendingTreeData data)
    {
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
    }

    int uniqueValues(int column) const
    {
        std::set<QString> columnUniques;

        const auto children = m_self->rootItem()->children();

        for(FilterItem* item : children) {
            if(!item->isSummary()) {
                columnUniques.emplace(item->column(column));
            }
        }

        return static_cast<int>(columnUniques.size());
    }

    void updateSummary()
    {
        if(!m_showSummary) {
            return;
        }

        const int columnCount = m_self->columnCount({});

        QStringList nodeColumns;
        for(int column{0}; column < columnCount; ++column) {
            nodeColumns.emplace_back(
                QString{tr("All (%1 %2s)")}.arg(uniqueValues(column)).arg(m_columns.at(column).name.toLower()));
        }

        m_summaryNode.setColumns(nodeColumns);
    }

    void populateModel(PendingTreeData& data)
    {
        auto* parent = m_self->rootItem();

        for(const auto& [key, item] : data.items) {
            if(m_nodes.contains(key)) {
                m_nodes.at(key).addTracks(item.tracks());
            }
            else {
                if(!m_resetting) {
                    const int row = parent->childCount();
                    m_self->beginInsertRows(m_self->indexOfItem(parent), row, row);
                }

                FilterItem* child = &m_nodes.emplace(key, item).first->second;
                parent->appendChild(child);

                if(!m_resetting) {
                    m_self->endInsertRows();
                }
            }
        }
        m_trackParents.merge(data.trackParents);

        parent->sortChildren(m_sortColumn, m_sortOrder);
        updateSummary();
    }

    void coverUpdated(const Track& track)
    {
        if(!m_trackParents.contains(track.id())) {
            return;
        }

        const auto parents = m_trackParents.at(track.id());

        for(const QString& parentKey : parents) {
            if(m_nodes.contains(parentKey)) {
                auto* parentItem = &m_nodes.at(parentKey);

                const QModelIndex nodeIndex = m_self->indexOfItem(parentItem);
                emit m_self->dataChanged(nodeIndex, nodeIndex, {Qt::DecorationRole});
            }
        }
    }
};

FilterModel::FilterModel(SettingsManager* settings, QObject* parent)
    : TreeModel{parent}
    , p{std::make_unique<Private>(this, settings)}
{
    QObject::connect(&p->m_populator, &FilterPopulator::populated, this,
                     [this](const PendingTreeData& data) { p->batchFinished(data); });

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

int FilterModel::sortColumn() const
{
    return p->m_sortColumn;
};

Qt::SortOrder FilterModel::sortOrder() const
{
    return p->m_sortOrder;
}

void FilterModel::sortOnColumn(int column, Qt::SortOrder order)
{
    p->m_sortColumn = column;
    p->m_sortOrder  = order;

    emit layoutAboutToBeChanged();
    rootItem()->sortChildren(column, order);
    emit layoutChanged();
}

bool FilterModel::showSummary() const
{
    return p->m_showSummary;
}

void FilterModel::setFont(const QString& font)
{
    if(font.isEmpty()) {
        p->m_font = {};
    }
    else {
        p->m_font.fromString(font);
    }

    emit dataChanged({}, {}, {Qt::FontRole, Qt::SizeHintRole});
}

void FilterModel::setColour(const QColor& colour)
{
    p->m_colour = colour;
    emit dataChanged({}, {});
}

void FilterModel::setRowHeight(int height)
{
    p->m_rowHeight = height;
    emit dataChanged({}, {});
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
    p->m_showDecoration = show;
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

    emit dataChanged({}, {}, {Qt::TextAlignmentRole});

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
        case(Qt::DisplayRole):
        case(Qt::ToolTipRole): {
            const QString& name = item->column(col);
            return !name.isEmpty() ? name : QStringLiteral("?");
        }
        case(FilterItem::Tracks):
            return QVariant::fromValue(item->tracks());
        case(FilterItem::IsSummary):
            return item->isSummary();
        case(Qt::DecorationRole):
            if(p->m_showDecoration) {
                if(item->trackCount() > 0) {
                    return p->m_coverProvider.trackCoverThumbnail(item->tracks().front());
                }
                return p->m_coverProvider.trackCoverThumbnail({});
            }
            break;
        case(Qt::SizeHintRole):
            return QSize{0, p->m_rowHeight};
        case(Qt::FontRole):
            return p->m_font;
        case(Qt::ForegroundRole):
            return p->m_colour;
        case(Qt::TextAlignmentRole):
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

QModelIndexList FilterModel::indexesForValues(const QStringList& values, int column) const
{
    QModelIndexList indexes;

    const auto rows = rootItem()->children();
    for(const auto& child : rows) {
        if(values.contains(child->column(column))) {
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

    p->m_populatorThread.start();

    QStringList columns;
    std::ranges::transform(p->m_columns, std::back_inserter(columns), [](const auto& column) { return column.field; });

    QMetaObject::invokeMethod(&p->m_populator,
                              [this, columns, tracksToAdd] { p->m_populator.run(columns, tracksToAdd); });
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

    p->m_populatorThread.start();

    QStringList columns;
    std::ranges::transform(p->m_columns, std::back_inserter(columns), [](const auto& column) { return column.field; });

    QMetaObject::invokeMethod(&p->m_populator,
                              [this, columns, tracksToUpdate] { p->m_populator.run(columns, tracksToUpdate); });

    addTracks(tracks);
}

void FilterModel::refreshTracks(const TrackList& tracks)
{
    for(const Track& track : tracks) {
        if(!p->m_trackParents.contains(track.id())) {
            continue;
        }

        const auto parents = p->m_trackParents.at(track.id());
        for(const auto& parent : parents) {
            if(p->m_nodes.contains(parent)) {
                p->m_nodes.at(parent).replaceTrack(track);
            }
        }
    }
}

void FilterModel::removeTracks(const TrackList& tracks)
{
    std::set<FilterItem*> items;

    for(const Track& track : tracks) {
        const int id = track.id();
        if(p->m_trackParents.contains(id)) {
            const auto trackNodes = p->m_trackParents[id];
            for(const QString& node : trackNodes) {
                FilterItem* item = &p->m_nodes[node];
                item->removeTrack(track);
                items.emplace(item);
            }
            p->m_trackParents.erase(id);
        }
    }

    auto* parent = rootItem();

    for(FilterItem* item : items) {
        if(item->trackCount() == 0) {
            const QModelIndex parentIndex;
            const int row = item->row();
            beginRemoveRows(parentIndex, row, row);
            parent->removeChild(row);
            parent->resetChildren();
            endRemoveRows();
            p->m_nodes.erase(item->key());
        }
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

    p->m_resetting = true;

    QStringList fields;
    std::ranges::transform(p->m_columns, std::back_inserter(fields), [](const auto& column) { return column.field; });

    QMetaObject::invokeMethod(&p->m_populator, [this, fields, tracks] { p->m_populator.run(fields, tracks); });
}
} // namespace Fooyin::Filters
