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
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(TRK_DB, "fy.trackdb")

using namespace Qt::StringLiterals;

using BindingsMap = std::map<QString, QVariant>;

namespace {
QString fetchTrackColumns()
{
    static const QString columns = u"TrackID,"
                                   "FilePath,"
                                   "Subsong,"
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
                                   "CuePath,"
                                   "Offset,"
                                   "Duration,"
                                   "FileSize,"
                                   "BitRate,"
                                   "SampleRate,"
                                   "Channels,"
                                   "BitDepth,"
                                   "Codec,"
                                   "CodecProfile,"
                                   "Tool,"
                                   "TagTypes,"
                                   "Encoding,"
                                   "ExtraTags,"
                                   "ExtraProperties,"
                                   "ModifiedDate,"
                                   "LibraryID,"
                                   "TrackHash,"
                                   "RGTrackGain,"
                                   "RGAlbumGain,"
                                   "RGTrackPeak,"
                                   "RGAlbumPeak,"
                                   "AddedDate,"
                                   "FirstPlayed,"
                                   "LastPlayed,"
                                   "PlayCount,"
                                   "Rating"_s;

    return columns;
}

BindingsMap trackBindings(const Fooyin::Track& track)
{
    return {{u":filePath"_s, track.filepath()},
            {u":subsong"_s, track.subsong()},
            {u":title"_s, track.title()},
            {u":trackNumber"_s, track.trackNumber()},
            {u":trackTotal"_s, track.trackTotal()},
            {u":artists"_s, track.artist()},
            {u":albumArtist"_s, track.albumArtist()},
            {u":album"_s, track.album()},
            {u":discNumber"_s, track.discNumber()},
            {u":discTotal"_s, track.discTotal()},
            {u":date"_s, track.date()},
            {u":composer"_s, track.composer()},
            {u":performer"_s, track.performer()},
            {u":genres"_s, track.genre()},
            {u":comment"_s, track.comment()},
            {u":cuePath"_s, track.cuePath()},
            {u":offset"_s, static_cast<quint64>(track.offset())},
            {u":duration"_s, static_cast<quint64>(track.duration())},
            {u":fileSize"_s, static_cast<quint64>(track.fileSize())},
            {u":bitRate"_s, track.bitrate()},
            {u":sampleRate"_s, track.sampleRate()},
            {u":channels"_s, track.channels()},
            {u":bitDepth"_s, track.bitDepth()},
            {u":codec"_s, track.codec()},
            {u":codecProfile"_s, track.codecProfile()},
            {u":tool"_s, track.tool()},
            {u":tagTypes"_s, track.tagType()},
            {u":encoding"_s, track.encoding()},
            {u":extraTags"_s, track.serialiseExtraTags()},
            {u":extraProperties"_s, track.serialiseExtraProperties()},
            {u":modifiedDate"_s, static_cast<quint64>(track.modifiedTime())},
            {u":trackHash"_s, track.hash()},
            {u":libraryID"_s, track.libraryId()},
            {u":rgTrackGain"_s, track.rgTrackGain()},
            {u":rgAlbumGain"_s, track.rgAlbumGain()},
            {u":rgTrackPeak"_s, track.rgTrackPeak()},
            {u":rgAlbumPeak"_s, track.rgAlbumPeak()}};
}

Fooyin::Track readToTrack(const Fooyin::DbQuery& q)
{
    Fooyin::Track track;

    track.setId(q.value(0).toInt());
    track.setFilePath(q.value(1).toString());
    track.setSubsong(q.value(2).toInt());
    track.setTitle(q.value(3).toString());
    track.setTrackNumber(q.value(4).toString());
    track.setTrackTotal(q.value(5).toString());
    track.setArtists(q.value(6).toString().split(QLatin1String{Fooyin::Constants::UnitSeparator}));
    track.setAlbumArtists(q.value(7).toString().split(QLatin1String{Fooyin::Constants::UnitSeparator}));
    track.setAlbum(q.value(8).toString());
    track.setDiscNumber(q.value(9).toString());
    track.setDiscTotal(q.value(10).toString());
    track.setDate(q.value(11).toString());
    track.setComposers(q.value(12).toString().split(QLatin1String{Fooyin::Constants::UnitSeparator}));
    track.setPerformers(q.value(13).toString().split(QLatin1String{Fooyin::Constants::UnitSeparator}));
    track.setGenres(q.value(14).toString().split(QLatin1String{Fooyin::Constants::UnitSeparator}));
    track.setComment(q.value(15).toString());
    track.setCuePath(q.value(16).toString());
    track.setOffset(q.value(17).toULongLong());
    track.setDuration(q.value(18).toULongLong());
    track.setFileSize(q.value(19).toInt());
    track.setBitrate(q.value(20).toInt());
    track.setSampleRate(q.value(21).toInt());
    track.setChannels(q.value(22).toInt());
    track.setBitDepth(q.value(23).toInt());
    track.setCodec(q.value(24).toString());
    track.setCodecProfile(q.value(25).toString());
    track.setTool(q.value(26).toString());
    track.setTagTypes(q.value(27).toString().split(QLatin1String{Fooyin::Constants::UnitSeparator}));
    track.setEncoding(q.value(28).toString());
    track.storeExtraTags(q.value(29).toByteArray());
    track.storeExtraProperties(q.value(30).toByteArray());
    track.setModifiedTime(q.value(31).toULongLong());
    track.setLibraryId(q.value(32).toInt());
    track.setHash(q.value(33).toString());

    bool isValid{false};
    if(const auto rgTrackGain = q.value(34).toFloat(&isValid); isValid) {
        track.setRGTrackGain(rgTrackGain);
    }
    if(const auto rgAlbumGain = q.value(35).toFloat(&isValid); isValid) {
        track.setRGAlbumGain(rgAlbumGain);
    }
    if(const auto rgTrackPeak = q.value(36).toFloat(&isValid); isValid) {
        track.setRGTrackPeak(rgTrackPeak);
    }
    if(const auto rgAlbumPeak = q.value(37).toFloat(&isValid); isValid) {
        track.setRGAlbumPeak(rgAlbumPeak);
    }

    track.setAddedTime(q.value(38).toULongLong());
    track.setFirstPlayed(q.value(39).toULongLong());
    track.setLastPlayed(q.value(40).toULongLong());
    track.setPlayCount(q.value(41).toInt());
    track.setRating(q.value(42).toFloat());

    track.generateHash();

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
        if(track.id() < 0) {
            insertTrack(track);
        }
    }

