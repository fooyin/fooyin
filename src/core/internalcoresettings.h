/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

namespace Fooyin {
class SettingsManager;
class SortingRegistry;

namespace Settings::Core::Internal {
Q_NAMESPACE

enum CoreInternalSettings : uint32_t
{
    MonitorLibraries = 0 | Settings::Bool
};
Q_ENUM_NS(CoreInternalSettings)
} // namespace Settings::Core::Internal

class CoreSettings
{
public:
    explicit CoreSettings(SettingsManager* settingsManager);
    ~CoreSettings();

    void shutdown();

    [[nodiscard]] SortingRegistry* sortingRegistry() const;

private:
    SettingsManager* m_settings;
    std::unique_ptr<SortingRegistry> m_sortingRegistry;
};
} // namespace Fooyin
