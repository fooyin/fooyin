/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "databasequery.h"

#include <core/constants.h>
#include <core/track.h>
#include <utils/fileutils.h>

#include <QDateTime>

using namespace Qt::Literals::StringLiterals;

namespace {
QString fetchQueryTracks()
{
    static const QStringList fields = {
        u"TrackID"_s,      // 0
        u"FilePath"_s,     // 1
        u"RelativePath"_s, // 2
        u"Title"_s,        // 3
        u"TrackNumber"_s,  // 4
        u"TrackTotal"_s,   // 5
        u"Artists"_s,      // 6
        u"AlbumArtist"_s,  // 7
        u"Album"_s,        // 8
        u"CoverPath"_s,    // 9
        u"DiscNumber"_s,   // 10
        u"DiscTotal"_s,    // 11
        u"Date"_s,         // 12
        u"Composer"_s,     // 13
        u"Performer"_s,    // 14
        u"Genres"_s,       // 15
        u"Lyrics"_s,       // 16
        u"Comment"_s,      // 17
        u"Duration"_s,     // 18
        u"FileSize"_s,     // 19
        u"BitRate"_s,      // 20
        u"SampleRate"_s,   // 21
        u"ExtraTags"_s,    // 22
        u"Type"_s,         // 23
        u"ModifiedDate"_s, // 24
        u"LibraryID"_s,    // 25
        u"TrackHash"_s,    // 26
        u"AddedDate"_s,    // 27
        u"FirstPlayed"_s,  // 28
        u"LastPlayed"_s,   // 29
        u"PlayCount"_s,    // 30
        u"Rating"_s,       // 31
    };

    const QString joinedFields = fields.join(", "_L1);

    return QString(u"SELECT %1 FROM TracksView"_s).arg(joinedFields);
}

Fooyin::BindingsMap trackBindings(const Fooyin::Track& track)
{
    return {{u"FilePath"_s, Fooyin::Utils::File::cleanPath(track.filepath())},
            {u"Title"_s, track.title()},
            {u"TrackNumber"_s, QString::number(track.trackNumber())},
            {u"TrackTotal"_s, QString::number(track.trackTotal())},
            {u"Artists"_s, track.artists().join(Fooyin::Constants::Separator)},
            {u"AlbumArtist"_s, track.albumArtist()},
            {u"Album"_s, track.album()},
            {u"CoverPath"_s, track.coverPath()},
            {u"DiscNumber"_s, QString::number(track.discNumber())},
            {u"DiscTotal"_s, QString::number(track.discTotal())},
            {u"Date"_s, track.date()},
            {u"Composer"_s, track.composer()},
            {u"Performer"_s, track.performer()},
            {u"Genres"_s, track.genres().join(Fooyin::Constants::Separator)},
            {u"Lyrics"_s, track.lyrics()},
            {u"Comment"_s, track.comment()},
            {u"Duration"_s, QString::number(track.duration())},
            {u"FileSize"_s, QString::number(track.fileSize())},
            {u"BitRate"_s, QString::number(track.bitrate())},
            {u"SampleRate"_s, QString::number(track.sampleRate())},
            {u"ExtraTags"_s, track.serialiseExtrasTags()},
            {u"Type"_s, QString::number(static_cast<int>(track.type()))},
            {u"ModifiedDate"_s, QString::number(track.modifiedTime())},
            {u"TrackHash"_s, track.hash()},
            {u"LibraryID"_s, QString::number(track.libraryId())}};
}

void readToTrack(const Fooyin::DatabaseQuery& q, Fooyin::Track& track)
{
    track.setId(q.value(0).toInt());
    track.setFilePath(q.value(1).toString());
    track.setRelativePath(q.value(2).toString());
    track.setTitle(q.value(3).toString());
    track.setTrackNumber(q.value(4).toInt());
    track.setTrackTotal(q.value(5).toInt());
    track.setArtists(q.value(6).toString().split(Fooyin::Constants::Separator, Qt::SkipEmptyParts));
    track.setAlbumArtist(q.value(7).toString());
    track.setAlbum(q.value(8).toString());
    track.setCoverPath(q.value(9).toString());
    track.setDiscNumber(q.value(10).toInt());
    track.setDiscTotal(q.value(11).toInt());
    track.setDate(q.value(12).toString());
    track.setComposer(q.value(13).toString());
    track.setPerformer(q.value(14).toString());
    track.setGenres(q.value(15).toString().split(Fooyin::Constants::Separator, Qt::SkipEmptyParts));
    track.setLyrics(q.value(16).toString());
    track.setComment(q.value(17).toString());
    track.setDuration(q.value(18).toULongLong());
    track.setFileSize(q.value(19).toInt());
    track.setBitrate(q.value(20).toInt());
    track.setSampleRate(q.value(21).toInt());
    track.storeExtraTags(q.value(22).toByteArray());
    track.setType(static_cast<Fooyin::Track::Type>(q.value(23).toInt()));
    track.setModifiedTime(q.value(24).toULongLong());
    track.setLibraryId(q.value(25).toInt());
    track.setHash(q.value(26).toString());
    track.setAddedTime(q.value(27).toULongLong());
    track.setFirstPlayed(q.value(28).toULongLong());
    track.setLastPlayed(q.value(29).toULongLong());
    track.setPlayCount(q.value(30).toInt());

    track.generateHash();
}
} // namespace

