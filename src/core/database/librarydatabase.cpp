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
using namespace Qt::Literals::StringLiterals;

BindingsMap getTrackBindings(const Track& track)
{
    return {{u"FilePath"_s, Utils::File::cleanPath(track.filepath())},
            {u"Title"_s, track.title()},
            {u"TrackNumber"_s, QString::number(track.trackNumber())},
            {u"TrackTotal"_s, QString::number(track.trackTotal())},
            {u"Artists"_s, track.artists().join(Constants::Separator)},
            {u"AlbumArtist"_s, track.albumArtist()},
            {u"Album"_s, track.album()},
            {u"CoverPath"_s, track.coverPath()},
            {u"DiscNumber"_s, QString::number(track.discNumber())},
            {u"DiscTotal"_s, QString::number(track.discTotal())},
            {u"Date"_s, track.date()},
            {u"Year"_s, QString::number(track.year())},
            {u"Composer"_s, track.composer()},
            {u"Performer"_s, track.performer()},
            {u"Genres"_s, track.genres().join(Constants::Separator)},
            {u"Lyrics"_s, track.lyrics()},
            {u"Comment"_s, track.comment()},
            {u"Duration"_s, QString::number(track.duration())},
            {u"FileSize"_s, QString::number(track.fileSize())},
            {u"BitRate"_s, QString::number(track.bitrate())},
            {u"SampleRate"_s, QString::number(track.sampleRate())},
            {u"ExtraTags"_s, track.serialiseExtrasTags()},
            {u"AddedDate"_s, QString::number(track.addedTime())},
            {u"ModifiedDate"_s, QString::number(track.modifiedTime())},
            {u"LibraryID"_s, QString::number(track.libraryId())}};
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
    static const QStringList fields = {
        u"TrackID"_s,      // 0
        u"FilePath"_s,     // 1
        u"Title"_s,        // 2
        u"TrackNumber"_s,  // 3
        u"TrackTotal"_s,   // 4
        u"Artists"_s,      // 5
        u"AlbumArtist"_s,  // 6
        u"Album"_s,        // 7
        u"CoverPath"_s,    // 8
        u"DiscNumber"_s,   // 9
        u"DiscTotal"_s,    // 10
        u"Date"_s,         // 11
        u"Year"_s,         // 12
        u"Composer"_s,     // 13
        u"Performer"_s,    // 14
        u"Genres"_s,       // 15
        u"Lyrics"_s,       // 16
        u"Comment"_s,      // 17
        u"Duration"_s,     // 18
        u"PlayCount"_s,    // 19
        u"Rating"_s,       // 20
        u"FileSize"_s,     // 21
        u"BitRate"_s,      // 22
        u"SampleRate"_s,   // 23
        u"ExtraTags"_s,    // 24
        u"AddedDate"_s,    // 25
        u"ModifiedDate"_s, // 26
        u"LibraryID"_s     // 27
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
    return true;
}

bool LibraryDatabase::updateTrack(const Track& track)
{
    if(track.id() < 0 || track.libraryId() < 0) {
        qDebug() << "Cannot update track (value negative): "
                 << " TrackID: " << track.id() << " LibraryID: " << track.libraryId();
        return false;
    }

    auto bindings = getTrackBindings(track);

    const auto q = module()->update("Tracks", bindings, {"TrackID", QString::number(track.id())},
                                    QString("Cannot update track %1").arg(track.filepath()));

    return !q.hasError();
}

bool LibraryDatabase::deleteTrack(int id)
{
    const auto queryText = QStringLiteral("DELETE FROM Tracks WHERE TrackID = :TrackID;");
    const auto q
        = module()->runQuery(queryText, {":TrackID", QString::number(id)}, QString{"Cannot delete track %1"}.arg(id));

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

    const int fileCount = static_cast<int>(std::ranges::count_if(std::as_const(tracks), [this](const Track& track) {
        return deleteTrack(track.id());
    }));

    const auto success = module()->db().commit();

    return (success && (fileCount == static_cast<int>(tracks.size())));
}

int LibraryDatabase::insertTrack(const Track& track)
{
    auto bindings = getTrackBindings(track);

    const auto query = module()->insert("Tracks", bindings, QString("Cannot insert track %1").arg(track.filepath()));

    return (query.hasError()) ? -1 : query.lastInsertId().toInt();
}
} // namespace Fy::Core::DB
