/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <LukeT1@proton.me>
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

namespace Settings::Discord {
Q_NAMESPACE
enum DiscordSettings : uint32_t
{
    ClientId       = 1 | Type::String,
    ArtistField    = 2 | Type::String,
    AlbumField     = 3 | Type::String,
    TitleField     = 4 | Type::String,
    ShowStateIcon  = 5 | Type::Bool,
    DiscordEnabled = 6 | Type::Bool,
};
Q_ENUM_NS(DiscordSettings)
} // namespace Settings::Discord

namespace Discord {
class DiscordSettings
{
public:
    explicit DiscordSettings(SettingsManager* settingsManager);

private:
    SettingsManager* m_settings;
};
} // namespace Discord
} // namespace Fooyin