namespace Fooyin {
struct TrackDatabase::Private
{
    TrackDatabase* self;

    explicit Private(TrackDatabase* self)
        : self{self}
    { }

    void removeUnmanagedTracks() const
    {
        QString queryText
            = u"DELETE FROM Tracks WHERE LibraryID = -1 AND TrackID NOT IN (SELECT TrackID FROM PlaylistTracks);"_s;
        const auto q = self->runQuery(queryText, u"Cannot cleanup tracks"_s);
    }

    void markUnusedStatsForDelete() const
    {
        const QString queryText
            = "UPDATE TrackStats SET LastSeen = :lastSeen WHERE LastSeen IS NULL AND TrackHash NOT IN "
              "(SELECT TrackHash FROM Tracks);";
        const auto q
            = self->runQuery(queryText, {{u":lastSeen"_s, QString::number(QDateTime::currentMSecsSinceEpoch())}},
                             u"Error updating LastSeen"_s);
    }

    void deleteExpiredStats() const
    {
        const QString queryText
            = "DELETE FROM TrackStats WHERE LastSeen IS NOT NULL AND LastSeen <= :clearInterval AND "
              "TrackHash NOT IN (SELECT TrackHash FROM Tracks);";
        const auto q = self->runQuery(
            queryText,
            {{u":clearInterval"_s, QString::number(QDateTime::currentDateTime().addDays(-28).toMSecsSinceEpoch())}},
            u"Error deleting expired track stats"_s);
    }

    int trackCount() const
    {
        auto q = self->runQuery(u"SELECT COUNT(*) FROM Tracks"_s, u"Cannot fetch track count"_s);

        if(!q.hasError() && q.next()) {
            return q.value(0).toInt();
        }
        return -1;
    }

    bool insertTrack(Track& track) const
    {
        auto bindings = trackBindings(track);
        const auto q  = self->insert(u"Tracks"_s, bindings, "Cannot insert track " + track.filepath());

        if(q.hasError()) {
            return false;
        }

        track.setId(q.lastInsertId().toInt());

        return insertOrUpdateStats(track);
    }

