/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "queueviewermodel.h"

#include "internalguisettings.h"

#include <core/player/playbackqueue.h>
#include <core/player/playercontroller.h>
#include <gui/guiconstants.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QIODevice>
#include <QMimeData>

constexpr auto ViewerItems = "application/x-fooyin-queuevieweritems";

namespace {
QByteArray saveIndexes(const QModelIndexList& indexes)
{
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);

    for(const QModelIndex& index : indexes) {
        if(!index.isValid()) {
            continue;
        }
        stream << index.row();
    }
    return result;
}

QModelIndexList restoreIndexes(QAbstractItemModel* model, QByteArray data)
{
    QModelIndexList result;
    QDataStream stream(&data, QIODevice::ReadOnly);

    while(!stream.atEnd()) {
        int row{-1};
        stream >> row;
        if(row >= 0) {
            result.emplace_back(model->index(row, 0, {}));
        }
    }
    return result;
}

bool cmpTrackIndices(const QModelIndex& index1, const QModelIndex& index2)
{
    QModelIndex item1{index1};
    QModelIndex item2{index2};

    QModelIndexList item1Parents;
    QModelIndexList item2Parents;
    const QModelIndex root;

    while(item1.parent() != item2.parent()) {
        if(item1.parent() != root) {
            item1Parents.push_back(item1);
            item1 = item1.parent();
        }
        if(item2.parent() != root) {
            item2Parents.push_back(item2);
            item2 = item2.parent();
        }
    }
    if(item1.row() == item2.row()) {
        return item1Parents.size() < item2Parents.size();
    }
    return item1.row() < item2.row();
}

struct cmpIndexes
{
    bool operator()(const QModelIndex& index1, const QModelIndex& index2) const
    {
        return cmpTrackIndices(index1, index2);
    }
};

using IndexRangeList = std::vector<QModelIndexList>;

IndexRangeList determineIndexGroups(const QModelIndexList& indexes)
{
    IndexRangeList indexGroups;

    QModelIndexList sortedIndexes{indexes};
    std::ranges::sort(sortedIndexes, cmpTrackIndices);

    auto startOfSequence = sortedIndexes.cbegin();
    while(startOfSequence != sortedIndexes.cend()) {
        auto endOfSequence
            = std::adjacent_find(startOfSequence, sortedIndexes.cend(), [](const auto& lhs, const auto& rhs) {
                  return lhs.parent() != rhs.parent() || rhs.row() != lhs.row() + 1;
              });
        if(endOfSequence != sortedIndexes.cend()) {
            std::advance(endOfSequence, 1);
        }

        indexGroups.emplace_back(startOfSequence, endOfSequence);

        startOfSequence = endOfSequence;
    }

    return indexGroups;
}
} // namespace

namespace Fooyin {
QueueViewerModel::QueueViewerModel(std::shared_ptr<TagLoader> tagLoader, SettingsManager* settings, QObject* parent)
    : TreeModel{parent}
    , m_settings{settings}
    , m_coverProvider{std::move(tagLoader), settings}
    , m_showIcon{true}
{
    QObject::connect(&m_coverProvider, &CoverProvider::coverAdded, this, [this](const Track& track) {
        if(m_trackParents.contains(track.albumHash())) {
            const auto items = m_trackParents.at(track.albumHash());
            for(QueueViewerItem* item : items) {
                const QModelIndex index = indexOfItem(item);
                emit dataChanged(index, index, {Qt::DecorationRole});
            }
        }
    });

    m_settings->subscribe<Settings::Gui::Internal::QueueViewerLeftScript>(this, &QueueViewerModel::regenerateTitles);
    m_settings->subscribe<Settings::Gui::Internal::QueueViewerRightScript>(this, &QueueViewerModel::regenerateTitles);
}

Qt::ItemFlags QueueViewerModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags flags = QAbstractItemModel::flags(index);

    if(index.isValid()) {
        flags |= Qt::ItemIsDragEnabled | Qt::ItemNeverHasChildren;
    }
    else {
        flags |= Qt::ItemIsDropEnabled;
    }

    return flags;
}

bool QueueViewerModel::hasChildren(const QModelIndex& parent) const
{
    return !parent.isValid();
}

QVariant QueueViewerModel::headerData(int /*section*/, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Orientation::Vertical) {
        return {};
    }

    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole) {
        return {};
    }

    return tr("Playback Queue");
}

QVariant QueueViewerModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = itemForIndex(index);

    switch(role) {
        case(Qt::DisplayRole):
            return item->title();
            break;
        case(QueueViewerItem::RightText):
            return item->subtitle();
            break;
        case(Qt::DecorationRole):
            if(m_showIcon) {
                return m_coverProvider.trackCoverThumbnail(item->track().track);
            }
            break;
        case(QueueViewerItem::Track):
            return QVariant::fromValue(item->track());
        default:
            break;
    }

    return {};
}

