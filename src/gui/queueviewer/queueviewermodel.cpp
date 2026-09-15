/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include "scripting/scriptvariableproviders.h"

#include <core/player/playbackqueue.h>
#include <core/player/playercontroller.h>
#include <core/scripting/scriptenvironmenthelpers.h>
#include <gui/guiconstants.h>
#include <gui/guisettings.h>
#include <gui/guiutils.h>
#include <gui/iconloader.h>
#include <gui/trackmimedata.h>
#include <utils/modelutils.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QIODevice>
#include <QMimeData>

#include <ranges>

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

QModelIndexList restoreIndexes(QAbstractItemModel& model, QByteArray data)
{
    QModelIndexList result;
    QDataStream stream(&data, QIODevice::ReadOnly);

    while(!stream.atEnd()) {
        int row{-1};
        stream >> row;
        if(row >= 0) {
            result.emplace_back(model.index(row, 0, {}));
        }
    }
    return result;
}
} // namespace

namespace Fooyin {
namespace {
using QueueIndexLookup = std::unordered_map<PlaylistTrack, std::vector<int>, PlaylistTrack::PlaylistTrackHash>;

QueueIndexLookup buildQueueIndexLookup(const QueueTracks& tracks)
{
    QueueIndexLookup indexes;

    for(auto queueIndex{0}; const auto& track : tracks) {
        indexes[track].push_back(queueIndex);
        ++queueIndex;
    }

    return indexes;
}

std::span<const int> queueIndexesFor(const QueueIndexLookup& queueIndexes, const PlaylistTrack& track)
{
    if(const auto it = queueIndexes.find(track); it != queueIndexes.cend()) {
        return std::span{it->second};
    }

    return {};
}

PlaybackScriptContext makeQueueScriptContext(PlayerController* playerController, SettingsManager* settings,
                                             const PlaylistTrack& queueTrack, std::span<const int> queueIndexes,
                                             int queueTotal)
{
    PlaybackScriptContext data;

    uint64_t currentPosition{0};
    uint64_t currentTrackDuration{0};
    int bitrate{0};
    QString decoder;
    int currentTrackId{-1};
    int currentPlayingTrackIndex{-1};
    auto playState{Player::PlayState::Stopped};

    if(playerController) {
        currentPosition          = playerController->currentPosition();
        currentTrackDuration     = playerController->currentTrack().duration();
        bitrate                  = playerController->bitrate();
        decoder                  = playerController->decoder();
        currentTrackId           = playerController->currentTrackId();
        currentPlayingTrackIndex = playerController->currentPlaylistTrack().indexInPlaylist;
        playState                = playerController->playState();
    }

    data.environment.setTrackState(queueTrack.indexInPlaylist, currentPlayingTrackIndex, currentTrackId, 0);
    data.environment.setPlaybackState(currentPosition, currentTrackDuration, bitrate, playState, std::move(decoder));
    data.environment.setRatingStarSymbols(Gui::ratingStarSymbols(*settings));
    data.environment.setEvaluationPolicy(TrackListContextPolicy::Unresolved, {}, true);
    data.environment.setQueueState(queueIndexes, queueTotal);

    return data;
}
} // namespace

QueueViewerModel::QueueViewerModel(CoverRepository* coverRepository, PlayerController* playerController,
                                   SettingsManager* settings, QObject* parent)
    : TreeModel{parent}
    , m_playerController{playerController}
    , m_settings{settings}
    , m_coverProvider{coverRepository}
    , m_showCurrent{false}
    , m_showUpcomingTracks{false}
    , m_showIcon{true}
    , m_firstQueueIndex{0}
    , m_currentQueueItemId{0}
    , m_iconSize{CoverProvider::findThumbnailSize({36, 36})}
    , m_currentTrackItem{nullptr}
{
    m_scriptParser.addProvider(playlistVariableProvider());
    m_scriptParser.addProvider(artworkMarkerVariableProvider());

    QObject::connect(&m_coverProvider, &CoverProvider::coverAdded, this, [this](const Track& track) {
        if(m_trackParents.contains(track.albumHash())) {
            const auto items = m_trackParents.at(track.albumHash());
            for(QueueViewerItem* item : items) {
                const QModelIndex index = indexOfItem(item);
                Q_EMIT dataChanged(index, index, {Qt::DecorationRole});
            }
        }
    });

    m_settings->subscribe<Settings::Gui::RatingFullStarSymbol>(this, [this](const QString&) { regenerateTitles(); });
    m_settings->subscribe<Settings::Gui::RatingHalfStarSymbol>(this, [this](const QString&) { regenerateTitles(); });
    m_settings->subscribe<Settings::Gui::RatingEmptyStarSymbol>(this, [this](const QString&) { regenerateTitles(); });

    updateShowCurrent();
}

Qt::ItemFlags QueueViewerModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags flags = QAbstractItemModel::flags(index);

