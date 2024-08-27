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

#pragma once

namespace Fooyin::Constants {
constexpr auto RecordSeparator = "\036";
constexpr auto UnitSeparator   = "\037";

namespace MetaData {
constexpr auto Title           = "title";
constexpr auto Artist          = "artist";
constexpr auto UniqueArtist    = "uniqueartist";
constexpr auto Album           = "album";
constexpr auto AlbumArtist     = "albumartist";
constexpr auto Track           = "track";
constexpr auto TrackTotal      = "tracktotal";
constexpr auto Disc            = "disc";
constexpr auto DiscTotal       = "disctotal";
constexpr auto Genre           = "genre";
constexpr auto Composer        = "composer";
constexpr auto Performer       = "performer";
constexpr auto Duration        = "duration";
constexpr auto DurationSecs    = "duration_s";
constexpr auto Lyrics          = "lyrics";
constexpr auto Comment         = "comment";
constexpr auto Date            = "date";
constexpr auto Year            = "year";
constexpr auto FileSize        = "filesize";
constexpr auto Bitrate         = "bitrate";
constexpr auto SampleRate      = "samplerate";
constexpr auto FirstPlayed     = "firstplayed";
constexpr auto LastPlayed      = "lastplayed";
constexpr auto PlayCount       = "playcount";
constexpr auto Rating          = "rating";
constexpr auto RatingStars     = "rating_stars";
constexpr auto RatingEditor    = "rating_editor";
constexpr auto Codec           = "codec";
constexpr auto Channels        = "channels";
constexpr auto BitDepth        = "bitdepth";
constexpr auto AddedTime       = "addedtime";
constexpr auto LastModified    = "lastmodified";
constexpr auto FilePath        = "filepath";
constexpr auto RelativePath    = "relativepath";
constexpr auto FileName        = "filename";
constexpr auto Extension       = "extension";
constexpr auto FileNameWithExt = "filename_ext";
constexpr auto Directory       = "directory";
constexpr auto Path            = "path";
constexpr auto Subsong         = "subsong";
} // namespace MetaData
} // namespace Fooyin::Constants
