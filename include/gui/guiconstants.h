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
constexpr auto LightIconTheme = "light";
constexpr auto DarkIconTheme  = "dark";

namespace Icons {
constexpr auto Fooyin       = "fooyin";
constexpr auto FooyinWindow = "org.fooyin.fooyin";
constexpr auto NoCover      = "nocover";
constexpr auto Play         = "media-playback-start";
constexpr auto Pause        = "media-playback-pause";
constexpr auto Stop         = "media-playback-stop";
constexpr auto Prev         = "media-skip-backward";
constexpr auto Next         = "media-skip-forward";
constexpr auto Repeat       = "media-playlist-repeat";
constexpr auto RepeatTrack  = "media-playlist-repeat-song";
constexpr auto Shuffle      = "media-playlist-shuffle";
constexpr auto VolumeHigh   = "audio-volume-high";
constexpr auto VolumeMed    = "audio-volume-medium";
constexpr auto VolumeLow    = "audio-volume-low";
constexpr auto VolumeMute   = "audio-volume-muted";
constexpr auto Font         = "preferences-desktop-font";
constexpr auto Add          = "list-add";
constexpr auto Remove       = "list-remove";
constexpr auto Up           = "go-up";
constexpr auto Down         = "go-down";
constexpr auto GoNext       = "go-next";
constexpr auto GoPrevious   = "go-previous";
constexpr auto Close        = "window-close";
constexpr auto Clear        = "edit-clear";
constexpr auto ScriptEditor = "view-scripteditor";
constexpr auto Log          = "view-log";

constexpr auto ScriptExpression = "script-expression";
constexpr auto ScriptVariable   = "script-variable";
constexpr auto ScriptFunction   = "script-function";
constexpr auto ScriptLiteral    = "script-literal";

constexpr auto Quit          = "application-exit";
constexpr auto Settings      = "preferences-system";
constexpr auto RescanLibrary = "view-refresh";
constexpr auto LayoutEditing = "applications-graphics";
constexpr auto QuickSetup    = "preferences-desktop";
constexpr auto Options       = "preferences-other";
} // namespace Icons

constexpr auto MenuBar = "Fooyin.MenuBar";

namespace Menus {
constexpr auto File          = "Fooyin.Menu.File";
constexpr auto Edit          = "Fooyin.Menu.Edit";
constexpr auto View          = "Fooyin.Menu.View";
constexpr auto Layout        = "Fooyin.Menu.Layout";
constexpr auto Playback      = "Fooyin.Menu.Playback";
constexpr auto PlaybackOrder = "Fooyin.Menu.Playback.Order";
constexpr auto Library       = "Fooyin.Menu.Library";
constexpr auto Database      = "Fooyin.Menu.Database";
constexpr auto SwitchLibrary = "Fooyin.Menu.Library.Switch";
constexpr auto Help          = "Fooyin.Menu.Help";

namespace Context {
constexpr auto Layout         = "Fooyin.Menu.Layout";
constexpr auto AddWidget      = "Fooyin.Menu.Widget.Add";
constexpr auto TrackSelection = "Fooyin.Menu.Tracks";
constexpr auto TrackQueue     = "Fooyin.Menu.Queue";
constexpr auto TracksPlaylist = "Fooyin.Menu.Tracks.Playlist";
constexpr auto Playlist       = "Fooyin.Menu.Playlist";
constexpr auto PlaylistAddTo  = "Fooyin.Menu.Playlist.AddTo";
constexpr auto ReplayGain     = "Fooyin.Menu.ReplayGain";
constexpr auto Artwork        = "Fooyin.Menu.Artwork";
constexpr auto Tagging        = "Fooyin.Menu.Tagging";
constexpr auto Utilities      = "Fooyin.Menu.Utilities";
} // namespace Context
} // namespace Menus