    const bool isCurrentQueueItem = showsUpcomingTracks() && index.isValid()
                                 && m_playerController->currentQueueItemId() != 0
                                 && queueIndex(index) == m_playerController->playbackQueue().currentIndex();

    if(index.isValid() && (!m_currentTrackItem || index.row() > 0) && !isCurrentQueueItem) {
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
        return Qt::AlignCenter;
    }

    if(role != Qt::DisplayRole) {
        return {};
    }

    if(m_playerController->playbackQueueMode() == PlaybackQueueMode::QueueAsPlaybackSource) {
        return m_showUpcomingTracks ? tr("Upcoming Tracks") : tr("Playing Tracks");
    }

    return tr("Playback Queue");
}

QVariant QueueViewerModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = itemForIndex(index);

    const bool isSequence = m_playerController->playbackQueueMode() == PlaybackQueueMode::QueueAsPlaybackSource;
    const bool isPlaying
        = m_playerController->currentIsQueueTrack()
       && ((isSequence && item->queueItemId() == m_playerController->currentQueueItemId())
           || (!isSequence && item->track() == m_playerController->currentPlaylistTrack() && index.row() == 0));

    switch(role) {
        case Qt::DisplayRole:
            return item->title();
        case QueueViewerItem::RightText:
            return item->subtitle();
        case QueueViewerItem::RichTitle:
            return item->richTitle();
        case QueueViewerItem::RichRightText:
            return item->richSubtitle();
        case QueueViewerItem::IsPlaybackIcon:
            return isPlaying && m_playerController->playState() != Player::PlayState::Stopped;
        case Qt::DecorationRole:
            if(isPlaying) {
                switch(m_playerController->playState()) {
                    case Player::PlayState::Playing:
                        return Gui::pixmapFromTheme(Constants::Icons::Play);
                    case Player::PlayState::Paused:
                        return Gui::pixmapFromTheme(Constants::Icons::Pause);
                    case Player::PlayState::Stopped:
                        break;
                }
            }
            if(m_showIcon) {
                return m_coverProvider.trackCoverThumbnail(item->track().track, m_iconSize);
            }
            break;
        case Qt::TextAlignmentRole:
            return static_cast<int>(Qt::AlignVCenter | Qt::AlignLeft);
        case QueueViewerItem::Track:
            return QVariant::fromValue(item->track());
        case QueueViewerItem::QueueItemId:
            return QVariant::fromValue<qulonglong>(item->queueItemId());
        default:
            break;
    }

    return {};
}

QStringList QueueViewerModel::mimeTypes() const
{
    return {QString::fromLatin1(ViewerItems), QString::fromLatin1(Constants::Mime::Tracks),
            QString::fromLatin1(Constants::Mime::TrackIds), QString::fromLatin1(Constants::Mime::QueueTracks)};
}

