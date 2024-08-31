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

namespace Fooyin {
namespace Tag {
constexpr auto Title         = "TITLE";
constexpr auto Artist        = "ARTIST";
constexpr auto ArtistAlt     = "ARTISTS";
constexpr auto Album         = "ALBUM";
constexpr auto AlbumArtist   = "ALBUMARTIST";
constexpr auto Genre         = "GENRE";
constexpr auto Composer      = "COMPOSER";
constexpr auto Performer     = "PERFORMER";
constexpr auto Comment       = "COMMENT";
constexpr auto Date          = "DATE";
constexpr auto Year          = "YEAR";
constexpr auto Rating        = "RATING";
constexpr auto RatingAlt     = "FMPS_RATING";
constexpr auto PlayCount     = "FMPS_PLAYCOUNT";
constexpr auto Track         = "TRACKNUMBER";
constexpr auto TrackAlt      = "TRACK";
constexpr auto TrackTotal    = "TRACKTOTAL";
constexpr auto TrackTotalAlt = "TOTALTRACKS";
constexpr auto Disc          = "DISCNUMBER";
constexpr auto DiscAlt       = "DISC";
constexpr auto DiscTotal     = "DISCTOTAL";
constexpr auto DiscTotalAlt  = "TOTALDISCS";

namespace ReplayGain {
constexpr auto ReplayGainStart = "REPLAYGAIN_";
constexpr auto AlbumPeak       = "REPLAYGAIN_ALBUM_PEAK";
constexpr auto AlbumPeakAlt    = "REPLAYGAIN_ALBUMPEAK";
constexpr auto AlbumGain       = "REPLAYGAIN_ALBUM_GAIN";
constexpr auto AlbumGainAlt    = "REPLAYGAIN_ALBUMGAIN";
constexpr auto TrackPeak       = "REPLAYGAIN_TRACK_PEAK";
constexpr auto TrackPeakAlt    = "REPLAYGAIN_TRACKPEAK";
constexpr auto TrackGain       = "REPLAYGAIN_TRACK_GAIN";
constexpr auto TrackGainAlt    = "REPLAYGAIN_TRACKGAIN";
} // namespace ReplayGain
} // namespace Tag

namespace Mp4 {
constexpr auto Title         = "\251nam";
constexpr auto Artist        = "\251ART";
constexpr auto Album         = "\251alb";
constexpr auto AlbumArtist   = "aART";
constexpr auto Genre         = "\251gen";
constexpr auto Composer      = "\251wrt";
constexpr auto Performer     = "perf";
constexpr auto PerformerAlt  = "----:com.apple.iTunes:PERFORMER";
constexpr auto Comment       = "\251cmt";
constexpr auto Date          = "\251day";
constexpr auto Rating        = "rate";
constexpr auto RatingAlt     = "----:com.apple.iTunes:FMPS_Rating";
constexpr auto RatingAlt2    = "----:com.apple.iTunes:RATING";
constexpr auto PlayCount     = "----:com.apple.iTunes:FMPS_Playcount";
constexpr auto Track         = "trkn";
constexpr auto TrackAlt      = "----:com.apple.iTunes:track";
constexpr auto TrackTotal    = "----:com.apple.iTunes:TRACKTOTAL";
constexpr auto TrackTotalAlt = "----:com.apple.iTunes:TOTALTRACKS";
constexpr auto Disc          = "disk";
constexpr auto DiscAlt       = "----:com.apple.iTunes:disc";
constexpr auto DiscTotal     = "----:com.apple.iTunes:DISCTOTAL";
constexpr auto DiscTotalAlt  = "----:com.apple.iTunes:TOTALDISCS";
constexpr auto Cover         = "covr";
} // namespace Mp4
} // namespace Fooyin