namespace Groups {
constexpr auto File     = "Fooyin.Group.File";
constexpr auto Edit     = "Fooyin.Group.Edit";
constexpr auto View     = "Fooyin.Group.View";
constexpr auto Layout   = "Fooyin.Group.View";
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
constexpr auto Playlist   = "Fooyin.Context.Playlist";
constexpr auto DirBrowser = "Fooyin.Context.DirBrowser";
constexpr auto Search     = "Fooyin.Context.Search";
} // namespace Context

namespace Actions {
constexpr auto AddFiles              = "File.AddFiles";
constexpr auto AddFolders            = "File.AddFolders";
constexpr auto NewPlaylist           = "File.NewPlaylist";
constexpr auto NewAutoPlaylist       = "File.NewAutoPlaylist";
constexpr auto EditAutoPlaylist      = "File.EditAutoPlaylist";
constexpr auto RemovePlaylist        = "File.RemovePlaylist";
constexpr auto LoadPlaylist          = "File.LoadPlaylist";
constexpr auto SavePlaylist          = "File.SavePlaylist";
constexpr auto SaveAllPlaylists      = "File.SaveAllPlaylists";
constexpr auto New                   = "File.New";
constexpr auto Exit                  = "File.Exit";
constexpr auto Settings              = "Edit.Settings";
constexpr auto SearchPlaylist        = "Edit.SearchPlaylist";
constexpr auto LayoutEditing         = "Layout.LayoutEditing";
constexpr auto LockSplitters         = "Layout.LockSplitters";
constexpr auto QuickSetup            = "View.QuickSetup";
constexpr auto ShowNowPlaying        = "View.ShowNowPlaying";
constexpr auto QuickSearch           = "Library.QuickSearch";
constexpr auto SearchLibrary         = "Library.SearchLibrary";
constexpr auto Refresh               = "Library.Refresh";
constexpr auto Rescan                = "Library.Rescan";
constexpr auto Stop                  = "Playback.Stop";
constexpr auto PlayPause             = "Playback.PlayPause";
constexpr auto Next                  = "Playback.Next";
constexpr auto Previous              = "Playback.Previous";
constexpr auto PlaybackDefault       = "Playback.Order.Default";
constexpr auto RepeatTrack           = "Playback.Order.RepeatTrack";
constexpr auto RepeatAlbum           = "Playback.Order.RepeatAlbum";
constexpr auto RepeatPlaylist        = "Playback.Order.RepeatPlaylist";
constexpr auto ShuffleTracks         = "Playback.Order.ShuffleTracks";
constexpr auto ShuffleAlbums         = "Playback.Order.ShuffleAlbums";
constexpr auto Random                = "Playback.Order.Random";
constexpr auto StopAfterCurrent      = "Playback.StopAfterCurrent";
constexpr auto StopAfterCurrentReset = "Playback.StopAfterCurrent.Reset";
constexpr auto ScriptEditor          = "View.ScriptEditor";
constexpr auto Log                   = "View.Log";
constexpr auto Cut                   = "Edit.Cut";
constexpr auto Copy                  = "Edit.Copy";
constexpr auto Paste                 = "Edit.Paste";
constexpr auto SelectAll             = "Edit.SelectAll";
constexpr auto Clear                 = "Edit.Clear";
constexpr auto Undo                  = "Edit.Undo";
constexpr auto Redo                  = "Edit.Redo";
constexpr auto Remove                = "Edit.Remove";
constexpr auto Rename                = "Edit.Rename";
constexpr auto VolumeUp              = "Volume.VolumeUp";
constexpr auto VolumeDown            = "Volume.VolumeDown";
constexpr auto Mute                  = "Volume.Mute";
constexpr auto SeekForwardSmall      = "Seek.Forward.Small";
constexpr auto SeekForwardLarge      = "Seek.Forward.Large";
constexpr auto SeekBackwardSmall     = "Seek.Backward.Small";
constexpr auto SeekBackwardLarge     = "Seek.Backward.Large";
constexpr auto Rate0                 = "Tagging.Rate0";
constexpr auto Rate1                 = "Tagging.Rate1";
constexpr auto Rate2                 = "Tagging.Rate2";
constexpr auto Rate3                 = "Tagging.Rate3";
constexpr auto Rate4                 = "Tagging.Rate4";
constexpr auto Rate5                 = "Tagging.Rate5";
constexpr auto AddToCurrent          = "Tracks.AddToCurrent";
constexpr auto AddToActive           = "Tracks.AddToActive";
constexpr auto SendToCurrent         = "Tracks.SendToCurrent";
constexpr auto SendToNew             = "Tracks.SendToNew";
constexpr auto AddToQueue            = "Tracks.AddToQueue";
constexpr auto QueueNext             = "Tracks.QueueNext";
constexpr auto SendToQueue           = "Tracks.SendToQueue";
constexpr auto RemoveFromQueue       = "Tracks.RemoveFromQueue";
constexpr auto OpenFolder            = "Tracks.OpenFolder";
constexpr auto SearchArtwork         = "Tracks.SearchArtwork";
constexpr auto SearchArtworkQuick    = "Tracks.SearchArtworkQuick";
constexpr auto RemoveArtwork         = "Tracks.RemoveArtwork";
constexpr auto OpenProperties        = "Tracks.OpenProperties";
} // namespace Actions

