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
    , m_coverProvider{std::move(audioLoader), settings}
    , m_showCurrent{false}
    , m_showIcon{true}
    , m_iconSize{CoverProvider::findThumbnailSize({36, 36})}
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
}

Qt::ItemFlags QueueViewerModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags flags = QAbstractItemModel::flags(index);

    if(index.isValid() && (!m_currentTrackItem || index.row() > 0)) {
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
        return !m_showCurrent || row > 0;
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

        QList<int> queueIndexes;
        queueIndexes.reserve(indexes.size());

        for(const QModelIndex& index : indexes) {
            if(const int queueRow = queueIndex(index); queueRow >= 0) {
                queueIndexes.append(queueRow);
            }
        }

        if(queueIndexes.isEmpty()) {
            return false;
        }

        int queueRow{row};
        if(queueRow < 0) {
            queueRow = static_cast<int>(m_trackItems.size());
        }
        else if(m_currentTrackItem) {
            --queueRow;
        }

        emit queueTracksMoved(std::clamp(queueRow, 0, static_cast<int>(m_trackItems.size())), queueIndexes);
    }
    else if(data->hasFormat(QString::fromLatin1(Constants::Mime::QueueTracks))) {
        int queueRow{row};
        if(queueRow < 0) {
            queueRow = static_cast<int>(m_trackItems.size());
        }
        else if(m_currentTrackItem) {
            --queueRow;
        }
        emit playlistTracksDropped(std::clamp(queueRow, 0, static_cast<int>(m_trackItems.size())),
                                   data->data(QString::fromLatin1(Constants::Mime::QueueTracks)));
    }
    else if(data->hasFormat(QString::fromLatin1(Constants::Mime::TrackIds))) {
        int queueRow{row};
        if(queueRow < 0) {
            queueRow = static_cast<int>(m_trackItems.size());
        }
        else if(m_currentTrackItem) {
            --queueRow;
        }
        emit tracksDropped(std::clamp(queueRow, 0, static_cast<int>(m_trackItems.size())),
                           data->data(QString::fromLatin1(Constants::Mime::TrackIds)));
    }
    return true;
}

void QueueViewerModel::reset(const QueueTracks& tracks)
{
    beginResetModel();
    resetRoot();
    m_currentTrackItem.reset();
    m_trackItems.clear();
    m_trackParents.clear();

    for(const auto& track : tracks) {
        auto* item = m_trackItems.emplace_back(std::make_unique<QueueViewerItem>(track)).get();
        item->generateTitle(&m_scriptParser, m_titleScript, m_subtitleScript);
        rootItem()->appendChild(item);
        m_trackParents[track.track.albumHash()].emplace_back(item);
    }

    if(m_showCurrent && m_playerController->currentIsQueueTrack()) {
        m_currentTrackItem = std::make_unique<QueueViewerItem>(m_playerController->currentPlaylistTrack());
        m_currentTrackItem->generateTitle(&m_scriptParser, m_titleScript, m_subtitleScript);
        rootItem()->insertChild(0, m_currentTrackItem.get());
        rootItem()->resetChildren();
    }

    endResetModel();
}

void QueueViewerModel::playbackStateChanged()
{
    if(rowCount({}) > 0) {
        const auto idx = index(0, 0, {});
        emit dataChanged(idx, idx, {Qt::DecorationRole});
    }
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
    for(const auto& item : m_trackItems) {
        item->generateTitle(&m_scriptParser, m_titleScript, m_subtitleScript);
    }

    if(m_currentTrackItem) {
        m_currentTrackItem->generateTitle(&m_scriptParser, m_titleScript, m_subtitleScript);
    }

    invalidateData();
}

void QueueViewerModel::setScripts(const QString& titleScript, const QString& subtitleScript)
{
    if(m_titleScript == titleScript && m_subtitleScript == subtitleScript) {
        return;
    }

    m_titleScript    = titleScript;
    m_subtitleScript = subtitleScript;
    regenerateTitles();
}

void QueueViewerModel::setShowCurrent(const bool showCurrent)
{
    if(m_showCurrent == showCurrent) {
        return;
    }

    m_showCurrent = showCurrent;
    updateShowCurrent();
}

void QueueViewerModel::setShowIcon(const bool showIcon)
{
    if(m_showIcon == showIcon) {
        return;
    }

    m_showIcon = showIcon;
    invalidateData();
}

void QueueViewerModel::setIconSize(const QSize& iconSize)
{
    const auto thumbnailSize = CoverProvider::findThumbnailSize(iconSize);
    if(m_iconSize == thumbnailSize) {
        return;
    }

    m_iconSize = thumbnailSize;
    invalidateData();
}

void QueueViewerModel::updateShowCurrent()
{
    if(m_showCurrent && !m_currentTrackItem && m_playerController->currentIsQueueTrack()) {
        m_currentTrackItem = std::make_unique<QueueViewerItem>(m_playerController->currentPlaylistTrack());
        m_currentTrackItem->generateTitle(&m_scriptParser, m_titleScript, m_subtitleScript);

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
