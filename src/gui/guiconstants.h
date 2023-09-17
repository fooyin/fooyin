/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

namespace Fy::Gui::Constants {
namespace Icons {
constexpr auto Fooyin     = "sc-fooyin";
constexpr auto Play       = "media-playback-start";
constexpr auto Pause      = "media-playback-pause";
constexpr auto Stop       = "media-playback-stop";
constexpr auto Prev       = "media-skip-backward";
constexpr auto Next       = "media-skip-forward";
constexpr auto Repeat     = "media-playlist-repeat-one";
constexpr auto RepeatAll  = "media-playlist-repeat";
constexpr auto Shuffle    = "media-playlist-shuffle";
constexpr auto VolumeHigh = "audio-volume-high";
constexpr auto VolumeMed  = "audio-volume-medium";
constexpr auto VolumeLow  = "audio-volume-low";
constexpr auto VolumeMute = "audio-volume-muted";
constexpr auto Font       = "preferences-desktop-font";
constexpr auto Add        = "list-add";
constexpr auto Remove     = "list-remove";
constexpr auto TextColour = "format-text-bold";

constexpr auto Quit          = "application-exit";
constexpr auto Settings      = "preferences-system";
constexpr auto RescanLibrary = "view-refresh";
constexpr auto LayoutEditing = "applications-graphics";
constexpr auto QuickSetup    = "preferences-desktop";
} // namespace Icons

constexpr auto MenuBar = "Fooyin.MenuBar";

namespace Menus {
constexpr auto File          = "Fooyin.Menu.File";
constexpr auto Edit          = "Fooyin.Menu.Edit";
constexpr auto View          = "Fooyin.Menu.View";
constexpr auto Playback      = "Fooyin.Menu.Playback";
constexpr auto PlaybackOrder = "Fooyin.Menu.Playback.Order";
constexpr auto Library       = "Fooyin.Menu.Library";
constexpr auto SwitchLibrary = "Fooyin.Menu.Library.Switch";
constexpr auto Help          = "Fooyin.Menu.Help";

namespace Context {
constexpr auto Layout         = "Fooyin.Menu.Layout";
constexpr auto AddWidget      = "Fooyin.Menu.Widget.Add";
constexpr auto TrackSelection = "Fooyin.Menu.Tracks";
constexpr auto TracksPlaylist = "Fooyin.Menu.Tracks.Playlist";
constexpr auto Playlist       = "Fooyin.Menu.Playlist";
constexpr auto PlaylistAddTo  = "Fooyin.Menu.Playlist.AddTo";
} // namespace Context
} // namespace Menus

namespace Groups {
constexpr auto One   = "Fooyin.Group.One";
constexpr auto Two   = "Fooyin.Group.Two";
constexpr auto Three = "Fooyin.Group.Three";

constexpr auto File     = "Fooyin.Group.File";
constexpr auto Edit     = "Fooyin.Group.Edit";
constexpr auto View     = "Fooyin.Group.View";
constexpr auto Playback = "Fooyin.Group.Playback";
constexpr auto Library  = "Fooyin.Group.Library";
constexpr auto Help     = "Fooyin.Group.Help";

namespace Context {
constexpr auto Layout        = "Fooyin.Group.Layout";
constexpr auto AddWidget     = "Fooyin.Group.Widget.Add";
constexpr auto Playlist      = "Fooyin.Group.Playlist";
constexpr auto PlaylistAddTo = "Fooyin.Group.Playlist.AddTo";
} // namespace Context
} // namespace Groups

namespace Actions {
constexpr auto Exit            = "Fooyin.Action.Exit";
constexpr auto Settings        = "Fooyin.Action.Settings";
constexpr auto LayoutEditing   = "Fooyin.Action.LayoutEditing";
constexpr auto QuickSetup      = "Fooyin.Action.QuickSetup";
constexpr auto About           = "Fooyin.Action.About";
constexpr auto Rescan          = "Fooyin.Action.Rescan";
constexpr auto LibrarySwitch   = "Fooyin.Action.LibrarySwitch";
constexpr auto Stop            = "Fooyin.Action.Stop";
constexpr auto PlayPause       = "Fooyin.Action.PlayPause";
constexpr auto Next            = "Fooyin.Action.Next";
constexpr auto Previous        = "Fooyin.Action.Previous";
constexpr auto PlaybackDefault = "Fooyin.Action.Playback.Default";
constexpr auto Repeat          = "Fooyin.Action.Playback.Repeat";
constexpr auto RepeatAll       = "Fooyin.Action.Playback.RepeatAll";
constexpr auto Shuffle         = "Fooyin.Action.Playback.Shuffle";
} // namespace Actions

namespace Page {
constexpr auto GeneralCore        = "Fooyin.Page.General.Core";
constexpr auto InterfaceGeneral   = "Fooyin.Page.Interface.General";
constexpr auto LibraryGeneral     = "Fooyin.Page.Library.General";
constexpr auto LibrarySorting     = "Fooyin.Page.Library.Sorting";
constexpr auto PlaylistInterface  = "Fooyin.Page.Playlist.Interface";
constexpr auto PlaylistPresets    = "Fooyin.Page.Playlist.Presets";
constexpr auto WidgetsLibraryTree = "Fooyin.Page.Widgets.LibraryTree";
constexpr auto Plugins            = "Fooyin.Page.Plugins";
} // namespace Page
} // namespace Fy::Gui::Constants