QStringList QueueViewerModel::mimeTypes() const
{
    return {QString::fromLatin1(ViewerItems), QString::fromLatin1(Constants::Mime::TrackIds),
            QString::fromLatin1(Constants::Mime::QueueTracks)};
}

bool QueueViewerModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                       const QModelIndex& parent) const
{
    if(action == Qt::MoveAction && data->hasFormat(QString::fromLatin1(ViewerItems))) {
        return true;
    }
    if((action == Qt::CopyAction || action == Qt::MoveAction)
       && (data->hasFormat(QString::fromLatin1(Constants::Mime::TrackIds))
           || data->hasFormat(QString::fromLatin1(Constants::Mime::QueueTracks)))) {
        return true;
    }

    return QAbstractItemModel::canDropMimeData(data, action, row, column, parent);
}

Qt::DropActions QueueViewerModel::supportedDropActions() const
{
    return Qt::MoveAction | Qt::CopyAction;
}

Qt::DropActions QueueViewerModel::supportedDragActions() const
{
    return Qt::MoveAction;
}

QMimeData* QueueViewerModel::mimeData(const QModelIndexList& indexes) const
{
    auto* mimeData = new QMimeData();
    mimeData->setData(QString::fromLatin1(ViewerItems), saveIndexes(indexes));
    return mimeData;
}

bool QueueViewerModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                    const QModelIndex& parent)
{
    if(!canDropMimeData(data, action, row, column, parent)) {
        return false;
    }

    if(data->hasFormat(QString::fromLatin1(ViewerItems))) {
        const QModelIndexList indexes = restoreIndexes(this, data->data(QString::fromLatin1(ViewerItems)));
        if(indexes.empty()) {
            return false;
        }
        moveTracks(row, indexes);
    }
    else if(data->hasFormat(QString::fromLatin1(Constants::Mime::QueueTracks))) {
        emit playlistTracksDropped(row, data->data(QString::fromLatin1(Constants::Mime::QueueTracks)));
    }
    else if(data->hasFormat(QString::fromLatin1(Constants::Mime::TrackIds))) {
        emit tracksDropped(row, data->data(QString::fromLatin1(Constants::Mime::TrackIds)));
    }

    emit queueChanged();
    return true;
}

void QueueViewerModel::insertTracks(int row, const QueueTracks& tracks)
{
    const auto titleScript    = m_settings->value<Settings::Gui::Internal::QueueViewerLeftScript>();
    const auto subtitleScript = m_settings->value<Settings::Gui::Internal::QueueViewerRightScript>();

    int first      = row < 0 ? rowCount({}) : row;
    const int last = first + static_cast<int>(tracks.size()) - 1;

    beginInsertRows({}, first, last);

    for(const auto& track : tracks) {
        m_trackItems.insert(m_trackItems.begin() + first, std::make_unique<QueueViewerItem>(track));
        auto* item = m_trackItems.at(first).get();

        item->generateTitle(&m_scriptParser, titleScript, subtitleScript);
        rootItem()->insertChild(first, item);
        m_trackParents[track.track.albumHash()].emplace_back(item);

        ++first;
    }

    endInsertRows();
}

void QueueViewerModel::addTracks(const QueueTracks& tracks)
{
    const auto titleScript    = m_settings->value<Settings::Gui::Internal::QueueViewerLeftScript>();
    const auto subtitleScript = m_settings->value<Settings::Gui::Internal::QueueViewerRightScript>();

    const int first = rowCount({});
    const int last  = first + static_cast<int>(tracks.size()) - 1;

    beginInsertRows({}, first, last);

    for(const auto& track : tracks) {
        auto* item = m_trackItems.emplace_back(std::make_unique<QueueViewerItem>(track)).get();
        item->generateTitle(&m_scriptParser, titleScript, subtitleScript);
        rootItem()->appendChild(item);
        m_trackParents[track.track.albumHash()].emplace_back(item);
    }

    endInsertRows();
}

