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

#pragma once

#include <gui/guiconstants.h>
#include <gui/settings/context/staticcontextmenu.h>

#include <array>

namespace Fooyin::ContextMenuIds {
using Item = StaticContextMenu::Item;
using StaticContextMenu::defaultLayoutIds;
using StaticContextMenu::isBuiltInSeparatorId;

namespace TrackSelection {
constexpr auto ArtworkSearchSeparator = "Fooyin.Menu.Artwork.SearchSeparator";
constexpr auto ArtworkAttachSeparator = "Fooyin.Menu.Artwork.AttachSeparator";
constexpr auto FileActionsSeparator   = "Fooyin.Menu.Track.FileActionsSeparator";
} // namespace TrackSelection

namespace LayoutEditing {
constexpr auto WidgetActions   = "Fooyin.Context.LayoutEditing.WidgetActions";
constexpr auto WidgetSeparator = "Fooyin.Context.LayoutEditing.Widget.Separator";
constexpr auto Replace         = "Fooyin.Context.LayoutEditing.Replace";
constexpr auto Split           = "Fooyin.Context.LayoutEditing.Split";
constexpr auto RemoveSplit     = "Fooyin.Context.LayoutEditing.RemoveSplit";
constexpr auto EditSeparator   = "Fooyin.Context.LayoutEditing.Edit.Separator";
constexpr auto Copy            = "Fooyin.Context.LayoutEditing.Copy";
constexpr auto Paste           = "Fooyin.Context.LayoutEditing.Paste";
constexpr auto InsertSeparator = "Fooyin.Context.LayoutEditing.Insert.Separator";
constexpr auto Insert          = "Fooyin.Context.LayoutEditing.Insert";
constexpr auto Move            = "Fooyin.Context.LayoutEditing.Move";
constexpr auto RemoveSeparator = "Fooyin.Context.LayoutEditing.Remove.Separator";
constexpr auto Remove          = "Fooyin.Context.LayoutEditing.Remove";

constexpr auto DefaultItems = std::to_array<Item>({
    {.id          = WidgetActions,
     .title       = {.context = "EditableLayout", .sourceText = QT_TRANSLATE_NOOP("EditableLayout", "Widget actions")},
     .isSeparator = false},
    {.id = WidgetSeparator, .title = {}, .isSeparator = true},
    {.id          = Replace,
     .title       = {.context = "EditableLayout", .sourceText = QT_TRANSLATE_NOOP("EditableLayout", "Replace")},
     .isSeparator = false},
    {.id          = Split,
     .title       = {.context = "EditableLayout", .sourceText = QT_TRANSLATE_NOOP("EditableLayout", "Split")},
     .isSeparator = false},
    {.id          = RemoveSplit,
     .title       = {.context = "EditableLayout", .sourceText = QT_TRANSLATE_NOOP("EditableLayout", "Remove split")},
     .isSeparator = false},
    {.id = EditSeparator, .title = {}, .isSeparator = true},
    {.id          = Copy,
     .title       = {.context = "EditableLayout", .sourceText = QT_TRANSLATE_NOOP("EditableLayout", "Copy")},
     .isSeparator = false},
    {.id          = Paste,
     .title       = {.context = "EditableLayout", .sourceText = QT_TRANSLATE_NOOP("EditableLayout", "Paste")},
     .isSeparator = false},
    {.id = InsertSeparator, .title = {}, .isSeparator = true},
    {.id          = Insert,
     .title       = {.context = "EditableLayout", .sourceText = QT_TRANSLATE_NOOP("EditableLayout", "Insert")},
     .isSeparator = false},
    {.id          = Move,
     .title       = {.context = "EditableLayout", .sourceText = QT_TRANSLATE_NOOP("EditableLayout", "Move")},
     .isSeparator = false},
    {.id = RemoveSeparator, .title = {}, .isSeparator = true},
    {.id          = Remove,
     .title       = {.context = "EditableLayout", .sourceText = QT_TRANSLATE_NOOP("EditableLayout", "Remove")},
     .isSeparator = false},
});
} // namespace LayoutEditing

namespace Playlist {
constexpr auto Play               = "Fooyin.Context.Playlist.Play";
constexpr auto StopAfterThis      = "Fooyin.Context.Playlist.StopAfterThis";
constexpr auto PlaybackSeparator  = "Fooyin.Context.Playlist.Playback.Separator";
constexpr auto Remove             = "Fooyin.Context.Playlist.Remove";
constexpr auto MutateSeparator    = "Fooyin.Context.Playlist.Mutate.Separator";
constexpr auto Crop               = "Fooyin.Context.Playlist.Crop";
constexpr auto Sort               = "Fooyin.Context.Playlist.Sort";
constexpr auto Clipboard          = "Fooyin.Context.Playlist.Clipboard";
constexpr auto ClipboardSeparator = "Fooyin.Context.Playlist.Clipboard.Separator";
constexpr auto Presets            = "Fooyin.Context.Playlist.Presets";
constexpr auto PresetsSeparator   = "Fooyin.Context.Playlist.Presets.Separator";
constexpr auto Queue              = "Fooyin.Context.Playlist.Queue";
constexpr auto TrackSeparator     = "Fooyin.Context.Playlist.Track.Separator";
constexpr auto TrackActions       = "Fooyin.Context.Playlist.TrackActions";

constexpr auto DefaultItems = std::to_array<Item>({
    {.id          = Play,
     .title       = {.context = "PlaylistWidget", .sourceText = QT_TRANSLATE_NOOP("PlaylistWidget", "Play")},
     .isSeparator = false},
    {.id          = StopAfterThis,
     .title       = {.context = "PlaylistWidget", .sourceText = QT_TRANSLATE_NOOP("PlaylistWidget", "Stop after this")},
     .isSeparator = false},
    {.id = PlaybackSeparator, .title = {}, .isSeparator = true},
    {.id          = Remove,
     .title       = {.context = "PlaylistWidget", .sourceText = QT_TRANSLATE_NOOP("PlaylistWidget", "Remove")},
     .isSeparator = false},
    {.id          = Crop,
     .title       = {.context = "PlaylistWidget", .sourceText = QT_TRANSLATE_NOOP("PlaylistWidget", "Crop")},
     .isSeparator = false},
    {.id          = Sort,
     .title       = {.context = "PlaylistWidget", .sourceText = QT_TRANSLATE_NOOP("PlaylistWidget", "Sort")},
     .isSeparator = false},
    {.id = MutateSeparator, .title = {}, .isSeparator = true},
    {.id          = Clipboard,
     .title       = {.context = "PlaylistWidget", .sourceText = QT_TRANSLATE_NOOP("PlaylistWidget", "Clipboard")},
     .isSeparator = false},
    {.id = ClipboardSeparator, .title = {}, .isSeparator = true},
    {.id          = Presets,
     .title       = {.context = "PlaylistWidget", .sourceText = QT_TRANSLATE_NOOP("PlaylistWidget", "Presets")},
     .isSeparator = false},
    {.id = PresetsSeparator, .title = {}, .isSeparator = true},
    {.id = Constants::Actions::AddToPlaylist,
     .title
     = {.context = "PlaylistWidget", .sourceText = QT_TRANSLATE_NOOP("PlaylistWidget", "Add to another playlist")},
     .isSeparator = false},
    {.id    = Constants::Actions::AddToQueue,
     .title = {.context = "PlaylistWidget", .sourceText = QT_TRANSLATE_NOOP("PlaylistWidget", "Add to playback queue")},
     .isSeparator = false},
    {.id    = Constants::Actions::QueueNext,
     .title = {.context = "PlaylistWidget", .sourceText = QT_TRANSLATE_NOOP("PlaylistWidget", "Queue to play next")},
     .isSeparator = false},
    {.id = Constants::Actions::RemoveFromQueue,
     .title
     = {.context = "PlaylistWidget", .sourceText = QT_TRANSLATE_NOOP("PlaylistWidget", "Remove from playback queue")},
     .isSeparator = false},
    {.id = TrackSeparator, .title = {}, .isSeparator = true},
    {.id          = TrackActions,
     .title       = {.context = "PlaylistWidget", .sourceText = QT_TRANSLATE_NOOP("PlaylistWidget", "Track menu")},
     .isSeparator = false},
});

} // namespace Playlist

namespace LibraryTree {
constexpr auto Play              = "Fooyin.Context.LibraryTree.Play";
constexpr auto PlaybackSeparator = "Fooyin.Context.LibraryTree.Playback.Separator";
constexpr auto Playlist          = "Fooyin.Context.LibraryTree.Playlist";
constexpr auto PlaylistSeparator = "Fooyin.Context.LibraryTree.Playlist.Separator";
constexpr auto Queue             = "Fooyin.Context.LibraryTree.Queue";
constexpr auto QueueSeparator    = "Fooyin.Context.LibraryTree.Queue.Separator";
constexpr auto OpenFolder        = "Fooyin.Context.LibraryTree.OpenFolder";
constexpr auto TrackSeparator    = "Fooyin.Context.LibraryTree.Track.Separator";
constexpr auto TrackActions      = "Fooyin.Context.LibraryTree.TrackActions";
constexpr auto WidgetSeparator   = "Fooyin.Context.LibraryTree.Widget.Separator";
constexpr auto Grouping          = "Fooyin.Context.LibraryTree.Grouping";
constexpr auto Configure         = "Fooyin.Context.LibraryTree.Configure";

constexpr auto DefaultItems = std::to_array<Item>({
    {.id          = Play,
     .title       = {.context = "LibraryTreeWidget", .sourceText = QT_TRANSLATE_NOOP("LibraryTreeWidget", "Play")},
     .isSeparator = false},
    {.id = PlaybackSeparator, .title = {}, .isSeparator = true},
    {.id          = Constants::Actions::AddToCurrent,
     .title       = {.context    = "LibraryTreeWidget",
                     .sourceText = QT_TRANSLATE_NOOP("LibraryTreeWidget", "Add to current playlist")},
     .isSeparator = false},
    {.id = Constants::Actions::AddToActive,
     .title
     = {.context = "LibraryTreeWidget", .sourceText = QT_TRANSLATE_NOOP("LibraryTreeWidget", "Add to active playlist")},
     .isSeparator = false},
    {.id          = Constants::Actions::SendToCurrent,
     .title       = {.context    = "LibraryTreeWidget",
                     .sourceText = QT_TRANSLATE_NOOP("LibraryTreeWidget", "Replace current playlist")},
     .isSeparator = false},
    {.id = Constants::Actions::SendToNew,
     .title
     = {.context = "LibraryTreeWidget", .sourceText = QT_TRANSLATE_NOOP("LibraryTreeWidget", "Create new playlist")},
     .isSeparator = false},
    {.id    = Constants::Actions::AddToPlaylist,
     .title = {.context = "LibraryTreeWidget", .sourceText = QT_TRANSLATE_NOOP("LibraryTreeWidget", "Add to playlist")},
     .isSeparator = false},
    {.id = PlaylistSeparator, .title = {}, .isSeparator = true},
    {.id = Constants::Actions::AddToQueue,
     .title
     = {.context = "LibraryTreeWidget", .sourceText = QT_TRANSLATE_NOOP("LibraryTreeWidget", "Add to playback queue")},
     .isSeparator = false},
    {.id = Constants::Actions::QueueNext,
     .title
     = {.context = "LibraryTreeWidget", .sourceText = QT_TRANSLATE_NOOP("LibraryTreeWidget", "Queue to play next")},
     .isSeparator = false},
    {.id          = Constants::Actions::RemoveFromQueue,
     .title       = {.context    = "LibraryTreeWidget",
                     .sourceText = QT_TRANSLATE_NOOP("LibraryTreeWidget", "Remove from playback queue")},
     .isSeparator = false},
    {.id = QueueSeparator, .title = {}, .isSeparator = true},
    {.id          = Grouping,
     .title       = {.context = "LibraryTreeWidget", .sourceText = QT_TRANSLATE_NOOP("LibraryTreeWidget", "Grouping")},
     .isSeparator = false},
    {.id          = Configure,
     .title       = {.context = "LibraryTreeWidget", .sourceText = QT_TRANSLATE_NOOP("LibraryTreeWidget", "Configure")},
     .isSeparator = false},
    {.id = WidgetSeparator, .title = {}, .isSeparator = true},
    {.id    = OpenFolder,
     .title = {.context = "LibraryTreeWidget", .sourceText = QT_TRANSLATE_NOOP("LibraryTreeWidget", "Open folder")},
     .isSeparator = false},
    {.id    = TrackActions,
     .title = {.context = "LibraryTreeWidget", .sourceText = QT_TRANSLATE_NOOP("LibraryTreeWidget", "Track menu")},
     .isSeparator = false},
});
} // namespace LibraryTree

namespace DirBrowser {
constexpr auto Play              = "Fooyin.Context.DirBrowser.Play";
constexpr auto PlaybackSeparator = "Fooyin.Context.DirBrowser.Playback.Separator";
constexpr auto PlaylistSeparator = "Fooyin.Context.DirBrowser.Playlist.Separator";
constexpr auto SendQueue         = "Fooyin.Context.DirBrowser.SendQueue";
constexpr auto QueueSeparator    = "Fooyin.Context.DirBrowser.Queue.Separator";
constexpr auto SetRoot           = "Fooyin.Context.DirBrowser.SetRoot";
constexpr auto ViewMode          = "Fooyin.Context.DirBrowser.ViewMode";
constexpr auto Configure         = "Fooyin.Context.DirBrowser.Configure";

constexpr auto DefaultItems = std::to_array<Item>({
    {.id          = Play,
     .title       = {.context = "DirBrowser", .sourceText = QT_TRANSLATE_NOOP("DirBrowser", "Play")},
     .isSeparator = false},
    {.id = PlaybackSeparator, .title = {}, .isSeparator = true},
    {.id          = Constants::Actions::AddToCurrent,
     .title       = {.context = "DirBrowser", .sourceText = QT_TRANSLATE_NOOP("DirBrowser", "Add to current playlist")},
     .isSeparator = false},
    {.id          = Constants::Actions::AddToActive,
     .title       = {.context = "DirBrowser", .sourceText = QT_TRANSLATE_NOOP("DirBrowser", "Add to active playlist")},
     .isSeparator = false},
    {.id    = Constants::Actions::SendToCurrent,
     .title = {.context = "DirBrowser", .sourceText = QT_TRANSLATE_NOOP("DirBrowser", "Replace current playlist")},
     .isSeparator = false},
    {.id          = Constants::Actions::SendToNew,
     .title       = {.context = "DirBrowser", .sourceText = QT_TRANSLATE_NOOP("DirBrowser", "Create new playlist")},
     .isSeparator = false},
    {.id          = Constants::Actions::AddToPlaylist,
     .title       = {.context = "DirBrowser", .sourceText = QT_TRANSLATE_NOOP("DirBrowser", "Add to playlist")},
     .isSeparator = false},
    {.id = PlaylistSeparator, .title = {}, .isSeparator = true},
    {.id          = Constants::Actions::AddToQueue,
     .title       = {.context = "DirBrowser", .sourceText = QT_TRANSLATE_NOOP("DirBrowser", "Add to playback queue")},
     .isSeparator = false},
    {.id          = Constants::Actions::QueueNext,
     .title       = {.context = "DirBrowser", .sourceText = QT_TRANSLATE_NOOP("DirBrowser", "Queue to play next")},
     .isSeparator = false},
    {.id          = SendQueue,
     .title       = {.context = "DirBrowser", .sourceText = QT_TRANSLATE_NOOP("DirBrowser", "Replace playback queue")},
     .isSeparator = false},
    {.id = QueueSeparator, .title = {}, .isSeparator = true},
    {.id          = SetRoot,
     .title       = {.context = "DirBrowser", .sourceText = QT_TRANSLATE_NOOP("DirBrowser", "Set as root")},
     .isSeparator = false},
    {.id          = ViewMode,
     .title       = {.context = "DirBrowser", .sourceText = QT_TRANSLATE_NOOP("DirBrowser", "View mode")},
     .isSeparator = false},
    {.id          = Configure,
     .title       = {.context = "DirBrowser", .sourceText = QT_TRANSLATE_NOOP("DirBrowser", "Configure")},
     .isSeparator = false},
});
} // namespace DirBrowser
} // namespace Fooyin::ContextMenuIds
