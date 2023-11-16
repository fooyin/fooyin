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

#pragma once

namespace Fooyin {
namespace Tag {
constexpr auto Title       = "TITLE";
constexpr auto Artist      = "ARTIST";
constexpr auto Album       = "ALBUM";
constexpr auto AlbumArtist = "ALBUMARTIST";
constexpr auto Genre       = "GENRE";
constexpr auto Composer    = "COMPOSER";
constexpr auto Performer   = "PERFORMER";
constexpr auto Comment     = "COMMENT";
constexpr auto Lyrics      = "LYRICS";
constexpr auto Date        = "DATE";
constexpr auto Rating      = "RATING";
constexpr auto TrackNumber = "TRACKNUMBER";
constexpr auto TrackTotal  = "TRACKTOTAL";
constexpr auto DiscNumber  = "DISCNUMBER";
constexpr auto DiscTotal   = "DISCTOTAL";
} // namespace Tag

namespace Mp4 {
constexpr auto Title        = "\251nam";
constexpr auto Artist       = "\251ART";
constexpr auto Album        = "\251alb";
constexpr auto AlbumArtist  = "aART";
constexpr auto Genre        = "\251gen";
constexpr auto Composer     = "\251wrt";
constexpr auto Performer    = "perf";
constexpr auto PerformerAlt = "----:com.apple.iTunes:PERFORMER";
constexpr auto Comment      = "\251cmt";
constexpr auto Lyrics       = "\251lyr";
constexpr auto Date         = "\251day";
constexpr auto Rating       = "rate";
constexpr auto TrackNumber  = "trkn";
constexpr auto DiscNumber   = "disk";
constexpr auto Cover        = "covr";
} // namespace Mp4
} // namespace Fooyin
