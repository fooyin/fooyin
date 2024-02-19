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

#include "databasequery.h"

#include <core/constants.h>
#include <core/track.h>
#include <utils/fileutils.h>

#include <QFileInfo>

namespace {
QString fetchQueryTracks()
{
    static const QStringList fields = {
        QStringLiteral("TrackID"),      // 0
        QStringLiteral("FilePath"),     // 1
        QStringLiteral("RelativePath"), // 2
        QStringLiteral("Title"),        // 3
        QStringLiteral("TrackNumber"),  // 4
        QStringLiteral("TrackTotal"),   // 5
        QStringLiteral("Artists"),      // 6
        QStringLiteral("AlbumArtist"),  // 7
        QStringLiteral("Album"),        // 8
        QStringLiteral("CoverPath"),    // 9
        QStringLiteral("DiscNumber"),   // 10
        QStringLiteral("DiscTotal"),    // 11
        QStringLiteral("Date"),         // 12
        QStringLiteral("Composer"),     // 13
        QStringLiteral("Performer"),    // 14
        QStringLiteral("Genres"),       // 15
        QStringLiteral("Comment"),      // 16
        QStringLiteral("Duration"),     // 17
        QStringLiteral("FileSize"),     // 18
        QStringLiteral("BitRate"),      // 19
        QStringLiteral("SampleRate"),   // 20
        QStringLiteral("ExtraTags"),    // 21
        QStringLiteral("Type"),         // 22
        QStringLiteral("ModifiedDate"), // 23
        QStringLiteral("LibraryID"),    // 24
        QStringLiteral("TrackHash"),    // 25
        QStringLiteral("AddedDate"),    // 26
        QStringLiteral("FirstPlayed"),  // 27
        QStringLiteral("LastPlayed"),   // 28
        QStringLiteral("PlayCount"),    // 29
        QStringLiteral("Rating"),       // 30
    };

    const QString joinedFields = fields.join(QStringLiteral(", "));

    return QString(QStringLiteral("SELECT %1 FROM TracksView")).arg(joinedFields);
}

Fooyin::BindingsMap trackBindings(const Fooyin::Track& track)
{
    return {{QStringLiteral("FilePath"), Fooyin::Utils::File::cleanPath(track.filepath())},
            {QStringLiteral("Title"), track.title()},
            {QStringLiteral("TrackNumber"), QString::number(track.trackNumber())},
            {QStringLiteral("TrackTotal"), QString::number(track.trackTotal())},
            {QStringLiteral("Artists"), track.artists().join(Fooyin::Constants::Separator)},
            {QStringLiteral("AlbumArtist"), track.albumArtist()},
            {QStringLiteral("Album"), track.album()},
            {QStringLiteral("CoverPath"), track.coverPath()},
            {QStringLiteral("DiscNumber"), QString::number(track.discNumber())},
            {QStringLiteral("DiscTotal"), QString::number(track.discTotal())},
            {QStringLiteral("Date"), track.date()},
            {QStringLiteral("Composer"), track.composer()},
            {QStringLiteral("Performer"), track.performer()},
            {QStringLiteral("Genres"), track.genres().join(Fooyin::Constants::Separator)},
            {QStringLiteral("Comment"), track.comment()},
            {QStringLiteral("Duration"), QString::number(track.duration())},
            {QStringLiteral("FileSize"), QString::number(track.fileSize())},
            {QStringLiteral("BitRate"), QString::number(track.bitrate())},
            {QStringLiteral("SampleRate"), QString::number(track.sampleRate())},
            {QStringLiteral("ExtraTags"), track.serialiseExtrasTags()},
            {QStringLiteral("Type"), QString::number(static_cast<int>(track.type()))},
            {QStringLiteral("ModifiedDate"), QString::number(track.modifiedTime())},
            {QStringLiteral("TrackHash"), track.hash()},
            {QStringLiteral("LibraryID"), QString::number(track.libraryId())}};
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
    track.setComment(q.value(16).toString());
    track.setDuration(q.value(17).toULongLong());
    track.setFileSize(q.value(18).toInt());
    track.setBitrate(q.value(19).toInt());
    track.setSampleRate(q.value(20).toInt());
    track.storeExtraTags(q.value(21).toByteArray());
    track.setType(static_cast<Fooyin::Track::Type>(q.value(22).toInt()));
    track.setModifiedTime(q.value(23).toULongLong());
    track.setLibraryId(q.value(24).toInt());
    track.setHash(q.value(25).toString());
    track.setAddedTime(q.value(26).toULongLong());
    track.setFirstPlayed(q.value(27).toULongLong());
    track.setLastPlayed(q.value(28).toULongLong());
    track.setPlayCount(q.value(29).toInt());

    track.generateHash();
    track.setEnabled(QFileInfo::exists(track.filepath()));
}
} // namespace