bool QueueViewerModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                       const QModelIndex& parent) const
{
    if(action == Qt::MoveAction && data->hasFormat(QString::fromLatin1(ViewerItems))) {
        if(showsUpcomingTracks()) {
            return row != 0;
        }
        return !m_currentTrackItem || row > 0;
    }
    if((action == Qt::CopyAction || action == Qt::MoveAction)
       && (data->hasFormat(QString::fromLatin1(Constants::Mime::Tracks))
           || data->hasFormat(QString::fromLatin1(Constants::Mime::TrackIds))
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
        const QModelIndexList indexes = restoreIndexes(*this, data->data(QString::fromLatin1(ViewerItems)));
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

        Q_EMIT queueTracksMoved(insertionQueueIndex(row), queueIndexes);
        return true;
    }
    if(data->hasFormat(QString::fromLatin1(Constants::Mime::QueueTracks))) {
        Q_EMIT playlistTracksDropped(insertionQueueIndex(row),
                                     data->data(QString::fromLatin1(Constants::Mime::QueueTracks)));
        return true;
    }

    const auto mimeTracks = TrackMimeData::tracksFrom(data);
    if((mimeTracks && !mimeTracks->empty()) || data->hasFormat(QString::fromLatin1(Constants::Mime::TrackIds))) {
        Q_EMIT tracksDropped(insertionQueueIndex(row), data);
        return true;
    }

    return false;
}

void QueueViewerModel::reset(const PlaybackQueue& queue)
{
    beginResetModel();
    resetRoot();
    m_currentTrackItem.reset();
    m_trackItems.clear();
    m_trackParents.clear();
    m_itemsById.clear();
    m_firstQueueIndex    = 0;
    m_currentQueueItemId = queue.currentItemId();

    if(showsUpcomingTracks() && queue.currentIndex() >= 0) {
        m_firstQueueIndex = queue.currentItem() ? queue.currentIndex() : queue.currentIndex() + 1;
        m_firstQueueIndex = std::min(m_firstQueueIndex, queue.trackCount());
    }

    QueueTracks tracks;
    tracks.reserve(queue.items().size() - static_cast<size_t>(m_firstQueueIndex));
    std::ranges::transform(queue.items() | std::views::drop(m_firstQueueIndex), std::back_inserter(tracks),
                           &PlaybackQueueItem::track);
    const auto queueIndexes = buildQueueIndexLookup(tracks);
    const int queueTotal    = static_cast<int>(tracks.size());

    for(const auto& queueItem : queue.items() | std::views::drop(m_firstQueueIndex)) {
        const auto& track = queueItem.track;
        auto* item        = m_trackItems.emplace_back(std::make_unique<QueueViewerItem>(track, queueItem.id)).get();
        const auto contextData = makeQueueScriptContext(m_playerController, m_settings, track,
                                                        queueIndexesFor(queueIndexes, track), queueTotal);
        item->generateTitle(&m_scriptParser, &m_scriptFormatter, m_titleScript, m_subtitleScript, contextData.context);
        rootItem()->appendChild(item);
        m_trackParents[track.track.albumHash()].emplace_back(item);
        m_itemsById.emplace(queueItem.id, item);
    }

    if(shouldShowCurrentRow()) {
        m_currentTrackItem = makeCurrentTrackItem(tracks);
        rootItem()->insertChild(0, m_currentTrackItem.get());
        rootItem()->resetChildren();
    }

    endResetModel();
}

bool QueueViewerModel::updatePlaybackPosition(const PlaybackQueue& queue)
{
    if(m_playerController->playbackQueueMode() != PlaybackQueueMode::QueueAsPlaybackSource) {
        return false;
    }

    int firstQueueIndex{0};
    if(showsUpcomingTracks() && queue.currentIndex() >= 0) {
        firstQueueIndex = queue.currentItem() ? queue.currentIndex() : queue.currentIndex() + 1;
        firstQueueIndex = std::min(firstQueueIndex, queue.trackCount());
    }

    const int expectedRows = queue.trackCount() - firstQueueIndex;
    int rowsToRemove{0};

    if(expectedRows > 0) {
        const auto* firstQueueItem = queue.item(firstQueueIndex);
        const auto firstItem       = firstQueueItem ? m_itemsById.find(firstQueueItem->id) : m_itemsById.cend();
        if(firstItem == m_itemsById.cend()) {
            return false;
        }

        rowsToRemove = firstItem->second->row();
        if(static_cast<int>(m_trackItems.size()) - rowsToRemove != expectedRows
           || m_trackItems.back()->queueItemId() != queue.items().back().id) {
            return false;
        }
    }
    else {
        rowsToRemove = static_cast<int>(m_trackItems.size());
    }

    const PlaybackQueueItemId previousQueueItemId{m_currentQueueItemId};
    removeLeadingItems(rowsToRemove);

    m_firstQueueIndex    = firstQueueIndex;
    m_currentQueueItemId = queue.currentItemId();

    const QModelIndex previousIndex = indexForQueueItem(previousQueueItemId);
    const QModelIndex currentIndex  = indexForQueueItem(m_currentQueueItemId);

    const auto updatePlaybackRoles = [this](const QModelIndex& index) {
        if(index.isValid()) {
            Q_EMIT dataChanged(index, index, {Qt::DecorationRole, QueueViewerItem::IsPlaybackIcon});
        }
    };

    updatePlaybackRoles(previousIndex);
    if(currentIndex != previousIndex) {
        updatePlaybackRoles(currentIndex);
    }

    return true;
}

void QueueViewerModel::playbackStateChanged()
{
    updateShowCurrent();

    const int rows = rowCount({});
    if(rows > 0) {
        Q_EMIT dataChanged(index(0, 0, {}), index(rows - 1, 0, {}),
                           {Qt::DecorationRole, QueueViewerItem::IsPlaybackIcon});
    }
}

QModelIndex QueueViewerModel::indexForQueueItem(const PlaybackQueueItemId queueItemId) const
{
    if(const auto item = m_itemsById.find(queueItemId); item != m_itemsById.cend()) {
        return indexOfItem(item->second);
    }
    return {};
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

    return row < 0 ? -1 : m_firstQueueIndex + row;
}

void QueueViewerModel::regenerateTitles()
{
    QueueTracks tracks;
    tracks.reserve(m_trackItems.size());
    std::ranges::transform(m_trackItems, std::back_inserter(tracks), [](const auto& item) { return item->track(); });
    const auto queueIndexes = buildQueueIndexLookup(tracks);
    const int queueTotal    = static_cast<int>(tracks.size());

    for(const auto& item : m_trackItems) {
        const auto track       = item->track();
        const auto contextData = makeQueueScriptContext(m_playerController, m_settings, track,
                                                        queueIndexesFor(queueIndexes, track), queueTotal);
        item->generateTitle(&m_scriptParser, &m_scriptFormatter, m_titleScript, m_subtitleScript, contextData.context);
    }

    if(m_currentTrackItem) {
        const auto track       = m_currentTrackItem->track();
        const auto contextData = makeQueueScriptContext(m_playerController, m_settings, track,
                                                        queueIndexesFor(queueIndexes, track), queueTotal);
        m_currentTrackItem->generateTitle(&m_scriptParser, &m_scriptFormatter, m_titleScript, m_subtitleScript,
                                          contextData.context);
    }

    invalidateData();
}

void QueueViewerModel::removeLeadingItems(const int count)
{
    if(count <= 0) {
        return;
    }

    beginRemoveRows({}, 0, count - 1);
    rootItem()->clearChildren();

    for(const auto& item : m_trackItems | std::views::take(count)) {
        m_itemsById.erase(item->queueItemId());

        const QString albumHash = item->track().track.albumHash();
        if(auto albumItems = m_trackParents.find(albumHash); albumItems != m_trackParents.end()) {
            std::erase(albumItems->second, item.get());
            if(albumItems->second.empty()) {
                m_trackParents.erase(albumItems);
            }
        }
    }

    m_trackItems.erase(m_trackItems.cbegin(), m_trackItems.cbegin() + count);
    for(const auto& item : m_trackItems) {
        rootItem()->appendChild(item.get());
    }
    endRemoveRows();
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

void QueueViewerModel::setShowUpcomingTracks(const bool showUpcomingTracks)
{
    m_showUpcomingTracks = showUpcomingTracks;
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

std::unique_ptr<QueueViewerItem> QueueViewerModel::makeCurrentTrackItem(const QueueTracks& tracks)
{
    const auto queueIndexes = buildQueueIndexLookup(tracks);
    const int queueTotal    = static_cast<int>(tracks.size());
    const auto currentTrack = m_playerController->currentPlaylistTrack();
    const auto contextData  = makeQueueScriptContext(m_playerController, m_settings, currentTrack,
                                                     queueIndexesFor(queueIndexes, currentTrack), queueTotal);

    auto currentTrackItem = std::make_unique<QueueViewerItem>(currentTrack);
    currentTrackItem->generateTitle(&m_scriptParser, &m_scriptFormatter, m_titleScript, m_subtitleScript,
                                    contextData.context);
    return currentTrackItem;
}

bool QueueViewerModel::shouldShowCurrentRow() const
{
    return m_showCurrent && m_playerController->playState() != Player::PlayState::Stopped
        && m_playerController->currentIsQueueTrack()
        && m_playerController->playbackQueueMode() == PlaybackQueueMode::PlaylistWithOverrides;
}

int QueueViewerModel::insertionQueueIndex(int row) const
{
    if(row < 0) {
        row = static_cast<int>(m_trackItems.size());
    }
    else if(m_currentTrackItem) {
        --row;
    }

    const int firstInsertionIndex
        = m_firstQueueIndex + (showsUpcomingTracks() && m_playerController->playbackQueue().currentItem() ? 1 : 0);
    return std::clamp(m_firstQueueIndex + row, firstInsertionIndex,
                      m_firstQueueIndex + static_cast<int>(m_trackItems.size()));
}

bool QueueViewerModel::showsUpcomingTracks() const
{
    return m_showUpcomingTracks && m_playerController->playbackQueueMode() == PlaybackQueueMode::QueueAsPlaybackSource;
}

void QueueViewerModel::updateShowCurrent()
{
    const bool canInsertCurrentRow  = shouldShowCurrentRow();
    const bool mustRemoveCurrentRow = m_currentTrackItem && !canInsertCurrentRow;

    if(canInsertCurrentRow && !m_currentTrackItem) {
        const QueueTracks tracks = m_playerController->playbackQueue().tracks();
        m_currentTrackItem       = makeCurrentTrackItem(tracks);

        beginInsertRows({}, 0, 0);
        rootItem()->insertChild(0, m_currentTrackItem.get());
        rootItem()->resetChildren();
        endInsertRows();
    }
    else if(mustRemoveCurrentRow && m_currentTrackItem) {
        beginRemoveRows({}, 0, 0);
        rootItem()->removeChild(0);
        m_currentTrackItem.reset();
        rootItem()->resetChildren();
        endRemoveRows();
    }
}
} // namespace Fooyin
