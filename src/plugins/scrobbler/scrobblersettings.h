/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

namespace Settings::Scrobbler {
Q_NAMESPACE
enum ScrobblerSettings : uint32_t
{
    ScrobblingEnabled = 0 | Type::Bool,
    ScrobblingDelay   = 1 | Type::Int,
    PreferAlbumArtist = 2 | Type::Bool,
};
Q_ENUM_NS(ScrobblerSettings)
} // namespace Settings::Scrobbler

namespace Scrobbler {
class ScrobblerSettings
{
public:
    explicit ScrobblerSettings(SettingsManager* settings);
};
} // namespace Scrobbler
} // namespace Fooyin
