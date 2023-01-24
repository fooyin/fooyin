/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/settings/settingsmanager.h>

#include <QObject>

namespace Gui::Settings {
Q_NAMESPACE
enum Gui : uint32_t
{
    LayoutEditing      = 1 | Core::Settings::Type::Bool,
    Geometry           = 2 | Core::Settings::Type::ByteArray,
    Layout             = 3 | Core::Settings::Type::ByteArray,
    SplitterHandles    = 4 | Core::Settings::Type::Bool,
    DiscHeaders        = 5 | Core::Settings::Type::Bool,
    SplitDiscs         = 6 | Core::Settings::Type::Bool,
    SimplePlaylist     = 7 | Core::Settings::Type::Bool,
    PlaylistAltColours = 8 | Core::Settings::Type::Bool,
    PlaylistHeader     = 9 | Core::Settings::Type::Bool,
    PlaylistScrollBar  = 10 | Core::Settings::Type::Bool,
    ElapsedTotal       = 11 | Core::Settings::Type::Bool,
    InfoAltColours     = 12 | Core::Settings::Type::Bool,
    InfoHeader         = 13 | Core::Settings::Type::Bool,
    InfoScrollBar      = 14 | Core::Settings::Type::Bool,
};
Q_ENUM_NS(Gui)

class GuiSettings
{
public:
    GuiSettings();
    ~GuiSettings();

private:
    ::Core::SettingsManager* m_settings;
};
} // namespace Gui::Settings
