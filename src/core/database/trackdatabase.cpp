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

#include "trackdatabase.h"

#include <core/constants.h>
#include <core/track.h>
#include <utils/database/dbquery.h>
#include <utils/database/dbtransaction.h>
#include <utils/fileutils.h>

#include <QFileInfo>

using BindingsMap = std::map<QString, QVariant>;

namespace {
QString fetchTrackColumns()
{
    static const QString columns = QStringLiteral("TrackID,"
                                                  "FilePath,"
                                                  "RelativePath,"
                                                  "Title,"
                                                  "TrackNumber,"
                                                  "TrackTotal,"
                                                  "Artists,"
                                                  "AlbumArtist,"
                                                  "Album,"
                                                  "DiscNumber,"
                                                  "DiscTotal,"
                                                  "Date,"
                                                  "Composer,"
                                                  "Performer,"
                                                  "Genres,"
                                                  "Comment,"
                                                  "Duration,"
                                                  "FileSize,"
                                                  "BitRate,"
                                                  "SampleRate,"
                                                  "ExtraTags,"
                                                  "HasEmbeddedCover,"
                                                  "CoverPath,"
                                                  "Type,"
                                                  "ModifiedDate, "
                                                  "LibraryID,"
                                                  "TrackHash,"
                                                  "AddedDate,"
                                                  "FirstPlayed,"
                                                  "LastPlayed,"
                                                  "PlayCount,"
                                                  "Rating");

    return columns;
}

BindingsMap trackBindings(const Fooyin::Track& track)
{
    return {{QStringLiteral(":filePath"), Fooyin::Utils::File::cleanPath(track.filepath())},
            {QStringLiteral(":title"), track.title()},
            {QStringLiteral(":trackNumber"), track.trackNumber()},
            {QStringLiteral(":trackTotal"), track.trackTotal()},
            {QStringLiteral(":artists"), track.artists().join(u"\037")},
            {QStringLiteral(":albumArtist"), track.albumArtist()},
            {QStringLiteral(":album"), track.album()},
            {QStringLiteral(":coverPath"), track.coverPath()},
            {QStringLiteral(":discNumber"), track.discNumber()},
            {QStringLiteral(":discTotal"), track.discTotal()},
            {QStringLiteral(":date"), track.date()},
            {QStringLiteral(":composer"), track.composer()},
            {QStringLiteral(":performer"), track.performer()},
            {QStringLiteral(":genres"), track.genres().join(u"\037")},
            {QStringLiteral(":comment"), track.comment()},
            {QStringLiteral(":duration"), QVariant::fromValue(track.duration())},
            {QStringLiteral(":fileSize"), QVariant::fromValue(track.fileSize())},
            {QStringLiteral(":bitRate"), track.bitrate()},
            {QStringLiteral(":sampleRate"), track.sampleRate()},
            {QStringLiteral(":extraTags"), track.serialiseExtrasTags()},
            {QStringLiteral(":type"), static_cast<int>(track.type())},
            {QStringLiteral(":modifiedDate"), QVariant::fromValue(track.modifiedTime())},
            {QStringLiteral(":trackHash"), track.hash()},
            {QStringLiteral(":libraryID"), track.libraryId()}};
}

Fooyin::Track readToTrack(const Fooyin::DbQuery& q)
{
    Fooyin::Track track;

    track.setId(q.value(0).toInt());
    track.setFilePath(q.value(1).toString());
    track.setRelativePath(q.value(2).toString());
    track.setTitle(q.value(3).toString());
    track.setTrackNumber(q.value(4).toInt());
    track.setTrackTotal(q.value(5).toInt());
    track.setArtists(q.value(6).toString().split(QStringLiteral("\037"), Qt::SkipEmptyParts));
    track.setAlbumArtist(q.value(7).toString());
    track.setAlbum(q.value(8).toString());
    track.setDiscNumber(q.value(9).toInt());
    track.setDiscTotal(q.value(10).toInt());
    track.setDate(q.value(11).toString());
    track.setComposer(q.value(12).toString());
    track.setPerformer(q.value(13).toString());
    track.setGenres(q.value(14).toString().split(QStringLiteral("\037"), Qt::SkipEmptyParts));
    track.setComment(q.value(15).toString());
    track.setDuration(q.value(16).toULongLong());
    track.setFileSize(q.value(17).toInt());
    track.setBitrate(q.value(18).toInt());
    track.setSampleRate(q.value(19).toInt());
    track.storeExtraTags(q.value(20).toByteArray());
    // track.setCoverPath(q.value(21).toString());
    track.setCoverPath(q.value(22).toString());
    track.setType(static_cast<Fooyin::Track::Type>(q.value(23).toInt()));
    track.setModifiedTime(q.value(24).toULongLong());
    track.setLibraryId(q.value(25).toInt());
    track.setHash(q.value(26).toString());
    track.setAddedTime(q.value(27).toULongLong());
    track.setFirstPlayed(q.value(28).toULongLong());
    track.setLastPlayed(q.value(29).toULongLong());
    track.setPlayCount(q.value(30).toInt());

    track.generateHash();
    track.setIsEnabled(QFileInfo::exists(track.filepath()));

    return track;
}
} // namespace