namespace Fooyin {
struct TrackDatabase::Private
{
    TrackDatabase* self;

    explicit Private(TrackDatabase* self_)
        : self{self_}
    { }

    void removeUnmanagedTracks() const
    {
        const QString queryText = QStringLiteral(
            "DELETE FROM Tracks WHERE LibraryID = -1 AND TrackID NOT IN (SELECT TrackID FROM PlaylistTracks);");
        const auto q = self->runQuery(queryText, QStringLiteral("Cannot cleanup tracks"));
    }

    void markUnusedStatsForDelete() const
    {
        const QString queryText
            = "UPDATE TrackStats SET LastSeen = :lastSeen WHERE LastSeen IS NULL AND TrackHash NOT IN "
              "(SELECT TrackHash FROM Tracks);";
        const auto q = self->runQuery(
            queryText, {{QStringLiteral(":lastSeen"), QString::number(QDateTime::currentMSecsSinceEpoch())}},
            QStringLiteral("Error updating LastSeen"));
    }

    void deleteExpiredStats() const
    {
        const QString queryText
            = "DELETE FROM TrackStats WHERE LastSeen IS NOT NULL AND LastSeen <= :clearInterval AND "
              "TrackHash NOT IN (SELECT TrackHash FROM Tracks);";
        const auto q
            = self->runQuery(queryText,
                             {{QStringLiteral(":clearInterval"),
                               QString::number(QDateTime::currentDateTime().addDays(-28).toMSecsSinceEpoch())}},
                             QStringLiteral("Error deleting expired track stats"));
    }

    int trackCount() const
    {
        auto q
            = self->runQuery(QStringLiteral("SELECT COUNT(*) FROM Tracks"), QStringLiteral("Cannot fetch track count"));

        if(!q.hasError() && q.next()) {
            return q.value(0).toInt();
        }
        return -1;
    }

    bool insertTrack(Track& track) const
    {
        auto bindings = trackBindings(track);
        const auto q  = self->insert(QStringLiteral("Tracks"), bindings, "Cannot insert track " + track.filepath());

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
            QStringLiteral("AddedDate"),   // 0
            QStringLiteral("FirstPlayed"), // 1
            QStringLiteral("LastPlayed"),  // 2
            QStringLiteral("PlayCount"),   // 3
            QStringLiteral("Rating"),      // 4
        };

        const QString joinedFields = fields.join(QStringLiteral(", "));

        const auto existsQuery
            = QString{QStringLiteral("SELECT %1 FROM TrackStats WHERE TrackHash = :trackHash;")}.arg(joinedFields);
        auto exists = self->runQuery(existsQuery, std::make_pair(QStringLiteral(":trackHash"), track.hash()),
                                     QStringLiteral("Couldn't check stats for track ") + track.filepath());

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
                    bindings.emplace(QStringLiteral("AddedDate"), QString::number(trackAdded));
                }
                else {
                    track.setAddedTime(added);
                }
            }
            if(trackFirstPlayed != firstPlayed) {
                if(firstPlayed == 0 || (trackFirstPlayed > 0 && trackFirstPlayed < firstPlayed)) {
                    changed = true;
                    bindings.emplace(QStringLiteral("FirstPlayed"), QString::number(trackFirstPlayed));
                }
                else {
                    track.setFirstPlayed(firstPlayed);
                }
            }
            if(trackLastPlayed != lastPlayed) {
                if(trackLastPlayed > lastPlayed) {
                    changed = true;
                    bindings.emplace(QStringLiteral("LastPlayed"), QString::number(trackLastPlayed));
                }
                else {
                    track.setLastPlayed(lastPlayed);
                }
            }
            if(trackPlayCount != playCount) {
                if(trackPlayCount > playCount) {
                    changed = true;
                    bindings.emplace(QStringLiteral("PlayCount"), QString::number(trackPlayCount));
                }
                else {
                    track.setPlayCount(playCount);
                }
            }

            if(changed) {
                const auto q
                    = self->update(QStringLiteral("TrackStats"), bindings, {QStringLiteral("TrackHash"), track.hash()},
                                   QStringLiteral("Cannot update stats for track ") + track.filepath());
                return !q.hasError();
            }

            return changed;
        }

        const BindingsMap bindings = {{QStringLiteral("TrackHash"), track.hash()},
                                      {QStringLiteral("AddedDate"), QString::number(track.addedTime())},
                                      {QStringLiteral("FirstPlayed"), QString::number(track.firstPlayed())},
                                      {QStringLiteral("LastPlayed"), QString::number(track.lastPlayed())},
                                      {QStringLiteral("PlayCount"), QString::number(track.playCount())}};

        const auto q = self->insert(QStringLiteral("TrackStats"), bindings,
                                    QStringLiteral("Cannot insert stats for track ") + track.filepath());
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
    const QString query = fetchQueryTracks() + QStringLiteral(" WHERE TrackID = :trackId;");
    q.prepareQuery(query);
    q.bindQueryValue(QStringLiteral("trackId"), track.id());

    if(!q.execQuery()) {
        q.error(QStringLiteral("Cannot reload track from database"));
        return false;
    }

    if(q.next()) {
        readToTrack(q, track);
    }

    return true;
}

