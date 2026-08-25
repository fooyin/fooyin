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

#include "trackmatchmodel.h"

#include <utils/helpers.h>
#include <utils/stringutils.h>

#include <QDataStream>
#include <QIODevice>
#include <QMimeData>

#include <cstdlib>
#include <set>

using namespace Qt::StringLiterals;

constexpr auto TrackRowsMimeType = "application/x-fooyin-metadata-lookup-track-rows"_L1;

namespace Fooyin {
namespace {
QString trackNumber(const ReleaseTrack& track)
{
    return u"%1.%2"_s.arg(track.mediumPosition).arg(track.number);
}

QString durationDelta(const Track& local, const ReleaseTrack& release)
{
    if(local.duration() == 0 || release.durationMs <= 0) {
        return {};
    }

    const int64_t difference = release.durationMs - static_cast<int64_t>(local.duration());
    if(std::llabs(difference) < 1000) {
        return {};
    }

    const QString sign = difference < 0 ? u"−"_s : u"+"_s;
    QString duration   = Utils::msToString(static_cast<uint64_t>(std::llabs(difference)));
    if(duration.startsWith(u'0')) {
        duration.remove(0, 1);
    }

    return sign + duration;
}
} // namespace

int RetrievedTrackModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_tracks.size());
}

int RetrievedTrackModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 3;
}

QVariant RetrievedTrackModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid() || std::cmp_greater_equal(index.row(), m_tracks.size()) || role != Qt::DisplayRole) {
        return {};
    }

    const ReleaseTrack& track = *m_tracks.at(index.row());
    switch(index.column()) {
        case 0:
            return trackNumber(track);
        case 1:
            return track.title;
        case 2:
            return track.durationMs > 0 ? Utils::msToString(static_cast<uint64_t>(track.durationMs)) : QString{};
        default:
            return {};
    }
}

QVariant RetrievedTrackModel::headerData(int section, Qt::Orientation orientation, int role) const
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

    switch(section) {
        case 0:
            return tr("#");
        case 1:
            return tr("Title");
        case 2:
            return tr("Duration");
        default:
            return {};
    }
}

void RetrievedTrackModel::setRelease(const Release& release)
{
    beginResetModel();

    m_release = release;
    m_tracks  = flattenedTracks(m_release);

    endResetModel();
}

TrackMatchModel::TrackMatchModel(TrackList tracks, QObject* parent)
    : QAbstractTableModel{parent}
    , m_tracks{std::move(tracks)}
    , m_remoteCount{0}
{
    buildAutomaticOrder();
}

int TrackMatchModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_order.size());
}

int TrackMatchModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 5;
}

QVariant TrackMatchModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid() || std::cmp_greater_equal(index.row(), m_order.size())) {
        return {};
    }

    const auto localIndex = m_order.at(index.row());
    if(!localIndex) {
        if(role == Qt::DisplayRole && std::cmp_less(index.row(), m_remoteCount)) {
            if(index.column() == 2) {
                return tr("No local track");
            }
            if(index.column() == 4) {
                return tr("Unmatched");
            }
        }
        return {};
    }

    const Track& local         = m_tracks.at(*localIndex);
    const TrackMatch& match    = m_matches.at(*localIndex);
    const auto remotes         = flattenedTracks(m_release);
    const ReleaseTrack* remote = std::cmp_less(index.row(), remotes.size()) ? remotes.at(index.row()) : nullptr;
    const bool durationDiffers = remote && local.duration() > 0 && remote->durationMs > 0
                              && std::llabs(static_cast<int64_t>(local.duration()) - remote->durationMs) > 10000;

    if(role == Qt::TextAlignmentRole && index.column() != 2) {
        return Qt::AlignCenter;
    }

    if(role == Qt::ToolTipRole) {
        QStringList details;

        if(!match.remoteIndex) {
            details.push_back(tr("This local track is not matched to a retrieved track."));
        }
        else if(match.manual) {
            details.push_back(tr("This match was set manually."));
        }
        else {
            details.push_back(tr("Automatic match confidence: %1%.").arg(match.confidence));
        }

        if(durationDiffers) {
            details.push_back(tr("The local and retrieved durations differ by more than 10 seconds."));
        }

        if(match.ambiguous) {
            details.push_back(tr("This automatic match is ambiguous."));
        }

        details.push_back(tr("Drag this row to match it with a retrieved track."));
        return details.join(u"\n"_s);
    }

    if(role != Qt::DisplayRole) {
        return {};
    }

    switch(index.column()) {
        case 0:
            return u"≡"_s;
        case 1: {
            const QString discNumber = local.discNumber();
            return discNumber.isEmpty() ? local.trackNumber() : u"%1.%2"_s.arg(discNumber, local.trackNumber());
        }
        case 2:
            return local.title().isEmpty() ? local.filenameExt() : local.title();
        case 3:
            return local.duration() > 0 ? Utils::msToString(local.duration()) : QString{};
        case 4: {
            if(!remote) {
                return tr("Unmatched");
            }
            const QString matchStatus = match.manual ? tr("Manual") : tr("%1%").arg(match.confidence);
            const QString difference  = durationDelta(local, *remote);
            return difference.isEmpty() ? matchStatus : tr("%1 / %2").arg(matchStatus, difference);
        }
        default:
            return {};
    }
}

QVariant TrackMatchModel::headerData(int section, Qt::Orientation orientation, int role) const
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

    switch(section) {
        case 0:
            return QString{};
        case 1:
            return tr("Current #");
        case 2:
            return tr("Title / file");
        case 3:
            return tr("Duration");
        case 4:
            return tr("Match / Δ");
        default:
            return {};
    }
}

