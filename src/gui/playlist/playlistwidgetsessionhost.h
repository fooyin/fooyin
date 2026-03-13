/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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

#include <QMetaObject>

namespace Fooyin {
class MusicLibrary;
class PlayerController;
class PlaylistInteractor;
class PlaylistController;
class PlaylistModel;
class PlaylistView;
class PlaylistWidget;
struct PlaylistWidgetLayoutState;
class LibraryManager;
class SettingsManager;
class SignalThrottler;
class TrackSelectionController;
class WidgetContext;

class PlaylistWidgetSessionHost
{
public:
    virtual ~PlaylistWidgetSessionHost() = default;

    [[nodiscard]] virtual PlaylistWidget* sessionWidget()                       = 0;
    [[nodiscard]] virtual bool sessionVisible() const                           = 0;
    [[nodiscard]] virtual PlaylistController* playlistController() const        = 0;
    [[nodiscard]] virtual PlayerController* playerController() const            = 0;
    [[nodiscard]] virtual MusicLibrary* musicLibrary() const                    = 0;
    [[nodiscard]] virtual PlaylistInteractor* playlistInteractor() const        = 0;
    [[nodiscard]] virtual SettingsManager* settingsManager() const              = 0;
    [[nodiscard]] virtual SignalThrottler* resetThrottler() const               = 0;
    [[nodiscard]] virtual LibraryManager* libraryManager() const                = 0;
    [[nodiscard]] virtual TrackSelectionController* selectionController() const = 0;
    [[nodiscard]] virtual WidgetContext* playlistContext() const                = 0;
    [[nodiscard]] virtual PlaylistModel* playlistModel() const                  = 0;
    [[nodiscard]] virtual PlaylistView* playlistView() const                    = 0;
    [[nodiscard]] virtual const PlaylistWidgetLayoutState& layoutState() const  = 0;
    [[nodiscard]] virtual bool hasDelayedStateLoad() const                      = 0;

    virtual void resetModel()                                            = 0;
    virtual void resetModelThrottled() const                             = 0;
    virtual void resetSort(bool force)                                   = 0;
    virtual void followCurrentTrack()                                    = 0;
    virtual void sessionHandleRestoredState()                            = 0;
    virtual void clearDelayedStateLoad()                                 = 0;
    virtual void setDelayedStateLoad(QMetaObject::Connection connection) = 0;
};
} // namespace Fooyin
