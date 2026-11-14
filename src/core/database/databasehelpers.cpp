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

#include "databasehelpers.h"

#include <core/constants.h>
#include <core/track.h>
#include <utils/database/dbquery.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Database {
const QString& trackInsertStatement(bool ignoreDuplicates)
{
    static const QString body = u"INTO Tracks ("
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
                                "SampleFrames,"
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
                                "StatisticsKey,"
                                "LibraryID,"
                                "RGTrackGain,"
                                "RGAlbumGain,"
                                "RGTrackPeak,"
                                "RGAlbumPeak,"
                                "CreatedDate"
                                ") "
                                "VALUES ("
                                ":filePath,"
                                ":subsong,"
                                ":title,"
                                ":trackNumber,"
                                ":trackTotal,"
                                ":artists,"
                                ":albumArtist,"
                                ":album,"
                                ":discNumber,"
                                ":discTotal,"
                                ":date,"
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
                                ":sampleFrames,"
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
                                ":statisticsKey,"
                                ":libraryID,"
                                ":rgTrackGain,"
                                ":rgAlbumGain,"
                                ":rgTrackPeak,"
                                ":rgAlbumPeak,"
                                ":createdDate"
                                ");"_s;

    static const QString insertStatement = "INSERT "_L1 + body;
    static const QString ignoreStatement = "INSERT OR IGNORE "_L1 + body;

    return ignoreDuplicates ? ignoreStatement : insertStatement;
}

BindingsMap trackBindings(const Track& track)
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
            {u":sampleFrames"_s, static_cast<quint64>(track.sampleFrames())},
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
            {u":statisticsKey"_s, track.statisticsKey()},
            {u":libraryID"_s, track.libraryId()},
            {u":rgTrackGain"_s, track.rgTrackGain()},
            {u":rgAlbumGain"_s, track.rgAlbumGain()},
            {u":rgTrackPeak"_s, track.rgTrackPeak()},
            {u":rgAlbumPeak"_s, track.rgAlbumPeak()},
            {u":createdDate"_s, static_cast<quint64>(track.createdTime())}};
}
QString trackColumns()
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
                                   "SampleFrames,"
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
                                   "StatisticsKey,"
                                   "StatisticsRevision,"
                                   "RGTrackGain,"
                                   "RGAlbumGain,"
                                   "RGTrackPeak,"
                                   "RGAlbumPeak,"
                                   "CreatedDate,"
                                   "AddedDate,"
                                   "FirstPlayed,"
                                   "LastPlayed,"
                                   "PlayCount,"
                                   "Rating,"
                                   "Loved"_s;

    return columns;
}

Track readTrack(const DbQuery& q, const std::shared_ptr<TrackMetadataStore>& store)
{
    Track track{store};

    track.setId(q.value(0).toInt());
    track.setFilePath(q.value(1).toString());
    track.setSubsong(q.value(2).toInt());
    track.setTitle(q.value(3).toString());
    track.setTrackNumber(q.value(4).toString());
    track.setTrackTotal(q.value(5).toString());
    track.setArtists(q.value(6).toString().split(QLatin1String{Constants::UnitSeparator}));
    track.setAlbumArtists(q.value(7).toString().split(QLatin1String{Constants::UnitSeparator}));
    track.setAlbum(q.value(8).toString());
    track.setDiscNumber(q.value(9).toString());
    track.setDiscTotal(q.value(10).toString());
    track.setDate(q.value(11).toString());
    track.setComposers(q.value(12).toString().split(QLatin1String{Constants::UnitSeparator}));
    track.setPerformers(q.value(13).toString().split(QLatin1String{Constants::UnitSeparator}));
    track.setGenres(q.value(14).toString().split(QLatin1String{Constants::UnitSeparator}));
    track.setComment(q.value(15).toString());
    track.setCuePath(q.value(16).toString());
    track.setOffset(q.value(17).toULongLong());
    track.setDuration(q.value(18).toULongLong());
    track.setFileSize(q.value(19).toULongLong());
    track.setBitrate(q.value(20).toInt());
    track.setSampleRate(q.value(21).toInt());
    track.setSampleFrames(q.value(22).toULongLong());
    track.setChannels(q.value(23).toInt());
    track.setBitDepth(q.value(24).toInt());
    track.setCodec(q.value(25).toString());
    track.setCodecProfile(q.value(26).toString());
    track.setTool(q.value(27).toString());
    track.setTagTypes(q.value(28).toString().split(QLatin1String{Constants::UnitSeparator}));
    track.setEncoding(q.value(29).toString());
    track.storeExtraTags(q.value(30).toByteArray());
    track.storeExtraProperties(q.value(31).toByteArray());
    track.setModifiedTime(q.value(32).toULongLong());
    track.setLibraryId(q.value(33).toInt());
    track.setHash(q.value(34).toString());
    track.setStatisticsKey(q.value(35).toString());
    track.setStatisticsRevision(q.value(36).toULongLong());

    bool isValid{false};
    if(const auto rgTrackGain = q.value(37).toFloat(&isValid); isValid) {
        track.setRGTrackGain(rgTrackGain);
    }
    if(const auto rgAlbumGain = q.value(38).toFloat(&isValid); isValid) {
        track.setRGAlbumGain(rgAlbumGain);
    }
    if(const auto rgTrackPeak = q.value(39).toFloat(&isValid); isValid) {
        track.setRGTrackPeak(rgTrackPeak);
    }
    if(const auto rgAlbumPeak = q.value(40).toFloat(&isValid); isValid) {
        track.setRGAlbumPeak(rgAlbumPeak);
    }
    track.setCreatedTime(q.value(41).toULongLong());

    track.setAddedTime(q.value(42).toULongLong());
    track.setFirstPlayed(q.value(43).toULongLong());
    track.setLastPlayed(q.value(44).toULongLong());
    track.setPlayCount(q.value(45).toInt());
    track.setRating(q.value(46).toFloat());
    track.setLoved(q.value(47).toBool());

    track.setMetadataWasRead(true);
    track.generateHash();

    return track;
}
} // namespace Fooyin::Database