namespace Fooyin {
bool TrackDatabase::storeTracks(TrackList& tracks)
{
    if(tracks.empty()) {
        return true;
    }

    DbTransaction transaction{db()};

    if(!transaction) {
        return false;
    }

    for(auto& track : tracks) {
        if(track.id() >= 0) {
            updateTrack(track);
        }
        else {
            insertTrack(track);
        }
    }

    return transaction.commit();
}

bool TrackDatabase::reloadTrack(Track& track) const
{
    const auto statement
        = QStringLiteral("SELECT %1 FROM TracksView WHERE TrackID = :trackId;").arg(fetchTrackColumns());

    DbQuery query{db(), statement};

    query.bindValue(QStringLiteral(":trackId"), track.id());

    if(!query.exec()) {
        return false;
    }

    if(query.next()) {
        track = readToTrack(query);
    }

    return true;
}

bool TrackDatabase::reloadTracks(TrackList& tracks) const
{
    const auto statement
        = QStringLiteral("SELECT %1 FROM TracksView WHERE TrackID IN (:trackIds);").arg(fetchTrackColumns());

    DbQuery q{db(), statement};

    QStringList trackIds;
    std::transform(tracks.cbegin(), tracks.cend(), std::back_inserter(trackIds),
                   [](const Track& track) { return QString::number(track.id()); });

    q.bindValue(QStringLiteral(":trackIds"), trackIds);

    if(!q.exec()) {
        return false;
    }

    tracks.clear();

    while(q.next()) {
        tracks.emplace_back(readToTrack(q));
    }

    return true;
}

TrackList TrackDatabase::getAllTracks() const
{
    const auto statement = QStringLiteral("SELECT %1 FROM TracksView").arg(fetchTrackColumns());

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
        tracks.emplace_back(readToTrack(q));
    }

    return tracks;
}

TrackList TrackDatabase::tracksByHash(const QString& hash) const
{
    const auto statement
        = QStringLiteral("SELECT %1 FROM TracksView WHERE TrackHash = :trackHash").arg(fetchTrackColumns());

    DbQuery q{db(), statement};

    q.bindValue(QStringLiteral(":trackHash"), hash);

    TrackList tracks;

    if(!q.exec()) {
        return tracks;
    }

    while(q.next()) {
        tracks.emplace_back(readToTrack(q));
    }

    return tracks;
}