void QueueViewerModel::removeTracks(const QueueTracks& tracks)
{
    std::vector<int> indexes;

    for(const auto& track : tracks) {
        auto itemIt = std::ranges::find_if(m_trackItems, [track](const auto& item) { return item->track() == track; });
        if(itemIt != m_trackItems.cend()) {
            indexes.emplace_back(itemIt->get()->row());
            if(m_trackParents.contains(track.track.albumHash())) {
                std::erase_if(m_trackParents.at(track.track.albumHash()),
                              [&itemIt](const auto& item) { return item == itemIt->get(); });
            }
        }
    }

    std::ranges::sort(indexes);

    auto startOfSequence = indexes.cbegin();
    while(startOfSequence != indexes.cend()) {
        auto endOfSequence
            = std::adjacent_find(startOfSequence, indexes.cend(), [](int a, int b) { return b != a + 1; });
        if(endOfSequence != indexes.cend()) {
            std::advance(endOfSequence, 1);
        }

        const int firstRow = *startOfSequence;
        int lastRow        = *std::prev(endOfSequence);

        beginRemoveRows({}, firstRow, lastRow);
        while(lastRow >= firstRow) {
            rootItem()->removeChild(lastRow);
            m_trackItems.erase(m_trackItems.begin() + lastRow);
            --lastRow;
        }
        endRemoveRows();

        startOfSequence = endOfSequence;

        rootItem()->resetChildren();
    }
}

void QueueViewerModel::removeIndexes(const std::vector<int>& indexes)
{
    auto startOfSequence = indexes.cbegin();
    while(startOfSequence != indexes.cend()) {
        auto endOfSequence
            = std::adjacent_find(startOfSequence, indexes.cend(), [](int a, int b) { return b != a + 1; });
        if(endOfSequence != indexes.cend()) {
            std::advance(endOfSequence, 1);
        }

        const int firstRow = *startOfSequence;
        int lastRow        = *std::prev(endOfSequence);

        beginRemoveRows({}, firstRow, lastRow);
        while(lastRow >= firstRow) {
            auto* child      = rootItem()->child(lastRow);
            const auto track = child->track().track;
            if(m_trackParents.contains(track.albumHash())) {
                std::erase_if(m_trackParents.at(track.albumHash()),
                              [&child](const auto& item) { return item == child; });
            }
            rootItem()->removeChild(lastRow);
            m_trackItems.erase(m_trackItems.begin() + lastRow);
            --lastRow;
        }
        endRemoveRows();

        startOfSequence = endOfSequence;

        rootItem()->resetChildren();
    }
}

void QueueViewerModel::reset(const QueueTracks& tracks)
{
    const auto titleScript    = m_settings->value<Settings::Gui::Internal::QueueViewerLeftScript>();
    const auto subtitleScript = m_settings->value<Settings::Gui::Internal::QueueViewerRightScript>();

    beginResetModel();
    resetRoot();

    for(const auto& track : tracks) {
        auto* item = m_trackItems.emplace_back(std::make_unique<QueueViewerItem>(track)).get();
        item->generateTitle(&m_scriptParser, titleScript, subtitleScript);
        rootItem()->appendChild(item);
        m_trackParents[track.track.albumHash()].emplace_back(item);
    }

    endResetModel();
}

QueueTracks QueueViewerModel::queueTracks() const
{
    QueueTracks tracks;

    for(const auto& item : m_trackItems) {
        tracks.emplace_back(item->track());
    }

    return tracks;
}

void QueueViewerModel::setShowIcon(bool show)
{
    m_showIcon = show;
    emit dataChanged({}, {}, {Qt::DecorationRole});
}

void QueueViewerModel::regenerateTitles()
{
    const auto titleScript    = m_settings->value<Settings::Gui::Internal::QueueViewerLeftScript>();
    const auto subtitleScript = m_settings->value<Settings::Gui::Internal::QueueViewerRightScript>();

    for(const auto& item : m_trackItems) {
        item->generateTitle(&m_scriptParser, titleScript, subtitleScript);
    }
    emit dataChanged({}, {}, {Qt::DisplayRole});
}

void QueueViewerModel::moveTracks(int row, const QModelIndexList& indexes)
{
    const auto indexGroups = determineIndexGroups(indexes);

    if(row < 0) {
        row = rowCount({});
    }

    auto* root = rootItem();

    for(const auto& group : indexGroups) {
        const auto children
            = std::views::transform(group, [this](const QModelIndex& index) { return itemForIndex(index); });

        int currRow{row};
        const int firstRow = children.front()->row();
        const int lastRow  = static_cast<int>(firstRow + children.size()) - 1;

        if(currRow >= firstRow && currRow <= lastRow + 1) {
            continue;
        }

        beginMoveRows({}, firstRow, lastRow, {}, row);

        for(auto* childItem : children) {
            childItem->resetRow();
            const int oldRow = childItem->row();
            root->moveChild(oldRow, currRow);

            if(oldRow < currRow) {
                Utils::move(m_trackItems, oldRow, currRow - 1);
            }
            else {
                Utils::move(m_trackItems, oldRow, currRow);
                ++currRow;
            }
        }

        root->resetChildren();

        endMoveRows();

        row = currRow;
    }
}
} // namespace Fooyin
