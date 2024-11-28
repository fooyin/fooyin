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

constexpr auto InvalidGain = -1000;
constexpr auto InvalidPeak = -1;

namespace MetaData {
constexpr auto Title           = "TITLE";
constexpr auto Artist          = "ARTIST";
constexpr auto UniqueArtist    = "UNIQUEARTIST";
constexpr auto Album           = "ALBUM";
constexpr auto AlbumArtist     = "ALBUMARTIST";
constexpr auto Track           = "TRACK";
constexpr auto TrackTotal      = "TRACKTOTAL";
constexpr auto Disc            = "DISC";
constexpr auto DiscTotal       = "DISCTOTAL";
constexpr auto Genre           = "GENRE";
constexpr auto Composer        = "COMPOSER";
constexpr auto Performer       = "PERFORMER";
constexpr auto Duration        = "DURATION";
constexpr auto DurationSecs    = "DURATION_S";
constexpr auto DurationMSecs   = "DURATION_MS";
constexpr auto Lyrics          = "LYRICS";
constexpr auto Comment         = "COMMENT";
constexpr auto Date            = "DATE";
constexpr auto Year            = "YEAR";
constexpr auto FileSize        = "FILESIZE";
constexpr auto FileSizeNatural = "FILESIZE_NATURAL";
constexpr auto Bitrate         = "BITRATE";
constexpr auto SampleRate      = "SAMPLERATE";
constexpr auto FirstPlayed     = "FIRSTPLAYED";
constexpr auto LastPlayed      = "LASTPLAYED";
constexpr auto PlayCount       = "PLAYCOUNT";
constexpr auto Rating          = "RATING";
constexpr auto RatingStars     = "RATING_STARS";
constexpr auto RatingEditor    = "RATING_EDITOR";
constexpr auto Codec           = "CODEC";
constexpr auto CodecProfile    = "CODEC_PROFILE";
constexpr auto Tool            = "TOOL";
constexpr auto TagType         = "TAGTYPE";
constexpr auto Encoding        = "ENCODING";
constexpr auto Channels        = "CHANNELS";
constexpr auto BitDepth        = "BITDEPTH";
constexpr auto AddedTime       = "ADDEDTIME";
constexpr auto LastModified    = "LASTMODIFIED";
constexpr auto FilePath        = "FILEPATH";
constexpr auto RelativePath    = "RELATIVEPATH";
constexpr auto FileName        = "FILENAME";
constexpr auto Extension       = "EXTENSION";
constexpr auto FileNameWithExt = "FILENAME_EXT";
constexpr auto Directory       = "DIRECTORY";
constexpr auto Path            = "PATH";
constexpr auto Subsong         = "SUBSONG";
constexpr auto RGTrackGain     = "REPLAYGAIN_TRACK_GAIN";
constexpr auto RGTrackPeak     = "REPLAYGAIN_TRACK_PEAK";
constexpr auto RGTrackPeakDB   = "REPLAYGAIN_TRACK_PEAK_DB";
constexpr auto RGAlbumGain     = "REPLAYGAIN_ALBUM_GAIN";
constexpr auto RGAlbumPeak     = "REPLAYGAIN_ALBUM_PEAK";
constexpr auto RGAlbumPeakDB   = "REPLAYGAIN_ALBUM_PEAK_DB";
} // namespace MetaData

constexpr auto FrontCover    = "%frontcover%";
constexpr auto BackCover     = "%backcover%";
constexpr auto ArtistPicture = "%artistpicture%";
constexpr auto RatingEditor  = "%rating_editor%";
} // namespace Fooyin::Constants