    bool insertOrUpdateStats(Track& track) const
    {
        if(track.hash().isEmpty()) {
            qDebug() << "Cannot insert/update track stats (Hash empty)";
            return false;
        }

        static const QStringList fields = {
            u"AddedDate"_s,   // 0
            u"FirstPlayed"_s, // 1
            u"LastPlayed"_s,  // 2
            u"PlayCount"_s,   // 3
            u"Rating"_s,      // 4
        };

        const QString joinedFields = fields.join(", "_L1);

        const auto existsQuery
            = QString{u"SELECT %1 FROM TrackStats WHERE TrackHash = :trackHash;"_s}.arg(joinedFields);
        auto exists = self->runQuery(existsQuery, std::make_pair(u":trackHash"_s, track.hash()),
                                     u"Couldn't check stats for track "_s + track.filepath());

        if(exists.hasError()) {
            return false;
        }

        if(exists.next()) {
            const auto added       = static_cast<uint64_t>(exists.value(0).toULongLong());
            const auto firstPlayed = static_cast<uint64_t>(exists.value(1).toULongLong());
            const auto lastPlayed  = static_cast<uint64_t>(exists.value(2).toULongLong());
            const int playCount    = exists.value(3).toInt();

            bool changed{false};
            BindingsMap bindings;

            const uint64_t trackAdded       = track.addedTime();
            const uint64_t trackFirstPlayed = track.firstPlayed();
            const uint64_t trackLastPlayed  = track.lastPlayed();
            const int trackPlayCount        = track.playCount();

            if(trackAdded != added) {
                if(added == 0 || (trackAdded > 0 && trackAdded < added)) {
                    changed = true;
                    bindings.emplace(u"AddedDate"_s, QString::number(trackAdded));
                }
                else {
                    track.setAddedTime(added);
                }
            }
            if(trackFirstPlayed != firstPlayed) {
                if(firstPlayed == 0 || (trackFirstPlayed > 0 && trackFirstPlayed < firstPlayed)) {
                    changed = true;
                    bindings.emplace(u"FirstPlayed"_s, QString::number(trackFirstPlayed));
                }
                else {
                    track.setFirstPlayed(firstPlayed);
                }
            }
            if(trackLastPlayed != lastPlayed) {
                if(trackLastPlayed > lastPlayed) {
                    changed = true;
                    bindings.emplace(u"LastPlayed"_s, QString::number(trackLastPlayed));
                }
                else {
                    track.setLastPlayed(lastPlayed);
                }
            }
            if(trackPlayCount != playCount) {
                if(trackPlayCount > playCount) {
                    changed = true;
                    bindings.emplace(u"PlayCount"_s, QString::number(trackPlayCount));
                }
                else {
                    track.setPlayCount(playCount);
                }
            }

            if(changed) {
                const auto q = self->update(u"TrackStats"_s, bindings, {u"TrackHash"_s, track.hash()},
                                            u"Cannot update stats for track "_s + track.filepath());
                return !q.hasError();
            }

            return changed;
        }

        const BindingsMap bindings = {{u"TrackHash"_s, track.hash()},
                                      {u"AddedDate"_s, QString::number(track.addedTime())},
                                      {u"FirstPlayed"_s, QString::number(track.firstPlayed())},
                                      {u"LastPlayed"_s, QString::number(track.lastPlayed())},
                                      {u"PlayCount"_s, QString::number(track.playCount())}};

        const auto q = self->insert(u"TrackStats"_s, bindings, u"Cannot insert stats for track "_s + track.filepath());
        return !q.hasError();
    }
};

TrackDatabase::TrackDatabase(const QString& connectionName)
    : DatabaseModule{connectionName}
    , p{std::make_unique<Private>(this)}
{ }

TrackDatabase::~TrackDatabase() = default;

bool TrackDatabase::storeTracks(TrackList& tracks)
{
    if(tracks.empty()) {
        return true;
    }

    if(!db().transaction()) {
        qDebug() << "Transaction could not be started";
        return false;
    }

    for(auto& track : tracks) {
        if(track.id() >= 0) {
            updateTrack(track);
        }
        else {
            p->insertTrack(track);
        }
    }

    if(!db().commit()) {
        qDebug() << "Transaction could not be commited";
        return false;
    }

    return true;
}

bool TrackDatabase::reloadTrack(Track& track) const
{
    DatabaseQuery q{this};
    const QString query = fetchQueryTracks() + u" WHERE TrackID = :trackId;"_s;
    q.prepareQuery(query);
    q.bindQueryValue(u"trackId"_s, track.id());

    if(!q.execQuery()) {
        q.error(u"Cannot fetch tracks from database"_s);
        return false;
    }

    if(q.next()) {
        readToTrack(q, track);
    }

    return true;
}

bool TrackDatabase::reloadTracks(TrackList& tracks) const
{
    bool success{true};

    for(auto& track : tracks) {
        if(!reloadTrack(track)) {
            success = false;
        }
    }

    return success;
}

TrackList TrackDatabase::getAllTracks() const
{
    DatabaseQuery q{this};
    const QString query = fetchQueryTracks();
    q.prepareQuery(query);

    TrackList tracks;

    if(!q.execQuery()) {
        q.error(u"Cannot fetch tracks from database"_s);
        return {};
    }

    const int numRows = p->trackCount();
    if(numRows > 0) {
        tracks.reserve(numRows);
    }

    while(q.next()) {
        Track track;
        readToTrack(q, track);
        tracks.push_back(track);
    }

    return tracks;
}

TrackList TrackDatabase::tracksByHash(const QString& hash) const
{
    DatabaseQuery q{this};
    const QString query = fetchQueryTracks() + " WHERE TrackHash = :trackHash";
    q.prepareQuery(query);
    q.bindQueryValue(u":trackHash"_s, hash);

    TrackList tracks;

    if(!q.execQuery()) {
        q.error(u"Cannot fetch tracks from database"_s);
        return tracks;
    }

    while(q.next()) {
        Track track;
        readToTrack(q, track);
        tracks.push_back(track);
    }

    return tracks;
}

bool TrackDatabase::updateTrack(const Track& track)
{
    if(track.id() < 0 || track.libraryId() < 0) {
        qDebug() << QString{"Cannot update track %1 (TrackID: %2, LibraryID: %3)"}
                        .arg(track.filepath())
                        .arg(track.id())
                        .arg(track.libraryId());
        return false;
    }

    auto bindings = trackBindings(track);
    const auto q  = update(u"Tracks"_s, bindings, {u"TrackID"_s, QString::number(track.id())},
                           "Cannot update track " + track.filepath());

    return !q.hasError();
}

bool TrackDatabase::updateTrackStats(Track& track)
{
    return p->insertOrUpdateStats(track);
}

bool TrackDatabase::deleteTrack(int id)
{
    const QString queryText = u"DELETE FROM Tracks WHERE TrackID = :trackID;"_s;
    const auto q = runQuery(queryText, {":trackID", QString::number(id)}, "Cannot delete track " + QString::number(id));

    return !q.hasError();
}

bool TrackDatabase::deleteTracks(const TrackList& tracks)
{
    if(tracks.empty()) {
        return true;
    }

    if(!db().transaction()) {
        qDebug() << "Transaction could not be started";
        return false;
    }

    const int fileCount = static_cast<int>(
        std::ranges::count_if(std::as_const(tracks), [this](const Track& track) { return deleteTrack(track.id()); }));

    const auto success = db().commit();

    return (success && (fileCount == static_cast<int>(tracks.size())));
}

std::set<int> TrackDatabase::deleteLibraryTracks(int libraryId)
{
    auto selectToRemove = runQuery(
        u"SELECT TrackID FROM Tracks WHERE LibraryID = :libraryId AND TrackID NOT IN (SELECT TrackID FROM PlaylistTracks);"_s,
        {{":libraryId", QString::number(libraryId)}}, "Cannot get library tracks for " + QString::number(libraryId));

    if(selectToRemove.hasError()) {
        return {};
    }

    std::set<int> tracksToRemove;

    while(selectToRemove.next()) {
        tracksToRemove.emplace(selectToRemove.value(0).toInt());
    }

    auto deleteTracks = runQuery(
        u"DELETE FROM Tracks WHERE LibraryID = :libraryId AND TrackID NOT IN (SELECT TrackID FROM PlaylistTracks);"_s,
        {{":libraryId", QString::number(libraryId)}}, "Cannot get library tracks for " + QString::number(libraryId));

    auto updateTracks = runQuery(u"UPDATE Tracks SET LibraryID = :nonLibraryId WHERE LibraryID = :libraryId;"_s,
                                 {{":nonLibraryId", "-1"}, {":libraryId", QString::number(libraryId)}},
                                 "Cannot get library tracks for " + QString::number(libraryId));

    return tracksToRemove;
}

void TrackDatabase::cleanupTracks()
{
    p->removeUnmanagedTracks();
    p->markUnusedStatsForDelete();
    p->deleteExpiredStats();
}
} // namespace Fooyin