namespace Mime {
constexpr auto PlaylistItems = "application/x-fooyin-playlistitems";
constexpr auto TrackIds      = "application/x-fooyin-trackIds";
constexpr auto QueueTracks   = "application/x-fooyin-queuetracks";
} // namespace Mime

namespace Page {
constexpr auto GeneralCore        = "Fooyin.Page.General.Core";
constexpr auto Playback           = "Fooyin.Page.Playback";
constexpr auto Output             = "Fooyin.Page.Playback.Output";
constexpr auto Decoding           = "Fooyin.Page.Playback.Decoding";
constexpr auto ReplayGain         = "Fooyin.Page.Playback.ReplayGain";
constexpr auto InterfaceGeneral   = "Fooyin.Page.Interface.General";
constexpr auto InterfaceTheme     = "Fooyin.Page.Interface.Theme";
constexpr auto ArtworkGeneral     = "Fooyin.Page.Interface.Artwork.General";
constexpr auto ArtworkDownload    = "Fooyin.Page.Interface.Artwork.Download";
constexpr auto ArtworkSearching   = "Fooyin.Page.Interface.Artwork.Searching";
constexpr auto ArtworkSources     = "Fooyin.Page.Interface.Artwork.Sources";
constexpr auto LibraryGeneral     = "Fooyin.Page.Library.General";
constexpr auto LibrarySearching   = "Fooyin.Page.Library.Searching";
constexpr auto LibrarySorting     = "Fooyin.Page.Library.Sorting";
constexpr auto PlaylistGeneral    = "Fooyin.Page.Playlist.General";
constexpr auto PlaylistInterface  = "Fooyin.Page.Playlist.Interface";
constexpr auto PlaylistPresets    = "Fooyin.Page.Playlist.Presets";
constexpr auto PlaylistColumns    = "Fooyin.Page.Playlist.Columns";
constexpr auto LibraryTreeGeneral = "Fooyin.Page.Widgets.LibraryTree.General";
constexpr auto LibraryTreeGroups  = "Fooyin.Page.Widgets.LibraryTree.Groups";
constexpr auto StatusWidget       = "Fooyin.Page.Widgets.Status";
constexpr auto PlaybackQueue      = "Fooyin.Page.Widgets.PlaybackQueue";
constexpr auto Plugins            = "Fooyin.Page.Plugins";
constexpr auto ShellIntegration   = "Fooyin.Page.ShellIntegration";
constexpr auto Shortcuts          = "Fooyin.Page.Shortcuts";
constexpr auto DirBrowser         = "Fooyin.Page.Widgets.DirBrowser";
constexpr auto Network            = "Fooyin.Page.Network";
} // namespace Page
} // namespace Fooyin::Constants
