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

#include <core/playhistory/playhistory.h>

#include "database/playhistorydatabase.h"

#include <core/constants.h>
#include <utils/async.h>
#include <utils/database/dbconnectionhandler.h>
#include <utils/database/dbconnectionprovider.h>

#include <QDateTime>

#include <array>

using namespace Qt::StringLiterals;

namespace {
int losslessState(const QString& codec)
{
    if(codec.isEmpty()) {
        return -1;
    }

    static constexpr std::array LosslessCodecs{"flac", "alac", "apple lossless", "wavpack", "ape", "monkey", "tta",
                                               "tak",  "wav",  "aiff",           "pcm",     "dsd", "mlp",    "truehd"};

    const QString lower = codec.toLower();
    for(const char* name : LosslessCodecs) {
        if(lower.contains(QLatin1String{name})) {
            return 1;
        }
    }

    return 0;
}
} // namespace

namespace Fooyin {
QString PlayHistoryEntry::albumKey() const
{
    return albumKey(album, albumArtists);
}

QString PlayHistoryEntry::albumKey(const QString& album, const QStringList& albumArtists)
{
    if(album.isEmpty()) {
        return {};
    }

    return album + QLatin1String{Constants::UnitSeparator} + albumArtists.join(QLatin1String{Constants::UnitSeparator});
}

PlayHistoryEntry PlayHistoryEntry::fromTrack(const Track& track, qint64 timestamp)
{
    PlayHistoryEntry entry;
    entry.timestamp    = timestamp;
    entry.hash         = track.hash().isEmpty() ? track.uniqueFilepath() : track.hash();
    entry.title        = track.title();
    entry.artists      = track.artists();
    entry.album        = track.album();
    entry.albumArtists = track.albumArtists();
    entry.genres       = track.genres();
    entry.composers    = track.composers();
    entry.year         = track.year();
    entry.duration     = track.duration();
    entry.bitrate      = track.bitrate();
    entry.lossless     = losslessState(track.codec());
    return entry;
}

PlayHistory::PlayHistory(DbConnectionPoolPtr dbPool, QObject* parent)
    : QObject{parent}
    , m_dbPool{std::move(dbPool)}
    , m_database{std::make_unique<PlayHistoryDatabase>()}
{
    m_database->initialise(DbConnectionProvider{m_dbPool});
}

PlayHistory::~PlayHistory() = default;

void PlayHistory::recordPlay(const Track& track)
{
    if(m_database->insert(PlayHistoryEntry::fromTrack(track, QDateTime::currentMSecsSinceEpoch()))) {
        Q_EMIT playRecorded();
    }
}

QFuture<PlayHistoryData> PlayHistory::load(qint64 from, qint64 to) const
{
    return Utils::asyncExec([pool = m_dbPool, from, to]() {
        const DbConnectionHandler dbHandler{pool};

        PlayHistoryDatabase database;
        database.initialise(DbConnectionProvider{pool});
        return database.load(from, to);
    });
}

QFuture<PlayHistorySummary> PlayHistory::summary() const
{
    return Utils::asyncExec([pool = m_dbPool]() {
        const DbConnectionHandler dbHandler{pool};

        PlayHistoryDatabase database;
        database.initialise(DbConnectionProvider{pool});
        return database.summary();
    });
}
} // namespace Fooyin

#include "core/playhistory/moc_playhistory.cpp"
