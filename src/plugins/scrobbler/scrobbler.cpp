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

#include "lastfmservice.h"

#include <core/player/playercontroller.h>

namespace Fooyin::Scrobbler {
Scrobbler::Scrobbler(PlayerController* playerController, std::shared_ptr<NetworkAccessManager> network,
                     SettingsManager* settings)
    : m_playerController{playerController}
    , m_network{std::move(network)}
    , m_settings{settings}
{
    m_services.emplace_back(std::make_unique<LastFmService>(m_network.get(), m_settings));

    for(auto& service : m_services) {
        service->loadSession();
        QObject::connect(m_playerController, &PlayerController::currentTrackChanged, service.get(),
                         &ScrobblerService::updateNowPlaying);
        QObject::connect(m_playerController, &PlayerController::trackPlayed, service.get(),
                         &ScrobblerService::scrobble);
    }
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

void Scrobbler::saveCache()
{
    for(const auto& service : m_services) {
        service->saveCache();
    }
}
} // namespace Fooyin::Scrobbler
