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

namespace Fooyin::Constants {
constexpr auto NoCover = "://images/nocover.png";

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

constexpr auto ScriptExpression = "script-expression";
constexpr auto ScriptVariable   = "script-variable";
constexpr auto ScriptFunction   = "script-function";
constexpr auto ScriptLiteral    = "script-literal";

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

namespace Context {
constexpr auto Playlist    = "Fooyin.Context.Playlist";
constexpr auto Search      = "Fooyin.Context.Search";
constexpr auto LibraryTree = "Fooyin.Context.LibraryTree";
} // namespace Context

namespace Actions {
constexpr auto Exit            = "File.Exit";
constexpr auto Settings        = "Edit.Settings";
constexpr auto LayoutEditing   = "View.LayoutEditing";
constexpr auto QuickSetup      = "View.QuickSetup";
constexpr auto About           = "Help.About";
constexpr auto Rescan          = "Library.Rescan";
constexpr auto Stop            = "Playback.Stop";
constexpr auto PlayPause       = "Playback.PlayPause";
constexpr auto Next            = "Playback.Next";
constexpr auto Previous        = "Playback.Previous";
constexpr auto PlaybackDefault = "Playback.Order.Default";
constexpr auto Repeat          = "Playback.Order.Repeat";
constexpr auto RepeatAll       = "Playback.Order.RepeatAll";
constexpr auto Shuffle         = "Playback.Order.Shuffle";
constexpr auto ScriptSandbox   = "View.ScriptSandbox";
constexpr auto SelectAll       = "Edit.SelectAll";
constexpr auto Clear           = "Edit.Clear";
constexpr auto Undo            = "Edit.Undo";
constexpr auto Redo            = "Edit.Redo";
constexpr auto Remove          = "Edit.Remove";
} // namespace Actions

namespace Mime {
constexpr auto PlaylistItems = "application/x-fooyin-playlistitems";
constexpr auto TrackList     = "application/x-fooyin-tracks";
} // namespace Mime

namespace Page {
constexpr auto GeneralCore           = "Fooyin.Page.General.Core";
constexpr auto Engine                = "Fooyin.Page.Engine";
constexpr auto InterfaceGeneral      = "Fooyin.Page.Interface.General";
constexpr auto LibraryGeneral        = "Fooyin.Page.Library.General";
constexpr auto LibrarySorting        = "Fooyin.Page.Library.Sorting";
constexpr auto PlaylistGeneral       = "Fooyin.Page.Playlist.General";
constexpr auto PlaylistInterface     = "Fooyin.Page.Playlist.Interface";
constexpr auto PlaylistPresets       = "Fooyin.Page.Playlist.Presets";
constexpr auto LibraryTreeGeneral    = "Fooyin.Page.Widgets.LibraryTree.General";
constexpr auto LibraryTreeAppearance = "Fooyin.Page.Widgets.LibraryTree.Appearance";
constexpr auto Plugins               = "Fooyin.Page.Plugins";
constexpr auto Shortcuts             = "Fooyin.Page.Shortcuts";
} // namespace Page
} // namespace Fooyin::Constants
