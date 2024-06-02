/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <utils/settings/settingsentry.h>

#include <QObject>

namespace Fooyin {
class SettingsManager;

namespace Settings::Filters {
Q_NAMESPACE
enum FiltersSettings : uint32_t
{
    FilterAltColours      = 1 | Type::Bool,
    FilterHeader          = 2 | Type::Bool,
    FilterScrollBar       = 3 | Type::Bool,
    FilterDoubleClick     = 4 | Type::Int,
    FilterMiddleClick     = 5 | Type::Int,
    FilterPlaylistEnabled = 6 | Type::Bool,
    FilterAutoSwitch      = 7 | Type::Bool,
    FilterAutoPlaylist    = 8 | Type::String,
    FilterFont            = 9 | Type::String,
    FilterColour          = 10 | Type::String,
    FilterRowHeight       = 11 | Type::Int,
    FilterSendPlayback    = 12 | Type::Bool,
    FilterKeepAlive       = 13 | Type::Bool

};
Q_ENUM_NS(FiltersSettings)
} // namespace Settings::Filters

namespace Filters {
class FiltersSettings
{
public:
    explicit FiltersSettings(SettingsManager* settingsManager);

private:
    SettingsManager* m_settings;
};
} // namespace Filters
} // namespace Fooyin
