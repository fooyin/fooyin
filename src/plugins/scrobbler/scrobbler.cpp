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

#include "scrobbler.h"

#include "scrobblersettings.h"
#include "services/lastfmservice.h"
#include "services/librefmservice.h"
#include "services/listenbrainzservice.h"

#include <core/player/playercontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QIODevice>

using namespace Qt::StringLiterals;

namespace Fooyin::Scrobbler {
Scrobbler::Scrobbler(PlayerController* playerController, std::shared_ptr<NetworkAccessManager> network,
                     SettingsManager* settings)
    : m_playerController{playerController}
    , m_network{std::move(network)}
    , m_settings{settings}
{
    addDefaultServices();
    restoreServices();

    for(auto& service : m_services) {
        service->initialise();
        service->loadSession();
    }

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, &Scrobbler::updateNowPlaying);
    QObject::connect(m_playerController, &PlayerController::trackPlayed, this, &Scrobbler::scrobble);
}

Scrobbler::~Scrobbler()
{
    saveServices();
}

std::vector<ScrobblerService*> Scrobbler::services() const
{
    std::vector<ScrobblerService*> services;
    std::ranges::transform(m_services, std::back_inserter(services), [](const auto& service) { return service.get(); });
    return services;
}

ScrobblerService* Scrobbler::service(const QString& name) const
{
    const auto serviceIt
        = std::ranges::find_if(m_services, [name](const auto& service) { return service->name() == name; });
    if(serviceIt != m_services.cend()) {
        return serviceIt->get();
    }
    return nullptr;
}

void Scrobbler::updateNowPlaying(const Track& track)
{
    for(auto& service : m_services) {
        if(service->isEnabled()) {
            service->updateNowPlaying(track);
        }
    }
}

void Scrobbler::scrobble(const Track& track)
{
    for(auto& service : m_services) {
        if(service->isEnabled()) {
            service->scrobble(track);
        }
    }
}

std::unique_ptr<ScrobblerService> Scrobbler::createCustomService(const ServiceDetails& details)
{
    switch(details.customType) {
        case ServiceDetails::CustomType::AudioScrobbler:
            return std::make_unique<LibreFmService>(details, m_network.get(), m_settings);
        case ServiceDetails::CustomType::ListenBrainz:
            return std::make_unique<ListenBrainzService>(details, m_network.get(), m_settings);
        case ServiceDetails::CustomType::None:
            break;
    }

    return nullptr;
}

ScrobblerService* Scrobbler::addCustomService(const ServiceDetails& details, bool init)
{
    ScrobblerService* service{nullptr};

    switch(details.customType) {
        case ServiceDetails::CustomType::AudioScrobbler:
            service
                = m_services.emplace_back(std::make_unique<LibreFmService>(details, m_network.get(), m_settings)).get();
            break;
        case ServiceDetails::CustomType::ListenBrainz:
            service
                = m_services.emplace_back(std::make_unique<ListenBrainzService>(details, m_network.get(), m_settings))
                      .get();
            break;
        case ServiceDetails::CustomType::None:
            break;
    }

    if(service) {
        if(init) {
            service->initialise();
            service->loadSession();
        }
        return service;
    }

    return nullptr;
}

bool Scrobbler::removeCustomService(ScrobblerService* service)
{
    const auto serviceIt = std::ranges::find_if(m_services, [service](const auto& s) { return s.get() == service; });
    if(serviceIt != m_services.cend()) {
        serviceIt->get()->deleteSession();
        m_services.erase(serviceIt);
        return true;
    }
    return false;
}

void Scrobbler::saveCache()
{
    for(const auto& service : m_services) {
        service->saveCache();
    }
}

void Scrobbler::addDefaultServices()
{
    auto addService = [this]<class T>(const QString& name) {
        m_services.emplace_back(std::make_unique<T>(ServiceDetails{.name       = name,
                                                                   .url        = {},
                                                                   .token      = {},
                                                                   .customType = ServiceDetails::CustomType::None,
                                                                   .isEnabled  = true},
                                                    m_network.get(), m_settings));
    };

    addService.template operator()<LastFmService>(u"LastFM"_s);
    addService.template operator()<LibreFmService>(u"LibreFM"_s);
    addService.template operator()<ListenBrainzService>(u"ListenBrainz"_s);
}

void Scrobbler::saveServices()
{
    QList<ServiceDetails> services;

    for(const auto& service : m_services) {
        services.push_back(service->details());
    }

    QByteArray data;
    QDataStream stream{&data, QIODevice::WriteOnly};

    stream << services;

    m_settings->set<Settings::Scrobbler::ServicesData>(data);
}

void Scrobbler::restoreServices()
{
    QByteArray data = m_settings->value<Settings::Scrobbler::ServicesData>();
    QDataStream stream{&data, QIODevice::ReadOnly};

    QList<ServiceDetails> services;

    stream >> services;

    for(const auto& details : std::as_const(services)) {
        if(details.isCustom()) {
            addCustomService(details, false);
        }
        else {
            auto serviceIt = std::ranges::find_if(
                m_services, [&details](const auto& service) { return service->name() == details.name; });
            if(serviceIt != m_services.cend()) {
                serviceIt->get()->updateDetails(details);
            }
        }
    }
}
} // namespace Fooyin::Scrobbler

#include "moc_scrobbler.cpp"