bool TrackDatabase::updateTrack(const Track& track)
{
    if(track.id() < 0) {
        qDebug() << QStringLiteral("Cannot update track %1 (Invalid ID)").arg(track.filepath());
        return false;
    }

    const auto statement = QStringLiteral("UPDATE TRACKS SET "
                                          "FilePath = :filePath,"
                                          "Title = :title, "
                                          "TrackNumber = :trackNumber,"
                                          "TrackTotal = :trackTotal,"
                                          "Artists = :artists,"
                                          "AlbumArtist = :albumArtist,"
                                          "Album = :album,"
                                          "CoverPath = :coverPath,"
                                          "DiscNumber = :discNumber,"
                                          "DiscTotal = :discTotal,"
                                          "Date = :date,"
                                          "Composer = :composer,"
                                          "Performer = :performer,"
                                          "Genres = :genres,"
                                          "Comment = :comment,"
                                          "Duration = :duration,"
                                          "FileSize = :fileSize,"
                                          "BitRate = :bitRate,"
                                          "SampleRate = :sampleRate,"
                                          "ExtraTags = :extraTags,"
                                          "Type = :type,"
                                          "ModifiedDate = :modifiedDate,"
                                          "TrackHash = :trackHash,"
                                          "LibraryID = :libraryID"
                                          "WHERE TrackID = :trackId;");

    DbQuery query{db(), statement};

    query.bindValue(QStringLiteral(":trackId"), track.id());

    const auto bindings = trackBindings(track);
    for(const auto& [name, value] : bindings) {
        query.bindValue(name, value);
    }

    return query.exec();
}

bool TrackDatabase::updateTrackStats(Track& track)
{
    return insertOrUpdateStats(track);
}

bool TrackDatabase::deleteTrack(int id)
{
    const QString statement = QStringLiteral("DELETE FROM Tracks WHERE TrackID = :trackID;");

    DbQuery query{db(), statement};

    query.bindValue(QStringLiteral(":trackID"), id);

    return !query.exec();
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

    const int fileCount = static_cast<int>(
        std::count_if(tracks.cbegin(), tracks.cend(), [this](const Track& track) { return deleteTrack(track.id()); }));

    const auto success = transaction.commit();

    return (success && (fileCount == static_cast<int>(tracks.size())));
}

std::set<int> TrackDatabase::deleteLibraryTracks(int libraryId)
{
    std::set<int> tracksToRemove;

    {
        const auto statement = QStringLiteral("SELECT TrackID FROM Tracks WHERE LibraryID = :libraryId AND TrackID "
                                              "NOT IN (SELECT TrackID FROM PlaylistTracks);");

        DbQuery query{db(), statement};

        query.bindValue(QStringLiteral(":libraryId"), libraryId);

        if(!query.exec()) {
            return {};
        }

        while(query.next()) {
            tracksToRemove.emplace(query.value(0).toInt());
        }
    }

    {
        const auto statement = QStringLiteral(
            "DELETE FROM Tracks WHERE LibraryID = :libraryId AND TrackID NOT IN (SELECT TrackID FROM PlaylistTracks);");

        DbQuery query{db(), statement};

        query.bindValue(QStringLiteral(":libraryId"), libraryId);

        if(!query.exec()) {
            return {};
        }
    }

    const auto statement = QStringLiteral("UPDATE Tracks SET LibraryID = :nonLibraryId WHERE LibraryID = :libraryId;");

    DbQuery query{db(), statement};

    query.bindValue(QStringLiteral(":nonLibraryId"), QStringLiteral("-1"));
    query.bindValue(QStringLiteral(":libraryId"), libraryId);

    if(!query.exec()) {
        return {};
    }

    return tracksToRemove;
}

void TrackDatabase::cleanupTracks()
{
    removeUnmanagedTracks();
    markUnusedStatsForDelete();
    deleteExpiredStats();
}

