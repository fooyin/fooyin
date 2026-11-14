/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include "trackdatabase.h"

#include "databasehelpers.h"
#include "library/statisticsutils.h"

#include <core/constants.h>
#include <core/track.h>
#include <core/trackmetadatastore.h>
#include <utils/database/dbquery.h>
#include <utils/database/dbtransaction.h>
#include <utils/fileutils.h>

#include <QCoreApplication>
#include <QFileInfo>
#include <QLoggingCategory>

#include <map>
#include <set>

Q_LOGGING_CATEGORY(TRK_DB, "fy.trackdb")

using namespace Qt::StringLiterals;

namespace Fooyin {
void TrackDatabase::setMetadataStore(std::shared_ptr<TrackMetadataStore> store)
{
    m_metadataStore = std::move(store);
}

bool TrackDatabase::storeTracks(TrackList& tracks)
{
    if(tracks.empty()) {
        return true;
    }

    DbTransaction transaction{db()};

    if(!transaction) {
        return false;
    }

    const PlaybackStatisticsDatabase statistics{dbProvider()};
    for(auto& track : tracks) {
        statistics.prepareTrack(track);
        if(track.id() < 0) {
            if(!insertTrack(track, true)) {
                return false;
            }

            if(track.id() < 0) {
                if(const int existingTrackId = idForTrack(track); existingTrackId >= 0) {
                    track.setId(existingTrackId);
                }
                else {
                    return false;
                }
            }

            Track::Stats importedStats{Track::Stat::Playcount};
            if(track.rating() > 0) {
                importedStats |= Track::Stat::Rating;
            }

            const auto stats = statistics.updateStats(track, importedStats);
            if(!stats) {
                return false;
            }
            stats->applyTo(track);
        }
    }

    return transaction.commit();
}

bool TrackDatabase::updateTracks(TrackList& tracks, bool overwritePlaycount)
{
    std::set<std::pair<QString, QString>> migratedGroups;
    return updateTracks(tracks, overwritePlaycount, migratedGroups);
}

bool TrackDatabase::updateTracks(TrackList& tracks, bool overwritePlaycount,
                                 std::set<std::pair<QString, QString>>& migratedGroups)
{
    if(tracks.empty()) {
        return true;
    }

    DbTransaction transaction{db()};
    if(!transaction) {
        return false;
    }

    PlaybackStatisticsDatabase statistics{dbProvider()};

    TrackList updated;
    updated.reserve(tracks.size());

    for(const auto& track : tracks) {
        auto result = updateTrack(track, statistics, migratedGroups, true, overwritePlaycount);
        if(!result) {
            return false;
        }
        updated.push_back(std::move(*result));
    }

    if(!statistics.refreshStatistics(updated) || !transaction.commit()) {
        return false;
    }

    tracks = std::move(updated);
    return true;
}

std::expected<void, QString> TrackDatabase::writeScanBatch(TrackList& tracksToStore, TrackList& tracksToUpdate,
                                                           bool overwritePlaycount)
{
    std::set<std::pair<QString, QString>> migratedGroups;
    return writeScanBatch(tracksToStore, tracksToUpdate, overwritePlaycount, migratedGroups);
}

std::expected<void, QString> TrackDatabase::writeScanBatch(TrackList& tracksToStore, TrackList& tracksToUpdate,
                                                           bool overwritePlaycount,
                                                           std::set<std::pair<QString, QString>>& migratedGroups)
{
    if(tracksToStore.empty() && tracksToUpdate.empty()) {
        return {};
    }

    DbTransaction transaction{db()};
    if(!transaction) {
        return std::unexpected{tr("Could not start the scan database transaction.")};
    }

    auto storedTracks{tracksToStore};
    auto updatedTracks{tracksToUpdate};
    auto pendingMigrations{migratedGroups};

    if(!storeTracks(storedTracks)) {
        return std::unexpected{tr("Could not store scanned tracks.")};
    }
    if(!updateTracks(updatedTracks, overwritePlaycount, pendingMigrations)) {
        return std::unexpected{tr("Could not update scanned tracks.")};
    }
    if(!transaction.commit()) {
        return std::unexpected{tr("Could not commit scanned tracks.")};
    }

    tracksToStore  = std::move(storedTracks);
    tracksToUpdate = std::move(updatedTracks);
    migratedGroups = std::move(pendingMigrations);
    return {};
}

bool TrackDatabase::reloadTrack(Track& track) const
{
    static const QString statement
        = u"SELECT %1 FROM TracksView WHERE TrackID = :trackId;"_s.arg(Database::trackColumns());

    DbQuery query{db(), statement};

    query.bindValue(u":trackId"_s, track.id());

    if(!query.exec()) {
        return false;
    }

    if(query.next()) {
        track = Database::readTrack(query, m_metadataStore);
    }

    return true;
}

bool TrackDatabase::reloadTracks(TrackList& tracks) const
{
    if(tracks.empty()) {
        return true;
    }

    QStringList placeholders;
    placeholders.reserve(static_cast<qsizetype>(tracks.size()));
    for(size_t i{0}; i < tracks.size(); ++i) {
        placeholders.emplace_back(u":trackId%1"_s.arg(i));
    }

    const QString statement
        = u"SELECT %1 FROM TracksView WHERE TrackID IN (%2);"_s.arg(Database::trackColumns(), placeholders.join(u','));

    DbQuery q{db(), statement};
    for(size_t i{0}; i < tracks.size(); ++i) {
        q.bindValue(placeholders.at(static_cast<qsizetype>(i)), tracks.at(i).id());
    }

    if(!q.exec()) {
        return false;
    }

    tracks.clear();

    while(q.next()) {
        tracks.emplace_back(Database::readTrack(q, m_metadataStore));
    }

    return true;
}

TrackList TrackDatabase::getAllTracks() const
{
    static const QString statement = u"SELECT %1 FROM TracksView"_s.arg(Database::trackColumns());

    DbQuery q{db(), statement};

    TrackList tracks;

    if(!q.exec()) {
        return {};
    }

    const int numRows = trackCount();
    if(numRows > 0) {
        tracks.reserve(numRows);
    }

    while(q.next()) {
        tracks.emplace_back(Database::readTrack(q, m_metadataStore));
    }

    return tracks;
}

TrackList TrackDatabase::tracksByHash(const QString& hash) const
{
    static const QString statement
        = u"SELECT %1 FROM TracksView WHERE TrackHash = :trackHash"_s.arg(Database::trackColumns());

    DbQuery q{db(), statement};

    q.bindValue(u":trackHash"_s, hash);

    TrackList tracks;

    if(!q.exec()) {
        return tracks;
    }

    while(q.next()) {
        tracks.emplace_back(Database::readTrack(q, m_metadataStore));
    }

    return tracks;
}

int TrackDatabase::idForTrack(Track& track) const
{
    static const QString statement
        = u"SELECT TrackID FROM Tracks WHERE FilePath = :path AND Offset = :offset AND Subsong = :subsong;"_s;

    DbQuery query{db(), statement};

    query.bindValue(u":path"_s, track.filepath());
    query.bindValue(u":offset"_s, static_cast<quint64>(track.offset()));
    query.bindValue(u":subsong"_s, track.subsong());

    if(!query.exec()) {
        return -1;
    }

    if(query.next()) {
        return query.value(0).toInt();
    }

    return -1;
}

std::expected<QString, QSqlError> TrackDatabase::rawRatingTag(int trackId, const QString& tag) const
{
    DbQuery query{db(), u"SELECT ExtraTags FROM Tracks WHERE TrackID = :id"_s};

    query.bindValue(u":id"_s, trackId);

    if(!query.exec()) {
        return std::unexpected{query.lastError()};
    }
    if(!query.next()) {
        return std::unexpected{QSqlError{u"Track no longer exists"_s, {}, QSqlError::StatementError}};
    }

    Track stored;
    stored.storeExtraTags(query.value(0).toByteArray());
    return stored.rawRatingTag(tag);
}

TrackDatabase::MetadataUpdateResult TrackDatabase::updateTrackMetadata(const TrackList& tracks)
{
    MetadataUpdateResult result;
    if(tracks.empty()) {
        return result;
    }

    PlaybackStatisticsDatabase statistics{dbProvider()};
    std::set<std::pair<QString, QString>> migratedGroups;
    result.tracks.reserve(tracks.size());

    for(const Track& track : tracks) {
        if(auto updated = updateTrack(track, statistics, migratedGroups, false)) {
            result.tracks.push_back(std::move(*updated));
        }
        else {
            qCWarning(TRK_DB) << "Failed to update track metadata:" << updated.error();
            ++result.failed;
        }
    }

    if(!statistics.refreshStatistics(result.tracks)) {
        result.failed += static_cast<int>(result.tracks.size());
        result.tracks.clear();
    }

    return result;
}

bool TrackDatabase::updateTrackModifiedTime(const Track& track) const
{
    DbQuery query{db(), u"UPDATE Tracks SET ModifiedDate = :date WHERE TrackID = :id"_s};
    query.bindValue(u":date"_s, QVariant::fromValue(track.modifiedTime()));
    query.bindValue(u":id"_s, track.id());
    return query.exec() && query.numRowsAffected() == 1;
}

bool TrackDatabase::updateTrackFileProperties(const Track& track) const
{
    DbQuery query{db(), u"UPDATE Tracks SET ModifiedDate = :date, ExtraTags = :tags, "
                        "ExtraProperties = :properties WHERE TrackID = :id"_s};
    query.bindValue(u":date"_s, QVariant::fromValue(track.modifiedTime()));
    query.bindValue(u":tags"_s, track.serialiseExtraTags());
    query.bindValue(u":properties"_s, track.serialiseExtraProperties());
    query.bindValue(u":id"_s, track.id());
    return query.exec() && query.numRowsAffected() == 1;
}

bool TrackDatabase::updateTrackStats(const Track& track)
{
    return PlaybackStatisticsDatabase{dbProvider()}.updateStats(track, Track::Stat::All).has_value();
}

bool TrackDatabase::updateTrackStats(Track& track, Track::Stats updatedStats)
{
    const PlaybackStatisticsDatabase statistics{dbProvider()};
    const auto committed = statistics.updateStats(track, updatedStats);
    if(!committed) {
        return false;
    }
    statistics.prepareTrack(track);
    committed->applyTo(track);
    return true;
}

bool TrackDatabase::updateTrackStats(const TrackList& tracks)
{
    DbTransaction transaction{db()};
    if(!transaction) {
        return false;
    }

    const PlaybackStatisticsDatabase statistics{dbProvider()};
    for(const auto& track : tracks) {
        if(!statistics.updateStats(track, Track::Stat::All)) {
            return false;
        }
    }

    return transaction.commit();
}

bool TrackDatabase::updateTrackStats(TrackList& tracks)
{
    DbTransaction transaction{db()};
    if(!transaction) {
        return false;
    }

    const PlaybackStatisticsDatabase statistics{dbProvider()};
    for(auto& track : tracks) {
        const auto committed = statistics.updateStats(track, Track::Stat::All);
        if(!committed) {
            return false;
        }
        statistics.prepareTrack(track);
        committed->applyTo(track);
    }

    return statistics.refreshStatistics(tracks) && transaction.commit();
}

PlaybackStatisticsDatabase::MaintenanceResult
TrackDatabase::manageStatistics(const PlaybackStatisticsRequest& request, QPromise<PlaybackStatisticsResult>* progress)
{
    PlaybackStatisticsDatabase statistics{dbProvider()};
    return statistics.manageStatistics(request, m_metadataStore, progress);
}

TrackList TrackDatabase::tracksByStatisticsKeys(const std::set<QString>& uniqueKeys) const
{
    TrackList tracks;

    const std::vector<QString> keys{uniqueKeys.begin(), uniqueKeys.end()};
    static constexpr int BatchSize = 500;
    for(int start{0}; std::cmp_less(start, keys.size()); start += BatchSize) {
        const auto end = static_cast<int>(std::min(start + BatchSize, static_cast<int>(keys.size())));

        QStringList placeholders;
        for(int i{start}; i < end; ++i) {
            placeholders.push_back(u":key%1"_s.arg(i));
        }
        DbQuery query{db(), u"SELECT %1 FROM TracksView WHERE StatisticsKey IN (%2)"_s.arg(Database::trackColumns(),
                                                                                           placeholders.join(u','))};
        for(int i{start}; i < end; ++i) {
            query.bindValue(placeholders.at(i - start), keys.at(i));
        }
        if(!query.exec()) {
            return {};
        }

        while(query.next()) {
            tracks.push_back(Database::readTrack(query, m_metadataStore));
        }
        if(query.lastError().isValid()) {
            return {};
        }
    }

    return tracks;
}

bool TrackDatabase::refreshStaleStatistics(Track& track) const
{
    return PlaybackStatisticsDatabase{dbProvider()}.refreshStaleStatistics(track);
}

bool TrackDatabase::deleteTrack(int id)
{
    static const QString statement = u"DELETE FROM Tracks WHERE TrackID = :trackID;"_s;

    DbQuery query{db(), statement};

    query.bindValue(u":trackID"_s, id);

    return query.exec();
}

bool TrackDatabase::deleteTracks(const TrackList& tracks)
{
    if(tracks.empty()) {
        return true;
    }

    DbTransaction transaction{db()};

    if(!transaction) {
        return false;
    }

    for(const Track& track : tracks) {
        if(!deleteTrack(track.id())) {
            return false;
        }
    }

    return transaction.commit();
}

std::set<int> TrackDatabase::deleteLibraryTracks(int libraryId)
{
    std::set<int> tracksToRemove;

    {
        static const QString statement = u"SELECT TrackID FROM Tracks WHERE LibraryID = :libraryId AND TrackID "
                                         "NOT IN (SELECT TrackID FROM PlaylistTracks);"_s;

        DbQuery query{db(), statement};

        query.bindValue(u":libraryId"_s, libraryId);

        if(!query.exec()) {
            return {};
        }

        while(query.next()) {
            tracksToRemove.emplace(query.value(0).toInt());
        }
    }

    {
        static const QString statement
            = u"DELETE FROM Tracks WHERE LibraryID = :libraryId AND TrackID NOT IN (SELECT TrackID FROM PlaylistTracks);"_s;

        DbQuery query{db(), statement};

        query.bindValue(u":libraryId"_s, libraryId);

        if(!query.exec()) {
            return {};
        }
    }

    static const QString statement = u"UPDATE Tracks SET LibraryID = :nonLibraryId WHERE LibraryID = :libraryId;"_s;

    DbQuery query{db(), statement};

    query.bindValue(u":nonLibraryId"_s, u"-1"_s);
    query.bindValue(u":libraryId"_s, libraryId);

    if(!query.exec()) {
        return {};
    }

    return tracksToRemove;
}

void TrackDatabase::cleanupTracks()
{
    removeUnmanagedTracks();
    PlaybackStatisticsDatabase{dbProvider()}.cleanupStats();
}

void TrackDatabase::dropViews(const QSqlDatabase& db)
{
    static const QString statement = u"DROP VIEW IF EXISTS TracksView;"_s;

    DbQuery query{db, statement};

    query.exec();
}

void TrackDatabase::insertViews(const QSqlDatabase& db)
{
    static const QString statement
        = u"CREATE VIEW IF NOT EXISTS TracksView AS "
          "SELECT "
          "Tracks.TrackID,"
          "Tracks.FilePath,"
          "Tracks.Subsong,"
          "Tracks.Title,"
          "Tracks.TrackNumber,"
          "Tracks.TrackTotal,"
          "Tracks.Artists,"
          "Tracks.AlbumArtist,"
          "Tracks.Album,"
          "Tracks.DiscNumber,"
          "Tracks.DiscTotal,"
          "Tracks.Date,"
          "Tracks.Composer,"
          "Tracks.Performer,"
          "Tracks.Genres,"
          "Tracks.Comment,"
          "Tracks.CuePath,"
          "Tracks.Offset,"
          "Tracks.Duration,"
          "Tracks.FileSize,"
          "Tracks.BitRate,"
          "Tracks.SampleRate,"
          "Tracks.SampleFrames,"
          "Tracks.Channels,"
          "Tracks.BitDepth,"
          "Tracks.Codec,"
          "Tracks.CodecProfile,"
          "Tracks.Tool,"
          "Tracks.TagTypes,"
          "Tracks.Encoding,"
          "Tracks.ExtraTags,"
          "Tracks.ExtraProperties,"
          "Tracks.ModifiedDate,"
          "Tracks.LibraryID,"
          "Tracks.TrackHash,"
          "Tracks.StatisticsKey,"
          "COALESCE((SELECT Value FROM Settings WHERE Name = 'PlaybackStatisticsRevision'), 0) "
          "AS StatisticsRevision,"
          "Tracks.RGTrackGain,"
          "Tracks.RGAlbumGain,"
          "Tracks.RGTrackPeak,"
          "Tracks.RGAlbumPeak,"
          "Tracks.CreatedDate,"
          "TrackStats.AddedDate,"
          "TrackStats.FirstPlayed,"
          "TrackStats.LastPlayed,"
          "TrackStats.PlayCount,"
          "TrackStats.Rating,"
          "TrackStats.Loved"
          " FROM Tracks "
          "LEFT JOIN TrackStats ON Tracks.StatisticsKey = TrackStats.StatisticsKey;"_s;

    DbQuery query{db, statement};

    query.exec();
}

int TrackDatabase::trackCount() const
{
    static const QString statement = u"SELECT COUNT(*) FROM Tracks;"_s;

    DbQuery query{db(), statement};

    if(!query.exec()) {
        return -1;
    }

    if(query.next()) {
        return query.value(0).toInt();
    }

    return -1;
}

bool TrackDatabase::insertTrack(Track& track, bool ignoreDuplicates) const
{
    DbQuery query{db(), Database::trackInsertStatement(ignoreDuplicates)};

    const auto bindings = Database::trackBindings(track);
    for(const auto& [name, value] : bindings) {
        query.bindValue(name, value);
    }

    if(!query.exec()) {
        return false;
    }

    if(ignoreDuplicates && query.numRowsAffected() == 0) {
        return true;
    }

    track.setId(query.lastInsertId().toInt());
    return true;
}

std::expected<Track, QSqlError> TrackDatabase::updateTrack(Track track, PlaybackStatisticsDatabase& statistics,
                                                           std::set<std::pair<QString, QString>>& migratedGroups,
                                                           bool importStatistics, bool overwritePlaycount)
{
    const auto previous = statistics.trackStatistics(track.id());
    if(!previous) {
        return std::unexpected{previous.error()};
    }

    if(track.statisticsRevision() < statistics.revision()) {
        track.replaceStatistics(*previous, Track::Stat::All);
        track.setAddedTime(previous->addedTime());
    }
    statistics.prepareTrack(track);

    const auto migration  = std::pair{previous->statisticsKey(), track.statisticsKey()};
    const bool keyChanged = migration.first != migration.second;

    DbTransaction transaction{db()};
    if(!transaction) {
        return std::unexpected{transaction.lastError()};
    }

    PlaybackStatisticsDatabase::Statistics current{.added       = previous->addedTime(),
                                                   .firstPlayed = previous->firstPlayed(),
                                                   .lastPlayed  = previous->lastPlayed(),
                                                   .playCount   = previous->playCount(),
                                                   .rating      = previous->rating(),
                                                   .loved       = previous->isLoved()};

    if(keyChanged) {
        const auto target = statistics.existingStats(track.statisticsKey());
        if(!target) {
            return std::unexpected{target.error()};
        }

        if(!migratedGroups.contains(migration)) {
            TrackList contributors{importStatistics ? track : *previous};
            if(*target) {
                contributors.push_back((*target)->toTrack());
            }

            const auto combined = PlaybackStatistics::combineStatistics(contributors, statistics.config());
            if(!statistics.replaceStats(track.statisticsKey(), combined)) {
                return std::unexpected{
                    QSqlError{u"Unable to migrate playback statistics"_s, {}, QSqlError::StatementError}};
            }

            current = {.added       = combined.addedTime(),
                       .firstPlayed = combined.firstPlayed(),
                       .lastPlayed  = combined.lastPlayed(),
                       .playCount   = combined.playCount(),
                       .rating      = combined.rating(),
                       .loved       = combined.isLoved()};
        }
        else {
            current = target->value_or(PlaybackStatisticsDatabase::Statistics{});
        }

        current.applyTo(track);
    }

    static const QString statement = u"UPDATE TRACKS SET "
                                     "FilePath = :filePath,"
                                     "Subsong = :subsong,"
                                     "Title = :title, "
                                     "TrackNumber = :trackNumber,"
                                     "TrackTotal = :trackTotal,"
                                     "Artists = :artists,"
                                     "AlbumArtist = :albumArtist,"
                                     "Album = :album,"
                                     "DiscNumber = :discNumber,"
                                     "DiscTotal = :discTotal,"
                                     "Date = :date,"
                                     "Composer = :composer,"
                                     "Performer = :performer,"
                                     "Genres = :genres,"
                                     "Comment = :comment,"
                                     "CuePath = :cuePath,"
                                     "Offset = :offset,"
                                     "Duration = :duration,"
                                     "FileSize = :fileSize,"
                                     "BitRate = :bitRate,"
                                     "SampleRate = :sampleRate,"
                                     "SampleFrames = :sampleFrames,"
                                     "Channels = :channels,"
                                     "BitDepth = :bitDepth,"
                                     "Codec = :codec,"
                                     "CodecProfile = :codecProfile,"
                                     "Tool = :tool,"
                                     "TagTypes = :tagTypes,"
                                     "Encoding = :encoding,"
                                     "ExtraTags = :extraTags,"
                                     "ExtraProperties = :extraProperties,"
                                     "ModifiedDate = :modifiedDate,"
                                     "TrackHash = :trackHash,"
                                     "StatisticsKey = :statisticsKey,"
                                     "LibraryID = :libraryID,"
                                     "RGTrackGain = :rgTrackGain,"
                                     "RGAlbumGain = :rgAlbumGain,"
                                     "RGTrackPeak = :rgTrackPeak,"
                                     "RGAlbumPeak = :rgAlbumPeak,"
                                     "CreatedDate = :createdDate"
                                     " WHERE TrackID = :trackId;"_s;

    DbQuery query{db(), statement};

    query.bindValue(u":trackId"_s, track.id());

    const auto bindings = Database::trackBindings(track);
    for(const auto& [name, value] : bindings) {
        query.bindValue(name, value);
    }
    query.bindValue(u":statisticsKey"_s, track.statisticsKey());

    if(!query.exec()) {
        return std::unexpected{query.lastError()};
    }

    const auto committed = statistics.updateStats(track, Track::Stat::All, current, overwritePlaycount);
    if(!committed) {
        return std::unexpected{committed.error()};
    }
    committed->applyTo(track);

    if(!transaction.commit()) {
        return std::unexpected{transaction.lastError()};
    }

    if(keyChanged) {
        migratedGroups.insert(migration);
    }
    return track;
}

void TrackDatabase::removeUnmanagedTracks() const
{
    static const QString statement = uR"(
        DELETE FROM Tracks
        WHERE
            (
                LibraryID = -1
                OR NOT EXISTS (
                    SELECT 1
                    FROM Libraries
                    WHERE Libraries.LibraryID = Tracks.LibraryID
                )
            )
            AND TrackID NOT IN (
                SELECT TrackID
                FROM PlaylistTracks
            );
    )"_s;

    DbQuery query{db(), statement};
    query.exec();
}
} // namespace Fooyin