    return transaction.commit();
}

bool TrackDatabase::updateTracks(TrackList& tracks)
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
    }

    return transaction.commit();
}

bool TrackDatabase::reloadTrack(Track& track) const
{
    const auto statement = u"SELECT %1 FROM TracksView WHERE TrackID = :trackId;"_s.arg(fetchTrackColumns());

    DbQuery query{db(), statement};

    query.bindValue(u":trackId"_s, track.id());

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
    const auto statement = u"SELECT %1 FROM TracksView WHERE TrackID IN (:trackIds);"_s.arg(fetchTrackColumns());

    DbQuery q{db(), statement};

    QStringList trackIds;
    std::transform(tracks.cbegin(), tracks.cend(), std::back_inserter(trackIds),
                   [](const Track& track) { return QString::number(track.id()); });

    q.bindValue(u":trackIds"_s, trackIds);

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
    const auto statement = u"SELECT %1 FROM TracksView"_s.arg(fetchTrackColumns());

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
    const auto statement = u"SELECT %1 FROM TracksView WHERE TrackHash = :trackHash"_s.arg(fetchTrackColumns());

    DbQuery q{db(), statement};

    q.bindValue(u":trackHash"_s, hash);

    TrackList tracks;

    if(!q.exec()) {
        return tracks;
    }

    while(q.next()) {
        tracks.emplace_back(readToTrack(q));
    }

    return tracks;
}

int TrackDatabase::idForTrack(Track& track) const
{
    const QString statement = u"SELECT TrackID FROM Tracks WHERE FilePath = :path;"_s;

    DbQuery query{db(), statement};

    query.bindValue(u":path"_s, track.filepath());

    if(!query.exec()) {
        return -1;
    }

    if(query.next()) {
        return query.value(0).toInt();
    }

    return -1;
}

bool TrackDatabase::updateTrack(const Track& track)
{
    if(track.id() < 0) {
        qCWarning(TRK_DB) << "Cannot update track" << track.filepath() << "(Invalid ID)";
        return false;
    }

    const auto statement = u"UPDATE TRACKS SET "
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
                           "LibraryID = :libraryID,"
                           "RGTrackGain = :rgTrackGain,"
                           "RGAlbumGain = :rgAlbumGain,"
                           "RGTrackPeak = :rgTrackPeak,"
                           "RGAlbumPeak = :rgAlbumPeak"
                           " WHERE TrackID = :trackId;"_s;

    DbQuery query{db(), statement};

    query.bindValue(u":trackId"_s, track.id());

    const auto bindings = trackBindings(track);
    for(const auto& [name, value] : bindings) {
        query.bindValue(name, value);
    }

    return query.exec();
}

bool TrackDatabase::updateTrackStats(const Track& track)
{
    return insertOrUpdateStats(track);
}

