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

namespace Fooyin::RadioBrowser::RadioBrowserContextMenu {
constexpr auto PageId              = "Fooyin.Page.Interface.ContextMenu.RadioBrowser";
constexpr auto DisabledSectionsKey = "Interface/ContextMenuRadioBrowserDisabledSections";
constexpr auto LayoutKey           = "Interface/ContextMenuRadioBrowserLayout";

constexpr auto Play              = "Fooyin.Context.RadioBrowser.Play";
constexpr auto PlaybackSeparator = "Fooyin.Context.RadioBrowser.Playback.Separator";
constexpr auto PlaylistSeparator = "Fooyin.Context.RadioBrowser.Playlist.Separator";
constexpr auto Save              = "Fooyin.Context.RadioBrowser.Save";
constexpr auto Remove            = "Fooyin.Context.RadioBrowser.Remove";
constexpr auto Edit              = "Fooyin.Context.RadioBrowser.Edit";
constexpr auto AddCustom         = "Fooyin.Context.RadioBrowser.AddCustom";
constexpr auto StationSeparator  = "Fooyin.Context.RadioBrowser.Station.Separator";
constexpr auto CopyStreamUrl     = "Fooyin.Context.RadioBrowser.CopyStreamUrl";
constexpr auto OpenHomepage      = "Fooyin.Context.RadioBrowser.OpenHomepage";
constexpr auto OpenRadioBrowser  = "Fooyin.Context.RadioBrowser.OpenRadioBrowser";
constexpr auto ViewSeparator     = "Fooyin.Context.RadioBrowser.View.Separator";
constexpr auto Display           = "Fooyin.Context.RadioBrowser.Display";
constexpr auto Configure         = "Fooyin.Context.RadioBrowser.Configure";

inline QStringList defaultDisabledSections()
{
    return {QString::fromLatin1(Constants::Actions::AddToCurrent),
            QString::fromLatin1(Constants::Actions::AddToActive),
            QString::fromLatin1(Constants::Actions::SendToCurrent),
            QString::fromLatin1(Constants::Actions::SendToNew),
            QString::fromLatin1(Constants::Actions::AddToPlaylist),
            QString::fromLatin1(Constants::Actions::AddToQueue),
            QString::fromLatin1(Constants::Actions::QueueNext),
            QString::fromLatin1(Constants::Actions::RemoveFromQueue),
            QString::fromLatin1(OpenRadioBrowser)};
}

constexpr auto DefaultItems = std::to_array<StaticContextMenu::Item>({
    {.id          = Play,
     .title       = {.context = "RadioBrowserWidget", .sourceText = QT_TRANSLATE_NOOP("RadioBrowserWidget", "Play")},
     .isSeparator = false},
    {.id = PlaybackSeparator, .title = {}, .isSeparator = true},
    {.id          = Constants::Actions::AddToCurrent,
     .title       = {.context    = "RadioBrowserWidget",
                     .sourceText = QT_TRANSLATE_NOOP("RadioBrowserWidget", "Add to current playlist")},
     .isSeparator = false},
    {.id          = Constants::Actions::AddToActive,
     .title       = {.context    = "RadioBrowserWidget",
                     .sourceText = QT_TRANSLATE_NOOP("RadioBrowserWidget", "Add to active playlist")},
     .isSeparator = false},
    {.id          = Constants::Actions::SendToCurrent,
     .title       = {.context    = "RadioBrowserWidget",
                     .sourceText = QT_TRANSLATE_NOOP("RadioBrowserWidget", "Replace current playlist")},
     .isSeparator = false},
    {.id = Constants::Actions::SendToNew,
     .title
     = {.context = "RadioBrowserWidget", .sourceText = QT_TRANSLATE_NOOP("RadioBrowserWidget", "Create new playlist")},
     .isSeparator = false},
    {.id = Constants::Actions::AddToPlaylist,
     .title
     = {.context = "RadioBrowserWidget", .sourceText = QT_TRANSLATE_NOOP("RadioBrowserWidget", "Add to playlist")},
     .isSeparator = false},
    {.id = PlaylistSeparator, .title = {}, .isSeparator = true},
    {.id          = Constants::Actions::AddToQueue,
     .title       = {.context    = "RadioBrowserWidget",
                     .sourceText = QT_TRANSLATE_NOOP("RadioBrowserWidget", "Add to playback queue")},
     .isSeparator = false},
    {.id = Constants::Actions::QueueNext,
     .title
     = {.context = "RadioBrowserWidget", .sourceText = QT_TRANSLATE_NOOP("RadioBrowserWidget", "Queue to play next")},
     .isSeparator = false},
    {.id          = Constants::Actions::RemoveFromQueue,
     .title       = {.context    = "RadioBrowserWidget",
                     .sourceText = QT_TRANSLATE_NOOP("RadioBrowserWidget", "Remove from playback queue")},
     .isSeparator = false},
    {.id = StationSeparator, .title = {}, .isSeparator = true},
    {.id = Save,
     .title
     = {.context = "RadioBrowserWidget", .sourceText = QT_TRANSLATE_NOOP("RadioBrowserWidget", "Add to My Stations")},
     .isSeparator = false},
    {.id          = Remove,
     .title       = {.context    = "RadioBrowserWidget",
                     .sourceText = QT_TRANSLATE_NOOP("RadioBrowserWidget", "Remove from My Stations")},
     .isSeparator = false},
    {.id    = Edit,
     .title = {.context = "RadioBrowserWidget", .sourceText = QT_TRANSLATE_NOOP("RadioBrowserWidget", "Edit station")},
     .isSeparator = false},
    {.id = AddCustom,
     .title
     = {.context = "RadioBrowserWidget", .sourceText = QT_TRANSLATE_NOOP("RadioBrowserWidget", "Add custom station")},
     .isSeparator = false},
    {.id = CopyStreamUrl,
     .title
     = {.context = "RadioBrowserWidget", .sourceText = QT_TRANSLATE_NOOP("RadioBrowserWidget", "Copy stream URL")},
     .isSeparator = false},
    {.id    = OpenHomepage,
     .title = {.context = "RadioBrowserWidget", .sourceText = QT_TRANSLATE_NOOP("RadioBrowserWidget", "Open homepage")},
     .isSeparator = false},
    {.id          = OpenRadioBrowser,
     .title       = {.context    = "RadioBrowserWidget",
                     .sourceText = QT_TRANSLATE_NOOP("RadioBrowserWidget", "Open radio-browser.info page")},
     .isSeparator = false},
    {.id = ViewSeparator, .title = {}, .isSeparator = true},
    {.id          = Display,
     .title       = {.context = "RadioBrowserWidget", .sourceText = QT_TRANSLATE_NOOP("RadioBrowserWidget", "Display")},
     .isSeparator = false},
    {.id    = Configure,
     .title = {.context = "RadioBrowserWidget", .sourceText = QT_TRANSLATE_NOOP("RadioBrowserWidget", "Configure")},
     .isSeparator = false},
});
} // namespace Fooyin::RadioBrowser::RadioBrowserContextMenu
