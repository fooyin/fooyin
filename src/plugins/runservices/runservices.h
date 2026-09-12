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
 *
 */

#pragma once

#include <utils/settings/settingsentry.h>

#include <QByteArray>
#include <QObject>
#include <QString>

#include <vector>

namespace Fooyin {
class SettingsManager;

namespace Settings::RunServices {
Q_NAMESPACE
enum RunServicesSettings : uint32_t
{
    Services = 1 | Type::ByteArray,
};
Q_ENUM_NS(RunServicesSettings)
} // namespace Settings::RunServices

namespace RunServices {
// Serialised to JSON so we can easily add import/export if needed
struct RunService
{
    QString id;
    QString name;
    QString path;
    int simultaneousRuns{1};
    bool enabled{true};

    bool operator==(const RunService&) const = default;
};

std::vector<RunService> runServices(const SettingsManager& settings);
void setRunServices(SettingsManager& settings, const std::vector<RunService>& services);

std::vector<RunService> runServicesFromJson(const QByteArray& data);
QByteArray runServicesJson(const std::vector<RunService>& services);
std::vector<RunService> defaultRunServices();
QByteArray defaultRunServicesJson();
} // namespace RunServices
} // namespace Fooyin
