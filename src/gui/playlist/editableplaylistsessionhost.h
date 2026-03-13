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

#include "playlistwidgetsessionhost.h"

class QAction;

namespace Fooyin {
class ActionManager;
enum class TrackAction;
struct PlaylistPreset;
class PresetRegistry;

class EditablePlaylistSessionHost : public PlaylistWidgetSessionHost
{
public:
    ~EditablePlaylistSessionHost() override = default;

    [[nodiscard]] virtual ActionManager* actionManager() const     = 0;
    [[nodiscard]] virtual PresetRegistry* presetRegistry() const   = 0;
    virtual void handlePresetChanged(const PlaylistPreset& preset) = 0;
    virtual void setHeaderVisible(bool visible)                    = 0;
    virtual void setScrollbarVisible(bool visible)                 = 0;
    virtual void setMiddleClickAction(TrackAction action)          = 0;
};
} // namespace Fooyin
