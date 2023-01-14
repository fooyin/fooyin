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

#include "core/settings/settingsmanager.h"

#include <QObject>

namespace Core::Settings {
Q_NAMESPACE
enum Core : uint32_t
{
    Version            = 1,
    DatabaseVersion    = 2,
    FirstRun           = 3,
    LayoutEditing      = 4,
    Geometry           = 5,
    Layout             = 6,
    SplitterHandles    = 7,
    DiscHeaders        = 8,
    SplitDiscs         = 9,
    SimplePlaylist     = 10,
    PlaylistAltColours = 11,
    PlaylistHeader     = 12,
    PlaylistScrollBar  = 13,
    PlayMode           = 14,
    ElapsedTotal       = 15,
    FilterAltColours   = 16,
    FilterHeader       = 17,
    FilterScrollBar    = 18,
    InfoAltColours     = 19,
    InfoHeader         = 20,
    InfoScrollBar      = 21,
};
Q_ENUM_NS(Core);

class CoreSettings
{
public:
    CoreSettings();
    ~CoreSettings();

private:
    SettingsManager* m_settings;
};
}; // namespace Core::Settings
