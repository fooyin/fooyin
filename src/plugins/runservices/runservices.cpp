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

#include "runservices.h"

#include <utils/settings/settingsmanager.h>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

using namespace Qt::StringLiterals;

constexpr auto IdKey               = "id"_L1;
constexpr auto NameKey             = "name"_L1;
constexpr auto PathKey             = "path"_L1;
constexpr auto SimultaneousRunsKey = "simultaneousRuns"_L1;
constexpr auto EnabledKey          = "enabled"_L1;

namespace Fooyin::RunServices {
namespace {
QString generateId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
} // namespace

std::vector<RunService> runServices(const SettingsManager& settings)
{
    return runServicesFromJson(settings.value<Settings::RunServices::Services>());
}

void setRunServices(SettingsManager& settings, const std::vector<RunService>& services)
{
    settings.set<Settings::RunServices::Services>(runServicesJson(services));
}

std::vector<RunService> runServicesFromJson(const QByteArray& data)
{
    const auto document           = QJsonDocument::fromJson(data);
    const QJsonArray serviceArray = document.array();

    std::vector<RunService> services;
    services.reserve(serviceArray.size());
    std::vector<QString> serviceIds;
    serviceIds.reserve(serviceArray.size());

    for(const auto& item : serviceArray) {
        const QJsonObject object = item.toObject();

        RunService service;
        service.id               = object.value(IdKey).toString().trimmed();
        service.name             = object.value(NameKey).toString().trimmed();
        service.path             = object.value(PathKey).toString().trimmed();
        service.simultaneousRuns = std::clamp(object.value(SimultaneousRunsKey).toInt(1), 1, 99);
        service.enabled          = object.value(EnabledKey).toBool(true);

        if(service.name.isEmpty() || service.path.isEmpty()) {
            continue;
        }

        if(service.id.isEmpty() || std::ranges::find(serviceIds, service.id) != serviceIds.cend()) {
            service.id = generateId();
            while(std::ranges::find(serviceIds, service.id) != serviceIds.cend()) {
                service.id = generateId();
            }
        }

        serviceIds.emplace_back(service.id);
        services.emplace_back(std::move(service));
    }

    return services;
}

QByteArray runServicesJson(const std::vector<RunService>& services)
{
    QJsonArray array;

    for(const RunService& service : services) {
        array.append(QJsonObject{{IdKey, service.id},
                                 {NameKey, service.name},
                                 {PathKey, service.path},
                                 {SimultaneousRunsKey, std::clamp(service.simultaneousRuns, 1, 99)},
                                 {EnabledKey, service.enabled}});
    }

    return QJsonDocument{array}.toJson(QJsonDocument::Compact);
}

QByteArray defaultRunServicesJson()
{
    return runServicesJson(defaultRunServices());
}

std::vector<RunService> defaultRunServices()
{
    return {
        {.id      = u"OpenDirectory"_s,
         .name    = QCoreApplication::translate("RunServices", "Open Directory"),
         .path    = uR"(\"%path%\")"_s,
         .enabled = false},
        {.id      = u"GoogleArtist"_s,
         .name    = QCoreApplication::translate("RunServices", "Google Artist"),
         .path    = u"https://www.google.com/search?q=$replace(%artist%, ,+)&ie=utf-8"_s,
         .enabled = false},
        {.id      = u"GoogleArtistTitle"_s,
         .name    = QCoreApplication::translate("RunServices", "Google Artist + Title"),
         .path    = u"https://www.google.com/search?q=$replace(%artist%+%title%, ,+)&ie=utf-8"_s,
         .enabled = false},
        {.id      = u"WikipediaArtist"_s,
         .name    = QCoreApplication::translate("RunServices", "Wikipedia Artist"),
         .path    = u"https://en.wikipedia.org/wiki/Special:Search?search=$replace(%artist%, ,_)"_s,
         .enabled = false},
    };
}
} // namespace Fooyin::RunServices