bool TrackDatabase::reloadTracks(TrackList& tracks) const
{
    DatabaseQuery q{this};
    const QString query = fetchQueryTracks() + QStringLiteral(" WHERE TrackID IN (:trackIds);");
    q.prepareQuery(query);

    QStringList trackIds;
    std::ranges::transform(tracks, std::back_inserter(trackIds),
                           [](const Track& track) { return QString::number(track.id()); });

    q.bindQueryValue(QStringLiteral("trackIds"), trackIds);

    if(!q.execQuery()) {
        q.error(QStringLiteral("Cannot reload tracks from database"));
        return false;
    }

    tracks.clear();

    while(q.next()) {
        Track track;
        readToTrack(q, track);
        tracks.push_back(std::move(track));
    }

    return true;
}

TrackList TrackDatabase::getAllTracks() const
{
    DatabaseQuery q{this};
    const QString query = fetchQueryTracks();
    q.prepareQuery(query);

    TrackList tracks;

    if(!q.execQuery()) {
        q.error(QStringLiteral("Cannot fetch tracks from database"));
        return {};
    }

    const int numRows = p->trackCount();
    if(numRows > 0) {
        tracks.reserve(numRows);
    }

    while(q.next()) {
        Track track;
        readToTrack(q, track);
        tracks.push_back(std::move(track));
    }

    return tracks;
}

TrackList TrackDatabase::tracksByHash(const QString& hash) const
{
    DatabaseQuery q{this};
    const QString query = fetchQueryTracks() + " WHERE TrackHash = :trackHash";
    q.prepareQuery(query);
    q.bindQueryValue(QStringLiteral(":trackHash"), hash);

    TrackList tracks;

    if(!q.execQuery()) {
        q.error(QStringLiteral("Cannot fetch tracks from database"));
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
    if(track.id() < 0) {
        qDebug() << QString{"Cannot update track %1 (Invalid ID)"}.arg(track.filepath());
        return false;
    }

    auto bindings = trackBindings(track);
    const auto q  = update(QStringLiteral("Tracks"), bindings, {QStringLiteral("TrackID"), QString::number(track.id())},
                           "Cannot update track " + track.filepath());

    return !q.hasError();
}

bool TrackDatabase::updateTrackStats(Track& track)
{
    return p->insertOrUpdateStats(track);
}

bool TrackDatabase::deleteTrack(int id)
{
    const QString queryText = QStringLiteral("DELETE FROM Tracks WHERE TrackID = :trackID;");
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
    auto selectToRemove = runQuery(QStringLiteral("SELECT TrackID FROM Tracks WHERE LibraryID = :libraryId AND TrackID "
                                                  "NOT IN (SELECT TrackID FROM PlaylistTracks);"),
                                   {{":libraryId", QString::number(libraryId)}},
                                   "Cannot get library tracks for " + QString::number(libraryId));

    if(selectToRemove.hasError()) {
        return {};
    }

    std::set<int> tracksToRemove;

    while(selectToRemove.next()) {
        tracksToRemove.emplace(selectToRemove.value(0).toInt());
    }

    auto deleteTracks = runQuery(
        QStringLiteral(
            "DELETE FROM Tracks WHERE LibraryID = :libraryId AND TrackID NOT IN (SELECT TrackID FROM PlaylistTracks);"),
        {{":libraryId", QString::number(libraryId)}}, "Cannot get library tracks for " + QString::number(libraryId));

    auto updateTracks
        = runQuery(QStringLiteral("UPDATE Tracks SET LibraryID = :nonLibraryId WHERE LibraryID = :libraryId;"),
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
