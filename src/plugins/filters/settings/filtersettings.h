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

namespace Filters::Settings {
Q_NAMESPACE
enum Filters : uint32_t
{
    FilterAltColours      = 1 | Utils::Settings::Bool,
    FilterHeader          = 2 | Utils::Settings::Bool,
    FilterScrollBar       = 3 | Utils::Settings::Bool,
    FilterFields          = 4 | Utils::Settings::ByteArray,
    FilterFontSize        = 5 | Utils::Settings::Int,
    FilterRowHeight       = 6 | Utils::Settings::Int,
    FilterDoubleClick     = 7 | Utils::Settings::Int,
    FilterMiddleClick     = 8 | Utils::Settings::Int,
    FilterPlaylistEnabled = 9 | Utils::Settings::Bool,
    FilterAutoSwitch      = 10 | Utils::Settings::Bool,
    FilterAutoPlaylist    = 11 | Utils::Settings::String,

};
Q_ENUM_NS(Filters)

class FiltersSettings
{
public:
    explicit FiltersSettings(Utils::SettingsManager* settingsManager);

private:
    Utils::SettingsManager* m_settings;
};
} // namespace Filters::Settings
} // namespace Fy
