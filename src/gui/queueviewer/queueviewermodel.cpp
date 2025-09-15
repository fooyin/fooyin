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
#include <utils/modelutils.h>
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
} // namespace

namespace Fooyin {
QueueViewerModel::QueueViewerModel(std::shared_ptr<AudioLoader> audioLoader, PlayerController* playerController,
                                   SettingsManager* settings, QObject* parent)
    : TreeModel{parent}
    , m_playerController{playerController}
    , m_settings{settings}
    , m_coverProvider{std::move(audioLoader), settings}
    , m_showIcon{m_settings->value<Settings::Gui::Internal::QueueViewerShowIcon>()}
    , m_iconSize{CoverProvider::findThumbnailSize(
          m_settings->value<Settings::Gui::Internal::QueueViewerIconSize>().toSize())}
    , m_currentTrackItem{nullptr}
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

    updateShowCurrent();

    m_settings->subscribe<Settings::Gui::Internal::QueueViewerShowCurrent>(this, &QueueViewerModel::updateShowCurrent);
    m_settings->subscribe<Settings::Gui::Internal::QueueViewerLeftScript>(this, &QueueViewerModel::regenerateTitles);
    m_settings->subscribe<Settings::Gui::Internal::QueueViewerRightScript>(this, &QueueViewerModel::regenerateTitles);
    m_settings->subscribe<Settings::Gui::Internal::QueueViewerShowIcon>(this, [this](const bool show) {
        m_showIcon = show;
        invalidateData();
    });
    m_settings->subscribe<Settings::Gui::Internal::QueueViewerIconSize>(this, [this](const auto& size) {
        m_iconSize = CoverProvider::findThumbnailSize(size.toSize());
        invalidateData();
    });
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

    const bool isPlaying = m_playerController->currentIsQueueTrack()
                        && item->track() == m_playerController->currentPlaylistTrack() && index.row() == 0;

