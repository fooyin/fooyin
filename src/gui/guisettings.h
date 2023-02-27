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

#include <utils/settings/settingtypes.h>

#include <QObject>

namespace Fy {
namespace Utils {
class SettingsManager;
}

namespace Gui::Settings {
Q_NAMESPACE
enum Gui : uint32_t
{
    LayoutEditing      = 1 | Utils::Settings::Bool,
    Geometry           = 2 | Utils::Settings::ByteArray,
    SplitterHandles    = 3 | Utils::Settings::Bool,
    DiscHeaders        = 4 | Utils::Settings::Bool,
    SplitDiscs         = 5 | Utils::Settings::Bool,
    SimplePlaylist     = 6 | Utils::Settings::Bool,
    PlaylistAltColours = 7 | Utils::Settings::Bool,
    PlaylistHeader     = 8 | Utils::Settings::Bool,
    PlaylistScrollBar  = 9 | Utils::Settings::Bool,
    ElapsedTotal       = 10 | Utils::Settings::Bool,
    InfoAltColours     = 11 | Utils::Settings::Bool,
    InfoHeader         = 12 | Utils::Settings::Bool,
    InfoScrollBar      = 13 | Utils::Settings::Bool,
};
Q_ENUM_NS(Gui)

class GuiSettings
{
public:
    GuiSettings(Utils::SettingsManager* settingsManager);

private:
    Utils::SettingsManager* m_settings;
};
} // namespace Gui::Settings
} // namespace Fy
