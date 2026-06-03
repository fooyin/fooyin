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

#include <core/track.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Database {
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
            {u":rgAlbumPeak"_s, track.rgAlbumPeak()},
            {u":createdDate"_s, static_cast<quint64>(track.createdTime())}};
}
} // namespace Fooyin::Database