Qt::ItemFlags TrackMatchModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags defaultFlags = QAbstractTableModel::flags(index);

    defaultFlags |= Qt::ItemIsDropEnabled;

    if(index.isValid() && std::cmp_less(index.row(), m_order.size()) && m_order.at(index.row())) {
        defaultFlags |= Qt::ItemIsDragEnabled;
    }

    return defaultFlags;
}

QStringList TrackMatchModel::mimeTypes() const
{
    return {TrackRowsMimeType};
}

QMimeData* TrackMatchModel::mimeData(const QModelIndexList& indexes) const
{
    std::set<int> rows;
    for(const QModelIndex& index : indexes) {
        if(index.isValid() && std::cmp_less(index.row(), m_order.size()) && m_order.at(index.row())) {
            rows.insert(index.row());
        }
    }

    std::vector<int> selectedRows;
    selectedRows.reserve(rows.size());

    for(const int row : rows) {
        selectedRows.emplace_back(row);
    }

    QByteArray encoded;
    QDataStream stream{&encoded, QIODevice::WriteOnly};

    stream << selectedRows;

    auto* mimeData = new QMimeData();
    mimeData->setData(TrackRowsMimeType, encoded);
    return mimeData;
}

bool TrackMatchModel::canDropMimeData(const QMimeData* data, Qt::DropAction action, int /*row*/, int /*column*/,
                                      const QModelIndex& /*parent*/) const
{
    return action == Qt::MoveAction && data->hasFormat(TrackRowsMimeType);
}

bool TrackMatchModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int /*column*/,
                                   const QModelIndex& parent)
{
    if(!canDropMimeData(data, action, row, 0, parent)) {
        return false;
    }

    QByteArray encoded = data->data(TrackRowsMimeType);
    QDataStream stream{&encoded, QIODevice::ReadOnly};
    QList<int> rows;
    stream >> rows;
    if(rows.size() != 1) {
        return false;
    }

    const int sourceRow = rows.constFirst();
    const int targetRow = row < 0 ? (parent.isValid() ? parent.row() : static_cast<int>(m_order.size()))
                                  : (row > sourceRow ? row - 1 : row);
    return moveTrack(sourceRow, std::clamp(targetRow, 0, static_cast<int>(m_order.size()) - 1));
}

Qt::DropActions TrackMatchModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

Qt::DropActions TrackMatchModel::supportedDragActions() const
{
    return Qt::MoveAction;
}

const Release& TrackMatchModel::release() const
{
    return m_release;
}

void TrackMatchModel::setRelease(Release release, bool matchByPosition)
{
    beginResetModel();

    m_release         = std::move(release);
    m_matchByPosition = matchByPosition;
    buildAutomaticOrder();

    endResetModel();

    Q_EMIT mappingsChanged();
}

bool TrackMatchModel::moveTrack(int sourceRow, int targetRow)
{
    if(sourceRow < 0 || targetRow < 0 || std::cmp_greater_equal(sourceRow, m_order.size())
       || std::cmp_greater_equal(targetRow, m_order.size()) || sourceRow == targetRow || !m_order.at(sourceRow)) {
        return false;
    }

    const int destination = targetRow > sourceRow ? targetRow + 1 : targetRow;
    if(!beginMoveRows({}, sourceRow, sourceRow, {}, destination)) {
        return false;
    }

    Utils::move(m_order, sourceRow, targetRow);
    endMoveRows();

    applyOrderAsMatches();

    Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
    Q_EMIT mappingsChanged();

    return true;
}

bool TrackMatchModel::hasTrackAt(int row) const
{
    return row >= 0 && std::cmp_less(row, m_order.size()) && m_order.at(row).has_value();
}

bool TrackMatchModel::hasUnresolved() const
{
    return std::ranges::any_of(m_matches,
                               [](const TrackMatch& match) { return !match.remoteIndex || match.ambiguous; });
}

const std::vector<TrackMatch>& TrackMatchModel::matches() const
{
    return m_matches;
}

std::vector<const ReleaseTrack*> TrackMatchModel::remoteTracks() const
{
    return flattenedTracks(m_release);
}

void TrackMatchModel::buildAutomaticOrder()
{
    m_automaticMatches
        = m_matchByPosition ? matchTracksByPosition(m_tracks, m_release) : matchTracks(m_tracks, m_release);
    m_matches     = m_automaticMatches;
    m_remoteCount = flattenedTracks(m_release).size();

    const auto unmatchedCount = static_cast<size_t>(
        std::ranges::count_if(m_matches, [](const TrackMatch& match) { return !match.remoteIndex; }));
    m_order.assign(m_remoteCount + unmatchedCount, std::nullopt);

    size_t unmatchedRow{m_remoteCount};
    for(const TrackMatch& match : m_matches) {
        if(match.remoteIndex && *match.remoteIndex < m_remoteCount && !m_order.at(*match.remoteIndex)) {
            m_order.at(*match.remoteIndex) = match.localIndex;
        }
        else {
            m_order.at(unmatchedRow++) = match.localIndex;
        }
    }
}

void TrackMatchModel::applyOrderAsMatches()
{
    for(size_t row{0}; row < m_order.size(); ++row) {
        if(!m_order.at(row)) {
            continue;
        }

        const size_t localIndex = *m_order.at(row);
        TrackMatch& match       = m_matches.at(localIndex);
        const auto remoteIndex  = row < m_remoteCount ? std::optional<size_t>{row} : std::nullopt;

        const TrackMatch& automaticMatch = m_automaticMatches.at(localIndex);
        if(automaticMatch.remoteIndex == remoteIndex) {
            match = automaticMatch;
        }
        else if(match.remoteIndex != remoteIndex) {
            match.remoteIndex = remoteIndex;
            match.confidence  = remoteIndex ? 100 : 0;
            match.ambiguous   = false;
            match.manual      = true;
        }
    }
}
} // namespace Fooyin
