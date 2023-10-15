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

#include "fycore_export.h"

#include <utils/settings/settingtypes.h>

#include <QObject>

namespace FYCORE_EXPORT Fy {
namespace Utils {
class SettingsManager;
}

namespace Core::Settings {
Q_NAMESPACE
enum Core : uint32_t
{
    Version           = 1 | Utils::Settings::String,
    DatabaseVersion   = 2 | Utils::Settings::String,
    FirstRun          = 3 | Utils::Settings::Bool,
    PlayMode          = 4 | Utils::Settings::Int,
    AutoRefresh       = 5 | Utils::Settings::Bool,
    LibrarySorting    = 6 | Utils::Settings::ByteArray,
    LibrarySortScript = 7 | Utils::Settings::String,
    ActivePlaylistId  = 8 | Utils::Settings::Int,
    AudioOutput       = 9 | Utils::Settings::String,
    OutputDevice      = 10 | Utils::Settings::String,
    OutputVolume      = 11 | Utils::Settings::Double,
};
Q_ENUM_NS(Core)

class CoreSettings
{
public:
    explicit CoreSettings(Utils::SettingsManager* settingsManager);

private:
    Utils::SettingsManager* m_settings;
};
} // namespace Core::Settings
} // namespace FYCORE_EXPORT Fy