bool TrackDatabase::updateTrackStats(const TrackList& tracks)
{
    bool success{true};

    DbTransaction transaction{db()};

    for(const Track& track : tracks) {
        if(!insertOrUpdateStats(track)) {
            success = false;
        }
    }

    return success && transaction.commit();
}

bool TrackDatabase::deleteTrack(int id)
{
    const QString statement = u"DELETE FROM Tracks WHERE TrackID = :trackID;"_s;

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

    const int fileCount = static_cast<int>(
        std::count_if(tracks.cbegin(), tracks.cend(), [this](const Track& track) { return deleteTrack(track.id()); }));

    const auto success = transaction.commit();

    return (success && (fileCount == static_cast<int>(tracks.size())));
}

std::set<int> TrackDatabase::deleteLibraryTracks(int libraryId)
{
    std::set<int> tracksToRemove;

    {
        const auto statement = u"SELECT TrackID FROM Tracks WHERE LibraryID = :libraryId AND TrackID "
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
        const auto statement
            = u"DELETE FROM Tracks WHERE LibraryID = :libraryId AND TrackID NOT IN (SELECT TrackID FROM PlaylistTracks);"_s;

        DbQuery query{db(), statement};

        query.bindValue(u":libraryId"_s, libraryId);

        if(!query.exec()) {
            return {};
        }
    }

    const auto statement = u"UPDATE Tracks SET LibraryID = :nonLibraryId WHERE LibraryID = :libraryId;"_s;

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
    updateLastSeenStats();
    deleteExpiredStats();
}

void TrackDatabase::dropViews(const QSqlDatabase& db)
{
    const auto statement = u"DROP VIEW IF EXISTS TracksView;"_s;

    DbQuery query{db, statement};

    query.exec();
}

void TrackDatabase::insertViews(const QSqlDatabase& db)
{
    const auto statement = u"CREATE VIEW IF NOT EXISTS TracksView AS "
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
                           "Tracks.RGTrackGain,"
                           "Tracks.RGAlbumGain,"
                           "Tracks.RGTrackPeak,"
                           "Tracks.RGAlbumPeak,"
                           "TrackStats.AddedDate,"
                           "TrackStats.FirstPlayed,"
                           "TrackStats.LastPlayed,"
                           "TrackStats.PlayCount,"
                           "TrackStats.Rating"
                           " FROM Tracks "
                           "LEFT JOIN TrackStats ON Tracks.TrackHash = TrackStats.TrackHash;"_s;

    DbQuery query{db, statement};

    query.exec();
}

