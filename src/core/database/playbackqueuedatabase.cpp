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

#include "playbackqueuedatabase.h"

#include <utils/database/dbquery.h>
#include <utils/database/dbtransaction.h>

using namespace Qt::StringLiterals;

namespace Fooyin {
std::vector<PlaybackQueueInfo> PlaybackQueueDatabase::queue() const
{
    static const QString statement
        = u"SELECT QueueIndex, TrackID, PlaylistID, PlaylistTrackIndex, Origin, SourceOrder, IsCurrent "
          "FROM PlaybackQueue ORDER BY QueueIndex;"_s;

    DbQuery query{db(), statement};
    if(!query.exec()) {
        return {};
    }

    std::vector<PlaybackQueueInfo> queue;

    while(query.next()) {
        PlaybackQueueInfo item;
        item.queueIndex         = query.value(0).toInt();
        item.trackId            = query.value(1).toInt();
        item.playlistDbId       = query.value(2).isNull() ? -1 : query.value(2).toInt();
        item.playlistTrackIndex = query.value(3).toInt();
        item.origin             = query.value(4).toInt();
        item.sourceOrder        = query.value(5).toInt();
        item.isCurrent          = query.value(6).toBool();
        queue.emplace_back(item);
    }

    return queue;
}

bool PlaybackQueueDatabase::replaceQueue(const std::vector<PlaybackQueueInfo>& queue) const
{
    DbTransaction transaction{db()};
    if(!transaction) {
        return false;
    }

    if(!clearQueue()) {
        return false;
    }

    static const QString statement
        = u"INSERT INTO PlaybackQueue "
          "(QueueIndex, TrackID, PlaylistID, PlaylistTrackIndex, Origin, SourceOrder, IsCurrent) "
          "VALUES (:queueIndex, :trackId, :playlistId, :playlistTrackIndex, :origin, :sourceOrder, :isCurrent);"_s;

    DbQuery query{db(), statement};
    for(int i{0}; const auto& item : queue) {
        query.bindValue(u":queueIndex"_s, i++);
        query.bindValue(u":trackId"_s, item.trackId);
        query.bindValue(u":playlistId"_s, item.playlistDbId >= 0 ? item.playlistDbId : QVariant{});
        query.bindValue(u":playlistTrackIndex"_s, item.playlistTrackIndex);
        query.bindValue(u":origin"_s, item.origin);
        query.bindValue(u":sourceOrder"_s, item.sourceOrder);
        query.bindValue(u":isCurrent"_s, item.isCurrent);

        if(!query.exec()) {
            return false;
        }
    }

    return transaction.commit();
}

bool PlaybackQueueDatabase::clearQueue() const
{
    static const QString statement = u"DELETE FROM PlaybackQueue;"_s;

    DbQuery query{db(), statement};
    return query.exec();
}
} // namespace Fooyin
