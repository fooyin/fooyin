/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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
 */

#pragma once

#include "cddasectorreader.h"

#include <core/coresettings.h>

namespace Fooyin {
class SettingsManager;

namespace Cdda {
constexpr auto MaximumReadOffsetFrames = 44100;
constexpr auto MaximumReadSpeed        = 128;

struct CdDriveSettings
{
    int readOffsetFrames{0};
    CdRippingSecurity security{CdRippingSecurity::Disabled};
    int readSpeedLimit{0}; // For ripping; 0 = max

    bool operator==(const CdDriveSettings&) const = default;
};

[[nodiscard]] CdDriveSettings normaliseDriveSettings(CdDriveSettings settings);
[[nodiscard]] QString cdDriveSettingsKey(const QString& backend, QString vendor, QString model,
                                         const QString& location);
[[nodiscard]] QString cdDriveSettingsGroup(const QString& settingsKey);

class CdDriveSettingsProvider
{
public:
    virtual ~CdDriveSettingsProvider()                                                     = default;
    [[nodiscard]] virtual CdDriveSettings settingsForDrive(const CdDriveInfo& drive) const = 0;
};

class CdDriveSettingsStore final : public CdDriveSettingsProvider
{
public:
    [[nodiscard]] CdDriveSettings settingsForDrive(const CdDriveInfo& drive) const override;
    [[nodiscard]] bool hasSettingsForDrive(const CdDriveInfo& drive) const;
    void setSettingsForDrive(const CdDriveInfo& drive, const CdDriveSettings& settings);

private:
    FySettings m_settings;
};
} // namespace Cdda
} // namespace Fooyin