int TrackDatabase::trackCount() const
{
    const auto statement = u"SELECT COUNT(*) FROM Tracks;"_s;

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
    const auto statement = u"INSERT INTO Tracks ("
                           "FilePath,"
                           "Subsong,"
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
                           "CuePath,"
                           "Offset,"
                           "Duration,"
                           "FileSize,"
                           "BitRate,"
                           "SampleRate,"
                           "Channels,"
                           "BitDepth,"
                           "Codec,"
                           "CodecProfile,"
                           "Tool,"
                           "TagTypes,"
                           "Encoding,"
                           "ExtraTags,"
                           "ExtraProperties,"
                           "ModifiedDate,"
                           "TrackHash,"
                           "LibraryID,"
                           "RGTrackGain,"
                           "RGAlbumGain,"
                           "RGTrackPeak,"
                           "RGAlbumPeak"
                           ") "
                           "VALUES ("
                           ":filePath,"
                           ":subsong,"
                           ":title, "
                           ":trackNumber,"
                           ":trackTotal,"
                           ":artists,"
                           ":albumArtist,"
                           ":album,"
                           ":discNumber,"
                           ":discTotal,"
                           ":date, "
                           ":composer,"
                           ":performer,"
                           ":genres,"
                           ":comment,"
                           ":cuePath,"
                           ":offset,"
                           ":duration,"
                           ":fileSize,"
                           ":bitRate,"
                           ":sampleRate,"
                           ":channels,"
                           ":bitDepth,"
                           ":codec,"
                           ":codecProfile,"
                           ":tool,"
                           ":tagTypes,"
                           ":encoding,"
                           ":extraTags,"
                           ":extraProperties,"
                           ":modifiedDate,"
                           ":trackHash,"
                           ":libraryID,"
                           ":rgTrackGain,"
                           ":rgAlbumGain,"
                           ":rgTrackPeak,"
                           ":rgAlbumPeak"
                           ");"_s;

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

bool TrackDatabase::insertOrUpdateStats(const Track& track) const
{
    if(track.hash().isEmpty()) {
        qCWarning(TRK_DB) << "Cannot insert/update track stats (Hash empty)";
        return false;
    }

    uint64_t added{0};
    uint64_t firstPlayed{0};
    uint64_t lastPlayed{0};
    int playCount{0};
    float rating{0};

    {
        const auto statement = u"SELECT AddedDate, FirstPlayed, LastPlayed, PlayCount, Rating FROM "
                               "TrackStats WHERE TrackHash = :trackHash;"_s;

        DbQuery query{db(), statement};

        query.bindValue(u":trackHash"_s, track.hash());

        if(!query.exec()) {
            return false;
        }

        if(query.next()) {
            added       = query.value(0).toULongLong();
            firstPlayed = query.value(1).toULongLong();
            lastPlayed  = query.value(2).toULongLong();
            playCount   = query.value(3).toInt();
            rating      = query.value(4).toFloat();
        }
    }

    bool dbNeedsUpdate{false};

    const uint64_t trackAdded       = track.addedTime();
    const uint64_t trackFirstPlayed = track.firstPlayed();
    const uint64_t trackLastPlayed  = track.lastPlayed();
    const int trackPlayCount        = track.playCount();
    const float trackRating         = track.rating();

    if(trackAdded != added) {
        if(added == 0 || (trackAdded > 0 && trackAdded < added)) {
            added         = trackAdded;
            dbNeedsUpdate = true;
        }
    }
    if(trackFirstPlayed != firstPlayed) {
        if(firstPlayed == 0 || (trackFirstPlayed > 0 && trackFirstPlayed < firstPlayed)) {
            firstPlayed   = trackFirstPlayed;
            dbNeedsUpdate = true;
        }
    }
    if(trackLastPlayed != lastPlayed) {
        if(trackLastPlayed > lastPlayed) {
            lastPlayed    = trackLastPlayed;
            dbNeedsUpdate = true;
        }
    }
    if(trackPlayCount != playCount) {
        if(trackPlayCount > playCount) {
            playCount     = trackPlayCount;
            dbNeedsUpdate = true;
        }
    }
    if(trackRating != rating) {
        rating        = trackRating;
        dbNeedsUpdate = true;
    }

    if(!dbNeedsUpdate) {
        return true;
    }

    const auto statement = u"INSERT OR REPLACE INTO TrackStats (TrackHash, AddedDate, FirstPlayed, LastPlayed, "
                           u"PlayCount, Rating) VALUES "
                           "(:trackHash, :addedDate, :firstPlayed, :lastPlayed, :playCount, :rating);"_s;

    DbQuery query{db(), statement};

    query.bindValue(u":trackHash"_s, track.hash());
    query.bindValue(u":addedDate"_s, QVariant::fromValue(added));
    query.bindValue(u":firstPlayed"_s, QVariant::fromValue(firstPlayed));
    query.bindValue(u":lastPlayed"_s, QVariant::fromValue(lastPlayed));
    query.bindValue(u":playCount"_s, playCount);
    query.bindValue(u":rating"_s, rating);

    return query.exec();
}

void TrackDatabase::removeUnmanagedTracks() const
{
    const auto statement
        = u"DELETE FROM Tracks WHERE LibraryID = -1 AND TrackID NOT IN (SELECT TrackID FROM PlaylistTracks);"_s;

    DbQuery query{db(), statement};

    query.exec();
}

void TrackDatabase::updateLastSeenStats() const
{
    const auto markStatement
        = u"UPDATE TrackStats SET LastSeen = :lastSeen WHERE LastSeen IS NULL AND TrackHash NOT IN "
          "(SELECT TrackHash FROM Tracks);"_s;

    DbQuery markQuery{db(), markStatement};
    markQuery.bindValue(u":lastSeen"_s, QDateTime::currentMSecsSinceEpoch());
    markQuery.exec();

    const auto unmarkStatement = u"UPDATE TrackStats SET LastSeen = NULL WHERE LastSeen IS NOT NULL AND TrackHash IN "
                                 "(SELECT TrackHash FROM Tracks);"_s;

    DbQuery unmarkQuery{db(), unmarkStatement};
    unmarkQuery.exec();
}

void TrackDatabase::deleteExpiredStats() const
{
    const auto statement = u"DELETE FROM TrackStats WHERE LastSeen IS NOT NULL AND LastSeen <= :clearInterval AND "
                           "TrackHash NOT IN (SELECT TrackHash FROM Tracks);"_s;

    DbQuery query{db(), statement};

    query.bindValue(u":clearInterval"_s, QDateTime::currentDateTime().addDays(-28).toMSecsSinceEpoch());

    query.exec();
}
} // namespace Fooyin