    switch(role) {
        case(Qt::DisplayRole):
            return item->title();
        case(QueueViewerItem::RightText):
            return item->subtitle();
        case(Qt::DecorationRole):
            if(isPlaying) {
                switch(m_playerController->playState()) {
                    case(Player::PlayState::Playing):
                        return Utils::pixmapFromTheme(Constants::Icons::Play);
                    case(Player::PlayState::Paused):
                        return Utils::pixmapFromTheme(Constants::Icons::Pause);
                    case(Player::PlayState::Stopped):
                        break;
                }
            }
            if(m_showIcon) {
                return m_coverProvider.trackCoverThumbnail(item->track().track, m_iconSize);
            }
            break;
        case(Qt::TextAlignmentRole):
            return static_cast<int>(Qt::AlignVCenter | Qt::AlignLeft);
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
        return !m_settings->value<Settings::Gui::Internal::QueueViewerShowCurrent>() || row > 0;
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

void QueueViewerModel::insertTracks(const QueueTracks& tracks, int row)
{
    const auto titleScript    = m_settings->value<Settings::Gui::Internal::QueueViewerLeftScript>();
    const auto subtitleScript = m_settings->value<Settings::Gui::Internal::QueueViewerRightScript>();

    int first = row < 0 ? rowCount({}) : row;
    if(m_currentTrackItem) {
        ++first;
    }
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

void QueueViewerModel::removeTracks(const QueueTracks& tracks)
{
    std::vector<int> indexes;

    for(const auto& track : tracks) {
        auto itemIt = std::ranges::find_if(m_trackItems, [track](const auto& item) { return item->track() == track; });
        if(itemIt != m_trackItems.cend()) {
            if(m_trackParents.contains(track.track.albumHash())) {
                std::erase_if(m_trackParents.at(track.track.albumHash()),
                              [&itemIt](const auto& item) { return item == itemIt->get(); });
            }

            itemIt->get()->resetRow();

            indexes.emplace_back(itemIt->get()->row());
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
            if(rootItem()->removeChild(lastRow) && lastRow >= 0 && std::cmp_less(lastRow, m_trackItems.size())) {
                m_trackItems.erase(m_trackItems.begin() + lastRow);
            }
            --lastRow;
        }
        endRemoveRows();

        startOfSequence = endOfSequence;

        rootItem()->resetChildren();
    }

    if(!m_currentTrackItem) {
        updateShowCurrent();
    }
}

void QueueViewerModel::removeIndexes(const std::vector<int>& indexes)
{
    std::vector<int> modelIndexes{indexes};

    if(m_currentTrackItem) {
        for(int& index : modelIndexes) {
            ++index;
        }
    }

    auto startOfSequence = modelIndexes.cbegin();
    while(startOfSequence != modelIndexes.cend()) {
        auto endOfSequence
            = std::adjacent_find(startOfSequence, modelIndexes.cend(), [](int a, int b) { return b != a + 1; });
        if(endOfSequence != modelIndexes.cend()) {
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
    m_currentTrackItem.reset();
    m_trackItems.clear();
    m_trackParents.clear();

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
        if(item) {
            tracks.emplace_back(item->track());
        }
    }

    return tracks;
}

void QueueViewerModel::playbackStateChanged()
{
    if(rowCount({}) > 0) {
        const auto idx = index(0, 0, {});
        emit dataChanged(idx, idx, {Qt::DecorationRole});
    }
}

void QueueViewerModel::currentTrackChanged()
{
    if(m_currentTrackItem) {
        beginRemoveRows({}, 0, 0);
        rootItem()->removeChild(0);
        rootItem()->resetChildren();
        m_currentTrackItem = nullptr;
        endRemoveRows();
    }

    playbackStateChanged();
}

int QueueViewerModel::queueIndex(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return -1;
    }

    int row = index.row();

    if(m_currentTrackItem) {
        --row;
    }

    return row;
}

void QueueViewerModel::regenerateTitles()
{
    const auto titleScript    = m_settings->value<Settings::Gui::Internal::QueueViewerLeftScript>();
    const auto subtitleScript = m_settings->value<Settings::Gui::Internal::QueueViewerRightScript>();

    for(const auto& item : m_trackItems) {
        item->generateTitle(&m_scriptParser, titleScript, subtitleScript);
    }

    if(m_currentTrackItem) {
        m_currentTrackItem->generateTitle(&m_scriptParser, titleScript, subtitleScript);
    }

    invalidateData();
}

void QueueViewerModel::moveTracks(int row, const QModelIndexList& indexes)
{
    const auto indexGroups = Utils::getIndexRanges(indexes);

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

            const int offset = m_currentTrackItem ? -1 : 0;

            if(oldRow < currRow) {
                Utils::move(m_trackItems, oldRow + offset, currRow + offset - 1);
            }
            else {
                Utils::move(m_trackItems, oldRow + offset, currRow + offset);
                ++currRow;
            }
        }

        root->resetChildren();

        endMoveRows();

        row = currRow;
    }
}

void QueueViewerModel::updateShowCurrent()
{
    if(m_settings->value<Settings::Gui::Internal::QueueViewerShowCurrent>() && !m_currentTrackItem
       && m_playerController->currentIsQueueTrack()) {
        const auto titleScript    = m_settings->value<Settings::Gui::Internal::QueueViewerLeftScript>();
        const auto subtitleScript = m_settings->value<Settings::Gui::Internal::QueueViewerRightScript>();

        m_currentTrackItem = std::make_unique<QueueViewerItem>(m_playerController->currentPlaylistTrack());
        m_currentTrackItem->generateTitle(&m_scriptParser, titleScript, subtitleScript);

        beginInsertRows({}, 0, 0);
        rootItem()->insertChild(0, m_currentTrackItem.get());
        rootItem()->resetChildren();
        endInsertRows();
    }
    else if(m_currentTrackItem) {
        beginRemoveRows({}, 0, 0);
        rootItem()->removeChild(0);
        m_currentTrackItem.reset();
        rootItem()->resetChildren();
        endRemoveRows();
    }
}
} // namespace Fooyin
