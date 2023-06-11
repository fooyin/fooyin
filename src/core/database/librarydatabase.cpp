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

#include "librarydatabase.h"

#include "core/constants.h"
#include "query.h"

#include <utils/utils.h>

namespace Fy::Core::DB {
QMap<QString, QVariant> getTrackBindings(const Track& track)
{
    return QMap<QString, QVariant>{
        {QStringLiteral("FilePath"), Utils::File::cleanPath(track.filepath())},
        {QStringLiteral("Title"), track.title()},
        {QStringLiteral("TrackNumber"), track.trackNumber()},
        {QStringLiteral("TrackTotal"), track.trackTotal()},
        {QStringLiteral("Artists"), track.artists().join(Constants::Separator)},
        {QStringLiteral("AlbumArtist"), track.albumArtist()},
        {QStringLiteral("Album"), track.album()},
        {QStringLiteral("CoverPath"), track.coverPath()},
        {QStringLiteral("DiscNumber"), track.discNumber()},
        {QStringLiteral("DiscTotal"), track.discTotal()},
        {QStringLiteral("Date"), track.date()},
        {QStringLiteral("Year"), track.year()},
        {QStringLiteral("Composer"), track.composer()},
        {QStringLiteral("Performer"), track.performer()},
        {QStringLiteral("Genres"), track.genres().join(Constants::Separator)},
        {QStringLiteral("Lyrics"), track.lyrics()},
        {QStringLiteral("Comment"), track.comment()},
        {QStringLiteral("Duration"), QVariant::fromValue(track.duration())},
        {QStringLiteral("FileSize"), QVariant::fromValue(track.fileSize())},
        {QStringLiteral("BitRate"), track.bitrate()},
        {QStringLiteral("SampleRate"), track.sampleRate()},
        {QStringLiteral("ExtraTags"), track.serialiseExtrasTags()},
        {QStringLiteral("AddedDate"), QVariant::fromValue(track.addedTime())},
        {QStringLiteral("ModifiedDate"), QVariant::fromValue(track.modifiedTime())},
        {QStringLiteral("LibraryID"), track.libraryId()},
    };
}

LibraryDatabase::LibraryDatabase(const QString& connectionName)
    : DB::Module(connectionName)
    , m_connectionName(connectionName)
{ }

bool LibraryDatabase::storeTracks(TrackList& tracks)
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
            const int id = insertTrack(track);
            track.setId(id);
        }
    }

    if(!db().commit()) {
        qDebug() << "Transaction could not be commited";
        return false;
    }

    return true;
}

bool LibraryDatabase::getAllTracks(TrackList& result)
{
    auto q           = Query(module());
    const auto query = fetchQueryTracks({}, {});
    q.prepareQuery(query);

    return dbFetchTracks(q, result);
}

bool LibraryDatabase::getAllTracks(TrackList& result, int offset, int limit)
{
    auto q = Query(module());

    QString offsetLimit;
    if(limit > 0) {
        offsetLimit.append(QString("LIMIT %1").arg(limit));
        if(offset > 0) {
            offsetLimit.append(QString(" OFFSET %1").arg(offset));
        }
    }

    const auto query = fetchQueryTracks({}, offsetLimit);
    q.prepareQuery(query);

    return dbFetchTracks(q, result);
}

QString LibraryDatabase::fetchQueryTracks(const QString& join, const QString& offsetLimit)
{
    static const auto fields = QStringList{
        QStringLiteral("TrackID"),      // 0
        QStringLiteral("FilePath"),     // 1
        QStringLiteral("Title"),        // 2
        QStringLiteral("TrackNumber"),  // 3
        QStringLiteral("TrackTotal"),   // 4
        QStringLiteral("Artists"),      // 5
        QStringLiteral("AlbumArtist"),  // 6
        QStringLiteral("Album"),        // 7
        QStringLiteral("CoverPath"),    // 8
        QStringLiteral("DiscNumber"),   // 9
        QStringLiteral("DiscTotal"),    // 10
        QStringLiteral("Date"),         // 11
        QStringLiteral("Year"),         // 12
        QStringLiteral("Composer"),     // 13
        QStringLiteral("Performer"),    // 14
        QStringLiteral("Genres"),       // 15
        QStringLiteral("Lyrics"),       // 16
        QStringLiteral("Comment"),      // 17
        QStringLiteral("Duration"),     // 18
        QStringLiteral("PlayCount"),    // 19
        QStringLiteral("Rating"),       // 20
        QStringLiteral("FileSize"),     // 21
        QStringLiteral("BitRate"),      // 22
        QStringLiteral("SampleRate"),   // 23
        QStringLiteral("ExtraTags"),    // 24
        QStringLiteral("AddedDate"),    // 25
        QStringLiteral("ModifiedDate"), // 26
        QStringLiteral("LibraryID"),    // 27
    };

    const auto joinedFields = fields.join(", ");

    return QString("SELECT %1 FROM Tracks %2 %5;").arg(joinedFields, join.isEmpty() ? "" : join, offsetLimit);
}