void TrackDatabase::updateViews(const QSqlDatabase& db)
{
    {
        const auto statement = QStringLiteral("DROP VIEW IF EXISTS TracksView;");

        DbQuery query{db, statement};

        if(!query.exec()) {
            return;
        }
    }

    const auto statement = QStringLiteral("CREATE VIEW IF NOT EXISTS TracksView AS "
                                          "SELECT "
                                          "Tracks.TrackID,"
                                          "Tracks.FilePath,"
                                          "SUBSTR(Tracks.FilePath, LENGTH(Libraries.Path) + 2) AS RelativePath,"
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
                                          "Tracks.Duration,"
                                          "Tracks.FileSize,"
                                          "Tracks.BitRate,"
                                          "Tracks.SampleRate,"
                                          "Tracks.ExtraTags,"
                                          "Tracks.HasEmbeddedCover,"
                                          "Tracks.CoverPath,"
                                          "Tracks.Type,"
                                          "Tracks.ModifiedDate,"
                                          "Tracks.LibraryID,"
                                          "Tracks.TrackHash,"
                                          "TrackStats.AddedDate,"
                                          "TrackStats.FirstPlayed,"
                                          "TrackStats.LastPlayed,"
                                          "TrackStats.PlayCount,"
                                          "TrackStats.Rating"
                                          " FROM Tracks "
                                          "LEFT JOIN Libraries ON Tracks.LibraryID = Libraries.LibraryID "
                                          "LEFT JOIN TrackStats ON Tracks.TrackHash = TrackStats.TrackHash;");

    DbQuery query{db, statement};

    query.exec();
}

int TrackDatabase::trackCount() const
{
    const auto statement = QStringLiteral("SELECT COUNT(*) FROM Tracks;");

    DbQuery query{db(), statement};

    if(!query.exec()) {
        return -1;
    }

    if(query.next()) {
        return query.value(0).toInt();
    }

    return -1;
}

bool TrackDatabase::insertTrack(Track& track) const
{
    const auto statement = QStringLiteral("INSERT INTO Tracks ("
                                          "FilePath,"
                                          "Title,"
                                          "TrackNumber,"
                                          "TrackTotal,"
                                          "Artists,"
                                          "AlbumArtist,"
                                          "Album,"
                                          "CoverPath,"
                                          "DiscNumber,"
                                          "DiscTotal,"
                                          "Date,"
                                          "Composer,"
                                          "Performer,"
                                          "Genres,"
                                          "Comment,"
                                          "Duration,"
                                          "FileSize,"
                                          "BitRate,"
                                          "SampleRate,"
                                          "ExtraTags,"
                                          "Type,"
                                          "ModifiedDate,"
                                          "TrackHash,"
                                          "LibraryID"
                                          ") "
                                          "VALUES ("
                                          ":filePath,"
                                          ":title, "
                                          ":trackNumber,"
                                          ":trackTotal,"
                                          ":artists,"
                                          ":albumArtist,"
                                          ":album,"
                                          ":coverPath,"
                                          ":discNumber,"
                                          ":discTotal,"
                                          ":date, "
                                          ":composer,"
                                          ":performer,"
                                          ":genres,"
                                          ":comment,"
                                          ":duration,"
                                          ":fileSize,"
                                          ":bitRate,"
                                          ":sampleRate,"
                                          ":extraTags, "
                                          ":type,"
                                          ":modifiedDate,"
                                          ":trackHash,"
                                          ":libraryID"
                                          ");");

    DbQuery query{db(), statement};

    const auto bindings = trackBindings(track);
    for(const auto& [name, value] : bindings) {
        query.bindValue(name, value);
    }

    if(!query.exec()) {
        return false;
    }

    track.setId(query.lastInsertId().toInt());

    return insertOrUpdateStats(track);
}

