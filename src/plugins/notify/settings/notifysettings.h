/*
 * Fooyin
 * Copyright 2025, ripdog <https://github.com/ripdog>
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

namespace Settings::Notify {
Q_NAMESPACE
enum NotifySettings : uint32_t
{
    Enabled      = 1 | Type::Bool,
    TitleField   = 2 | Type::String,
    BodyField    = 3 | Type::String,
    ShowAlbumArt = 4 | Type::Bool,
    Timeout      = 5 | Type::Int,
};
Q_ENUM_NS(NotifySettings)
} // namespace Settings::Notify

namespace Notify {
class NotifySettings
{
public:
    explicit NotifySettings(SettingsManager* settingsManager);

private:
    SettingsManager* m_settings;
};
} // namespace Notify
} // namespace Fooyin
