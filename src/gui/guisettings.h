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
    SplitterHandles    = 3 | Core::Settings::Type::Bool,
    DiscHeaders        = 4 | Core::Settings::Type::Bool,
    SplitDiscs         = 5 | Core::Settings::Type::Bool,
    SimplePlaylist     = 6 | Core::Settings::Type::Bool,
    PlaylistAltColours = 7 | Core::Settings::Type::Bool,
    PlaylistHeader     = 8 | Core::Settings::Type::Bool,
    PlaylistScrollBar  = 9 | Core::Settings::Type::Bool,
    ElapsedTotal       = 10 | Core::Settings::Type::Bool,
    InfoAltColours     = 11 | Core::Settings::Type::Bool,
    InfoHeader         = 12 | Core::Settings::Type::Bool,
    InfoScrollBar      = 13 | Core::Settings::Type::Bool,
};
Q_ENUM_NS(Gui)

class GuiSettings
{
public:
    GuiSettings(Core::SettingsManager* settingsManager);

private:
    Core::SettingsManager* m_settings;
};
} // namespace Gui::Settings
