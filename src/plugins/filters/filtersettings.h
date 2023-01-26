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

namespace Filters::Settings {
Q_NAMESPACE
enum Filters : uint32_t
{
    FilterAltColours = 1 | Core::Settings::Type::Bool,
    FilterHeader     = 2 | Core::Settings::Type::Bool,
    FilterScrollBar  = 3 | Core::Settings::Type::Bool,
};
Q_ENUM_NS(Filters)

class FiltersSettings
{
public:
    FiltersSettings(Core::SettingsManager* settingsManager);
    ~FiltersSettings() = default;

private:
    Core::SettingsManager* m_settings;
};
} // namespace Filters::Settings
