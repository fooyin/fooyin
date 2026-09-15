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

namespace Fooyin::Filters::FilterContextMenu {
constexpr auto PageId              = "Fooyin.Page.Interface.ContextMenu.LibraryFilter";
constexpr auto DisabledSectionsKey = "Interface/ContextMenuLibraryFilterDisabledSections";
constexpr auto LayoutKey           = "Interface/ContextMenuLibraryFilterLayout";

constexpr auto Playlist          = "Fooyin.Context.LibraryFilter.Playlist";
constexpr auto PlaylistSeparator = "Fooyin.Context.LibraryFilter.Playlist.Separator";
constexpr auto Queue             = "Fooyin.Context.LibraryFilter.Queue";
constexpr auto QueueSeparator    = "Fooyin.Context.LibraryFilter.Queue.Separator";
constexpr auto FilterOptions     = "Fooyin.Context.LibraryFilter.FilterOptions";
constexpr auto Configure         = "Fooyin.Context.LibraryFilter.Configure";
constexpr auto WidgetSeparator   = "Fooyin.Context.LibraryFilter.Widget.Separator";
constexpr auto TrackActions      = "Fooyin.Context.LibraryFilter.TrackActions";

inline QStringList defaultDisabledSections()
{
    return {QString::fromLatin1(Constants::Actions::AddToPlaylist)};
}

constexpr auto DefaultItems = std::to_array<StaticContextMenu::Item>({
    {.id    = Constants::Actions::AddToCurrent,
     .title = {.context = "FilterWidget", .sourceText = QT_TRANSLATE_NOOP("FilterWidget", "Add to current playlist")},
     .isSeparator = false},
    {.id    = Constants::Actions::AddToActive,
     .title = {.context = "FilterWidget", .sourceText = QT_TRANSLATE_NOOP("FilterWidget", "Add to active playlist")},
     .isSeparator = false},
    {.id    = Constants::Actions::SendToCurrent,
     .title = {.context = "FilterWidget", .sourceText = QT_TRANSLATE_NOOP("FilterWidget", "Replace current playlist")},
     .isSeparator = false},
    {.id          = Constants::Actions::SendToNew,
     .title       = {.context = "FilterWidget", .sourceText = QT_TRANSLATE_NOOP("FilterWidget", "Create new playlist")},
     .isSeparator = false},
    {.id          = Constants::Actions::AddToPlaylist,
     .title       = {.context = "FilterWidget", .sourceText = QT_TRANSLATE_NOOP("FilterWidget", "Add to playlist")},
     .isSeparator = false},
    {.id = PlaylistSeparator, .title = {}, .isSeparator = true},
    {.id    = Constants::Actions::AddToQueue,
     .title = {.context = "FilterWidget", .sourceText = QT_TRANSLATE_NOOP("FilterWidget", "Add to playback queue")},
     .isSeparator = false},
    {.id          = Constants::Actions::QueueNext,
     .title       = {.context = "FilterWidget", .sourceText = QT_TRANSLATE_NOOP("FilterWidget", "Queue to play next")},
     .isSeparator = false},
    {.id = Constants::Actions::RemoveFromQueue,
     .title
     = {.context = "FilterWidget", .sourceText = QT_TRANSLATE_NOOP("FilterWidget", "Remove from playback queue")},
     .isSeparator = false},
    {.id = QueueSeparator, .title = {}, .isSeparator = true},
    {.id          = FilterOptions,
     .title       = {.context = "FilterWidget", .sourceText = QT_TRANSLATE_NOOP("FilterWidget", "Filter options")},
     .isSeparator = false},
    {.id          = Configure,
     .title       = {.context = "FilterWidget", .sourceText = QT_TRANSLATE_NOOP("FilterWidget", "Configure")},
     .isSeparator = false},
    {.id = WidgetSeparator, .title = {}, .isSeparator = true},
    {.id          = TrackActions,
     .title       = {.context = "FilterWidget", .sourceText = QT_TRANSLATE_NOOP("FilterWidget", "Track menu")},
     .isSeparator = false},
});

} // namespace Fooyin::Filters::FilterContextMenu
