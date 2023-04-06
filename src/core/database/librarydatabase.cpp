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
#include "core/library/libraryutils.h"
#include "query.h"

#include <utils/helpers.h>
#include <utils/utils.h>

#include <QBuffer>

namespace Fy::Core::DB {
QMap<QString, QVariant> getTrackBindings(const Track& track)
{
    return QMap<QString, QVariant>{
        {QStringLiteral("FilePath"),     Utils::File::cleanPath(track.filepath())  },
        {QStringLiteral("Title"),        track.title()                             },
        {QStringLiteral("TrackNumber"),  track.trackNumber()                       },
        {QStringLiteral("TrackTotal"),   track.trackTotal()                        },
        {QStringLiteral("Artists"),      track.artists().join(Constants::Separator)},
        {QStringLiteral("AlbumArtist"),  track.albumArtist()                       },
        {QStringLiteral("Album"),        track.album()                             },
        {QStringLiteral("CoverPath"),    track.coverPath()                         },
        {QStringLiteral("DiscNumber"),   track.discNumber()                        },
        {QStringLiteral("DiscTotal"),    track.discTotal()                         },
        {QStringLiteral("Date"),         track.date()                              },
        {QStringLiteral("Composer"),     track.composer()                          },
        {QStringLiteral("Performer"),    track.performer()                         },
        {QStringLiteral("Genres"),       track.genres().join(Constants::Separator) },
        {QStringLiteral("Lyrics"),       track.lyrics()                            },
        {QStringLiteral("Comment"),      track.comment()                           },
        {QStringLiteral("Duration"),     QVariant::fromValue(track.duration())     },
        {QStringLiteral("FileSize"),     QVariant::fromValue(track.fileSize())     },
        {QStringLiteral("BitRate"),      track.bitrate()                           },
        {QStringLiteral("SampleRate"),   track.sampleRate()                        },
        {QStringLiteral("ExtraTags"),    track.extraTagsToJson()                   },
        {QStringLiteral("AddedDate"),    QVariant::fromValue(track.addedTime())    },
        {QStringLiteral("ModifiedDate"), QVariant::fromValue(track.modifiedTime()) },
        {QStringLiteral("LibraryID"),    track.libraryId()                         },
    };
}

QString getOrderString(Core::Library::SortOrder order)
{
    QString orderString{"AlbumArtist ASC"};

    switch(order) {
        case(Core::Library::SortOrder::NoSorting):
            orderString += ", Date DESC";
    }
    orderString += ", Album ASC, DiscNumber ASC, TrackNumber ASC";
    return orderString;
}

LibraryDatabase::LibraryDatabase(const QString& connectionName, int libraryId)
    : DB::Module(connectionName)
    , m_libraryId(libraryId)
    , m_connectionName(connectionName)
{ }

bool LibraryDatabase::storeCovers(TrackList& tracks)
{
    if(tracks.empty()) {
        return false;
    }

    for(auto& track : tracks) {
        if(track.libraryId() < 0) {
            track.setLibraryId(m_libraryId);
        }

        if(!track.hasCover()) {
            const QString coverPath = Library::Utils::storeCover(track);
            track.setCoverPath(coverPath);
        }
    }
    return true;
}

bool LibraryDatabase::storeTracks(TrackList& tracks)
{
    if(tracks.empty()) {
        return true;
    }

    storeCovers(tracks);

    db().transaction();

    for(auto& track : tracks) {
        if(track.id() >= 0) {
            updateTrack(track);
        }
        else {
            const int id = insertTrack(track);
            track.setId(id);
        }
    }

    return db().commit();
}

bool LibraryDatabase::getAllTracks(TrackList& result, Core::Library::SortOrder order) const
{
    auto q               = Query(module());
    const QString& where = m_libraryId >= 0 ? QString{"LibraryID = %1"}.arg(m_libraryId) : "";
    const auto query     = fetchQueryTracks(where, {}, getOrderString(order), {});
    q.prepareQuery(query);

    return dbFetchTracks(q, result);
}

bool LibraryDatabase::getAllTracks(TrackList& result, Core::Library::SortOrder order, int offset, int limit) const
{
    auto q = Query(module());

    const QString& where = m_libraryId >= 0 ? QString{"LibraryID = %1"}.arg(m_libraryId) : "";

    QString offsetLimit;
    if(limit > 0) {
        offsetLimit.append(QString("LIMIT %1").arg(limit));
        if(offset > 0) {
            offsetLimit.append(QString(" OFFSET %1").arg(offset));
        }
    }

    const auto query = fetchQueryTracks(where, {}, getOrderString(order), offsetLimit);
    q.prepareQuery(query);

    return dbFetchTracks(q, result);
}

QString LibraryDatabase::fetchQueryTracks(const QString& where, const QString& join, const QString& order,
                                          const QString& offsetLimit)
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
        QStringLiteral("Composer"),     // 12
        QStringLiteral("Performer"),    // 13
        QStringLiteral("Genres"),       // 14
        QStringLiteral("Lyrics"),       // 15
        QStringLiteral("Comment"),      // 16
        QStringLiteral("Duration"),     // 17
        QStringLiteral("PlayCount"),    // 18
        QStringLiteral("Rating"),       // 19
        QStringLiteral("FileSize"),     // 20
        QStringLiteral("BitRate"),      // 21
        QStringLiteral("SampleRate"),   // 22
        QStringLiteral("ExtraTags"),    // 23
        QStringLiteral("AddedDate"),    // 24
        QStringLiteral("ModifiedDate"), // 25
        QStringLiteral("LibraryID"),    // 26
    };

    const auto joinedFields = fields.join(", ");

    return QString("SELECT %1 FROM Tracks %2 WHERE %3 ORDER BY %4 %5;")
        .arg(joinedFields,
             join.isEmpty() ? "" : join,
             where.isEmpty() ? "1" : where,
             order.isEmpty() ? "1" : order,
             offsetLimit);
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
        track.setComposer(q.value(12).toString());
        track.setPerformer(q.value(13).toString());
        track.setGenres(q.value(14).toString().split(Constants::Separator, Qt::SkipEmptyParts));
        track.setLyrics(q.value(15).toString());
        track.setComment(q.value(16).toString());
        track.setDuration(q.value(17).value<uint64_t>());
        track.setPlayCount(q.value(18).toInt());
        track.setFileSize(q.value(20).toInt());
        track.setBitrate(q.value(21).toInt());
        track.setSampleRate(q.value(22).toInt());
        track.jsonToExtraTags(q.value(23).toByteArray());
        track.setAddedTime(q.value(24).value<uint64_t>());
        track.setModifiedTime(q.value(25).value<uint64_t>());
        track.setLibraryId(q.value(26).toInt());

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

    const auto q = module()->update(
        "Tracks", bindings, {"TrackID", track.id()}, QString("Cannot update track %1").arg(track.filepath()));

    return !q.hasError();
}

bool LibraryDatabase::deleteTrack(int id)
{
    const auto queryText = QStringLiteral("DELETE FROM Tracks WHERE TrackID = :TrackID;");
    const auto q         = module()->runQuery(queryText, {":TrackID", id}, QString("Cannot delete track %1").arg(id));

    return (!q.hasError());
}

bool LibraryDatabase::deleteTracks(const TrackPtrList& tracks)
{
    if(tracks.empty()) {
        return true;
    }

    module()->db().transaction();

    const int fileCount = static_cast<int>(std::count_if(tracks.cbegin(), tracks.cend(), [&](Track* track) {
        return deleteTrack(track->id());
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