bool LibraryDatabase::dbFetchTracks(Query& q, TrackList& result)
{
    result.clear();

    if(!q.execQuery()) {
        q.error("Cannot fetch tracks from database");
        return false;
    }

    while(q.next()) {
        Track track{q.value(1).toString()};

        track.setId(q.value(0).toInt());
        track.setTitle(q.value(2).toString());
        track.setTrackNumber(q.value(3).toInt());
        track.setTrackTotal(q.value(4).toInt());
        track.setArtists(q.value(5).toString().split(Constants::Separator, Qt::SkipEmptyParts));
        track.setAlbumArtist(q.value(6).toString());
        track.setAlbum(q.value(7).toString());
        track.setCoverPath(q.value(8).toString());
        track.setDiscNumber(q.value(9).toInt());
        track.setDiscTotal(q.value(10).toInt());
        track.setDate(q.value(11).toString());
        track.setYear(q.value(12).toInt());
        track.setComposer(q.value(13).toString());
        track.setPerformer(q.value(14).toString());
        track.setGenres(q.value(15).toString().split(Constants::Separator, Qt::SkipEmptyParts));
        track.setLyrics(q.value(16).toString());
        track.setComment(q.value(17).toString());
        track.setDuration(q.value(18).value<uint64_t>());
        track.setPlayCount(q.value(19).toInt());
        // 20 - Rating
        track.setFileSize(q.value(21).toInt());
        track.setBitrate(q.value(22).toInt());
        track.setSampleRate(q.value(23).toInt());
        track.storeExtraTags(q.value(24).toByteArray());
        track.setAddedTime(q.value(25).value<uint64_t>());
        track.setModifiedTime(q.value(26).value<uint64_t>());
        track.setLibraryId(q.value(27).toInt());

        track.generateHash();

        result.emplace_back(track);
    }
    return !result.empty();
}

bool LibraryDatabase::updateTrack(const Track& track)
{
    if(track.id() < 0 || track.libraryId() < 0) {
        qDebug() << "Cannot update track (value negative): "
                 << " TrackID: " << track.id() << " LibraryID: " << track.libraryId();
        return false;
    }

    auto bindings = getTrackBindings(track);

    const auto q = module()->update("Tracks", bindings, {"TrackID", track.id()},
                                    QString("Cannot update track %1").arg(track.filepath()));

    return !q.hasError();
}

bool LibraryDatabase::deleteTrack(int id)
{
    const auto queryText = QStringLiteral("DELETE FROM Tracks WHERE TrackID = :TrackID;");
    const auto q         = module()->runQuery(queryText, {":TrackID", id}, QString("Cannot delete track %1").arg(id));

    return (!q.hasError());
}

bool LibraryDatabase::deleteTracks(const TrackList& tracks)
{
    if(tracks.empty()) {
        return true;
    }

    if(!module()->db().transaction()) {
        qDebug() << "Transaction could not be started";
        return false;
    }

    const int fileCount = static_cast<int>(std::count_if(tracks.cbegin(), tracks.cend(), [&](const Track& track) {
        return deleteTrack(track.id());
    }));

    const auto success = module()->db().commit();

    return (success && (fileCount == static_cast<int>(tracks.size())));
}

Module* LibraryDatabase::module()
{
    return this;
}

const Module* LibraryDatabase::module() const
{
    return this;
}

int LibraryDatabase::insertTrack(const Track& track)
{
    auto bindings = getTrackBindings(track);

    const auto query = module()->insert("Tracks", bindings, QString("Cannot insert track %1").arg(track.filepath()));

    return (query.hasError()) ? -1 : query.lastInsertId().toInt();
}
} // namespace Fy::Core::DB
