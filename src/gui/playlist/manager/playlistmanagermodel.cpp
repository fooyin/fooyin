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

#include "playlistmanagermodel.h"

#include "playlist/playlistcontroller.h"
#include "playlist/playlistinteractor.h"

#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <gui/guiconstants.h>
#include <gui/iconloader.h>
#include <gui/trackmimedata.h>
#include <utils/helpers.h>
#include <utils/stringutils.h>

#include <QMimeData>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
QString formatDuration(const PlaylistSummary& summary)
{
    QString text = Utils::msToString(summary.durationMs);
    if(summary.unknownDuration) {
        text.prepend(u"> "_s);
    }
    return text;
}

QString formatSize(const PlaylistSummary& summary)
{
    QString text = Utils::formatFileSize(summary.totalSize);
    if(summary.unknownSize) {
        text.prepend(u"> "_s);
    }
    return text;
}

PlaylistSummary summarisePlaylist(const Playlist* playlist)
{
    if(!playlist) {
        return {};
    }

    PlaylistSummary summary;
    summary.trackCount = playlist->trackCount();

    for(const auto& track : playlist->tracks()) {
        if(track.duration() == 0) {
            summary.unknownDuration = true;
        }
        else {
            summary.durationMs += track.duration();
        }

        if(track.fileSize() == 0) {
            summary.unknownSize = true;
        }
        else {
            summary.totalSize += track.fileSize();
        }
    }

    return summary;
}
} // namespace

PlaylistManagerModel::PlaylistManagerModel(PlaylistInteractor* playlistInteractor, QObject* parent)
    : QAbstractTableModel{parent}
    , m_playlistInteractor{playlistInteractor}
    , m_playlistHandler{playlistInteractor->handler()}
    , m_playerController{playlistInteractor->playerController()}
{
    QObject::connect(m_playlistHandler, &PlaylistHandler::playlistsPopulated, this, &PlaylistManagerModel::populate);
    QObject::connect(m_playlistHandler, &PlaylistHandler::playlistAdded, this, &PlaylistManagerModel::addPlaylist);
    QObject::connect(m_playlistHandler, &PlaylistHandler::playlistRemoved, this, &PlaylistManagerModel::removePlaylist);
    QObject::connect(m_playlistHandler, &PlaylistHandler::playlistRenamed, this,
                     &PlaylistManagerModel::refreshPlaylist);
    QObject::connect(m_playlistHandler, &PlaylistHandler::playlistUpdated, this,
                     &PlaylistManagerModel::refreshPlaylist);
    QObject::connect(m_playlistHandler, &PlaylistHandler::playlistIndexChanged, this,
                     &PlaylistManagerModel::reorderPlaylist);
    QObject::connect(m_playlistHandler, &PlaylistHandler::tracksAdded, this, &PlaylistManagerModel::refreshPlaylist);
    QObject::connect(m_playlistHandler, &PlaylistHandler::tracksPatched, this, &PlaylistManagerModel::refreshPlaylist);
    QObject::connect(m_playlistHandler, &PlaylistHandler::tracksChanged, this, &PlaylistManagerModel::refreshPlaylist);
    QObject::connect(m_playlistHandler, &PlaylistHandler::tracksUpdated, this, &PlaylistManagerModel::refreshPlaylist);
    QObject::connect(m_playlistHandler, &PlaylistHandler::tracksRemoved, this, &PlaylistManagerModel::refreshPlaylist);

    const auto refreshDecorations = [this]() {
        if(!m_playlists.empty()) {
            Q_EMIT dataChanged(index(0, Name), index(rowCount({}) - 1, Name), {Qt::DecorationRole});
        }
    };
    QObject::connect(m_playlistHandler, &PlaylistHandler::activePlaylistChanged, this, refreshDecorations);
    QObject::connect(m_playerController, &PlayerController::playStateChanged, this, refreshDecorations);

    populate();
}

int PlaylistManagerModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_playlists.size());
}

int PlaylistManagerModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : Count;
}

QVariant PlaylistManagerModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid() || index.row() < 0 || std::cmp_greater_equal(index.row(), m_playlists.size())) {
        return {};
    }

    const auto* playlist           = m_playlists.at(static_cast<size_t>(index.row()));
    const PlaylistSummary& summary = m_summaries.at(static_cast<size_t>(index.row()));

    if(role == Qt::DisplayRole) {
        switch(static_cast<Column>(index.column())) {
            case Name:
                return playlist->name();
            case Tracks:
                return summary.trackCount;
            case Duration:
                return formatDuration(summary);
            case TotalSize:
                return formatSize(summary);
            default:
                break;
        }
    }

    if(role == Qt::EditRole && index.column() == Name) {
        return playlist->name();
    }

    if(role == Qt::DecorationRole && index.column() == Name) {
        if(const auto* active = m_playlistHandler->activePlaylist(); active && active->id() == playlist->id()) {
            const auto state = m_playerController->playState();
            if(state == Player::PlayState::Playing) {
                return Gui::pixmapFromTheme(Constants::Icons::Play);
            }
            if(state == Player::PlayState::Paused) {
                return Gui::pixmapFromTheme(Constants::Icons::Pause);
            }
        }
        if(playlist->isLocked()) {
            return Gui::pixmapFromTheme(Constants::Icons::ReadOnly);
        }
    }

    if(role == Qt::TextAlignmentRole && index.column() != Name) {
        return static_cast<int>(Qt::AlignVCenter | Qt::AlignRight);
    }

    if(role == SortRole) {
        switch(index.column()) {
            case Name:
                return playlist->name();
            case Tracks:
                return summary.trackCount;
            case Duration:
                return static_cast<quint64>(summary.durationMs);
            case TotalSize:
                return static_cast<quint64>(summary.totalSize);
            default:
                break;
        }
    }

    return {};
}

QVariant PlaylistManagerModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(section, orientation, role);
    }

    switch(static_cast<Column>(section)) {
        case Name:
            return tr("Playlist name");
        case Tracks:
            return tr("Tracks");
        case Duration:
            return tr("Duration");
        case TotalSize:
            return tr("Total size");
        case Count:
            return {};
    }
    return {};
}

QStringList PlaylistManagerModel::mimeTypes() const
{
    return {QString::fromLatin1(Constants::Mime::Tracks), QString::fromLatin1(Constants::Mime::TrackIds)};
}

bool PlaylistManagerModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                           const QModelIndex& parent) const
{
    if(column < -1 || !isSupportedDrop(data)) {
        return false;
    }

    if(action != Qt::CopyAction && action != Qt::MoveAction) {
        return false;
    }

    if(auto* playlist = playlistForDropTarget(row, parent)) {
        return !playlist->isAutoPlaylist() && !playlist->isLocked();
    }

    return true;
}

Qt::DropActions PlaylistManagerModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

Qt::ItemFlags PlaylistManagerModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags defaultFlags = QAbstractTableModel::flags(index);

    if(!index.isValid()) {
        defaultFlags |= Qt::ItemIsDropEnabled;
    }

    if(index.column() == Name) {
        defaultFlags |= Qt::ItemIsEditable;
    }

    return defaultFlags;
}

bool PlaylistManagerModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                                        const QModelIndex& parent)
{
    if(!canDropMimeData(data, action, row, column, parent) || !m_playlistInteractor) {
        return false;
    }

    auto* playlist       = playlistForDropTarget(row, parent);
    const UId playlistId = playlist ? playlist->id() : UId{};

    const auto mimeTracks = TrackMimeData::tracksFrom(data);
    if(mimeTracks.has_value() && !mimeTracks->empty()) {
        m_playlistInteractor->tracksToPlaylist(*mimeTracks, playlistId);
        return true;
    }

    if(data->hasFormat(QString::fromLatin1(Constants::Mime::TrackIds))) {
        m_playlistInteractor->trackIdsToPlaylist(data->data(QString::fromLatin1(Constants::Mime::TrackIds)),
                                                 playlistId);
        return true;
    }

    if(data->hasUrls()) {
        m_playlistInteractor->filesToPlaylist(data->urls(), playlistId);
        return true;
    }

    return false;
}