bool TrackDatabase::insertOrUpdateStats(Track& track) const
{
    if(track.hash().isEmpty()) {
        qDebug() << "Cannot insert/update track stats (Hash empty)";
        return false;
    }

    uint64_t added{0};
    uint64_t firstPlayed{0};
    uint64_t lastPlayed{0};
    int playCount{0};

    {
        const auto statement = QStringLiteral("SELECT AddedDate, FirstPlayed, LastPlayed, PlayCount, Rating FROM "
                                              "TrackStats WHERE TrackHash = :trackHash;");

        DbQuery query{db(), statement};

        query.bindValue(QStringLiteral(":trackHash"), track.hash());

        if(!query.exec()) {
            return false;
        }

        if(query.next()) {
            added       = query.value(0).toULongLong();
            firstPlayed = query.value(1).toULongLong();
            lastPlayed  = query.value(2).toULongLong();
            playCount   = query.value(3).toInt();
        }
    }

    bool dbNeedsUpdate{false};

    const uint64_t trackAdded       = track.addedTime();
    const uint64_t trackFirstPlayed = track.firstPlayed();
    const uint64_t trackLastPlayed  = track.lastPlayed();
    const int trackPlayCount        = track.playCount();

    if(trackAdded != added) {
        if(added == 0 || (trackAdded > 0 && trackAdded < added)) {
            added         = trackAdded;
            dbNeedsUpdate = true;
        }
        else {
            track.setAddedTime(added);
        }
    }
    if(trackFirstPlayed != firstPlayed) {
        if(firstPlayed == 0 || (trackFirstPlayed > 0 && trackFirstPlayed < firstPlayed)) {
            firstPlayed   = trackFirstPlayed;
            dbNeedsUpdate = true;
        }
        else {
            track.setFirstPlayed(firstPlayed);
        }
    }
    if(trackLastPlayed != lastPlayed) {
        if(trackLastPlayed > lastPlayed) {
            lastPlayed    = trackLastPlayed;
            dbNeedsUpdate = true;
        }
        else {
            track.setLastPlayed(lastPlayed);
        }
    }
    if(trackPlayCount != playCount) {
        if(trackPlayCount > playCount) {
            playCount     = trackPlayCount;
            dbNeedsUpdate = true;
        }
        else {
            track.setPlayCount(playCount);
        }
    }

    if(!dbNeedsUpdate) {
        return true;
    }

    const auto statement = QStringLiteral(
        "INSERT OR REPLACE INTO TrackStats (TrackHash, AddedDate, FirstPlayed, LastPlayed, PlayCount) VALUES "
        "(:trackHash, :addedDate, :firstPlayed, :lastPlayed, :playCount);");

    DbQuery query{db(), statement};

    query.bindValue(QStringLiteral(":trackHash"), track.hash());
    query.bindValue(QStringLiteral(":addedDate"), QVariant::fromValue(added));
    query.bindValue(QStringLiteral(":firstPlayed"), QVariant::fromValue(firstPlayed));
    query.bindValue(QStringLiteral(":lastPlayed"), QVariant::fromValue(lastPlayed));
    query.bindValue(QStringLiteral(":playCount"), playCount);

    return query.exec();
}

void TrackDatabase::removeUnmanagedTracks() const
{
    const auto statement = QStringLiteral(
        "DELETE FROM Tracks WHERE LibraryID = -1 AND TrackID NOT IN (SELECT TrackID FROM PlaylistTracks);");

    DbQuery query{db(), statement};

    query.exec();
}

void TrackDatabase::markUnusedStatsForDelete() const
{
    const auto statement
        = QStringLiteral("UPDATE TrackStats SET LastSeen = :lastSeen WHERE LastSeen IS NULL AND TrackHash NOT IN "
                         "(SELECT TrackHash FROM Tracks);");

    DbQuery query{db(), statement};

    query.bindValue(QStringLiteral(":lastSeen"), QDateTime::currentMSecsSinceEpoch());

    query.exec();
}

void TrackDatabase::deleteExpiredStats() const
{
    const auto statement
        = QStringLiteral("DELETE FROM TrackStats WHERE LastSeen IS NOT NULL AND LastSeen <= :clearInterval AND "
                         "TrackHash NOT IN (SELECT TrackHash FROM Tracks);");

    DbQuery query{db(), statement};

    query.bindValue(QStringLiteral(":clearInterval"), QDateTime::currentDateTime().addDays(-28).toMSecsSinceEpoch());

    query.exec();
}
} // namespace Fooyin
