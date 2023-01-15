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
    Version            = 1 | Type::String,
    DatabaseVersion    = 2 | Type::String,
    FirstRun           = 3 | Type::Bool,
    LayoutEditing      = 4 | Type::Bool,
    Geometry           = 5 | Type::ByteArray,
    Layout             = 6 | Type::ByteArray,
    SplitterHandles    = 7 | Type::Bool,
    DiscHeaders        = 8 | Type::Bool,
    SplitDiscs         = 9 | Type::Bool,
    SimplePlaylist     = 10 | Type::Bool,
    PlaylistAltColours = 11 | Type::Bool,
    PlaylistHeader     = 12 | Type::Bool,
    PlaylistScrollBar  = 13 | Type::Bool,
    PlayMode           = 14,
    ElapsedTotal       = 15 | Type::Bool,
    InfoAltColours     = 16 | Type::Bool,
    InfoHeader         = 17 | Type::Bool,
    InfoScrollBar      = 18 | Type::Bool,
};
Q_ENUM_NS(Core)

class CoreSettings
{
public:
    CoreSettings();
    ~CoreSettings();

private:
    SettingsManager* m_settings;
};
} // namespace Core::Settings