bool PlaylistManagerModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(role != Qt::EditRole || !index.isValid() || index.column() != Name) {
        return false;
    }

    if(auto* playlist = playlistForRow(index.row())) {
        const QString name = value.toString().trimmed();
        if(name != playlist->name()) {
            m_playlistHandler->renamePlaylist(playlist->id(), name);
        }
        return true;
    }

    return false;
}

Playlist* PlaylistManagerModel::playlistForRow(int row) const
{
    if(row < 0 || std::cmp_greater_equal(row, m_playlists.size())) {
        return nullptr;
    }
    return m_playlists.at(static_cast<size_t>(row));
}

QModelIndex PlaylistManagerModel::indexForPlaylist(const Playlist* playlist, int column) const
{
    const int row = rowForPlaylist(playlist);
    return row >= 0 ? index(row, column) : QModelIndex{};
}

void PlaylistManagerModel::populate()
{
    beginResetModel();

    m_playlists = m_playlistHandler->playlists();

    m_summaries.clear();
    m_summaries.reserve(m_playlists.size());

    for(const auto* playlist : m_playlists) {
        m_summaries.push_back(summarisePlaylist(playlist));
    }

    endResetModel();
}

void PlaylistManagerModel::addPlaylist(Playlist* playlist)
{
    if(!playlist) {
        return;
    }

    const int row = static_cast<int>(m_playlists.size());

    beginInsertRows({}, row, row);
    m_playlists.push_back(playlist);
    m_summaries.push_back(summarisePlaylist(playlist));
    endInsertRows();
}

void PlaylistManagerModel::reorderPlaylist(Playlist* playlist)
{
    const int from = rowForPlaylist(playlist);
    if(from < 0) {
        return;
    }

    const int to = playlist->index();
    if(to == from) {
        return;
    }

    const int destRow = (to > from) ? (to + 1) : to;

    beginMoveRows({}, from, from, {}, destRow);
    Utils::move(m_playlists, from, to);
    Utils::move(m_summaries, from, to);
    endMoveRows();
}

void PlaylistManagerModel::removePlaylist(Playlist* playlist)
{
    const int row = rowForPlaylist(playlist);
    if(row < 0) {
        return;
    }

    beginRemoveRows({}, row, row);
    m_playlists.erase(m_playlists.begin() + row);
    m_summaries.erase(m_summaries.begin() + row);
    endRemoveRows();
}

void PlaylistManagerModel::refreshPlaylist(Playlist* playlist)
{
    const int row = rowForPlaylist(playlist);
    if(row < 0) {
        return;
    }

    m_summaries[static_cast<size_t>(row)] = summarisePlaylist(playlist);
    Q_EMIT dataChanged(index(row, 0), index(row, Count - 1));
}

int PlaylistManagerModel::rowForPlaylist(const Playlist* playlist) const
{
    if(!playlist) {
        return -1;
    }

    for(size_t i{0}; i < m_playlists.size(); ++i) {
        if(m_playlists[i] == playlist) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

Playlist* PlaylistManagerModel::playlistForDropTarget(int row, const QModelIndex& parent) const
{
    if(parent.isValid()) {
        return playlistForRow(parent.row());
    }

    if(row >= 0 && row < rowCount({})) {
        return playlistForRow(row);
    }

    return nullptr;
}

bool PlaylistManagerModel::isSupportedDrop(const QMimeData* data) const
{
    return data
        && (data->hasUrls() || data->hasFormat(QString::fromLatin1(Constants::Mime::TrackIds))
            || data->hasFormat(QString::fromLatin1(Constants::Mime::Tracks)));
}

} // namespace Fooyin
