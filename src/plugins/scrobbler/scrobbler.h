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

#include "services/scrobblerservice.h"
#include "services/servicedetails.h"

#include <memory>
#include <vector>

class QString;

namespace Fooyin {
class NetworkAccessManager;
class PlayerController;
class SettingsManager;

namespace Scrobbler {
struct ServiceDetails;

class Scrobbler : public QObject
{
    Q_OBJECT

public:
    Scrobbler(PlayerController* playerController, std::shared_ptr<NetworkAccessManager> network,
              SettingsManager* settings);
    ~Scrobbler() override;

    [[nodiscard]] std::vector<ScrobblerService*> services() const;
    [[nodiscard]] ScrobblerService* service(const QString& name) const;

    void updateNowPlaying(const Track& track);
    void scrobble(const Track& track);

    std::unique_ptr<ScrobblerService> createCustomService(const ServiceDetails& details);
    ScrobblerService* addCustomService(const ServiceDetails& details, bool init = true);
    bool removeCustomService(ScrobblerService* service);

    void saveCache();

private:
    void addDefaultServices();
    void saveServices();
    void restoreServices();

    PlayerController* m_playerController;
    std::shared_ptr<NetworkAccessManager> m_network;
    SettingsManager* m_settings;

    std::vector<std::unique_ptr<ScrobblerService>> m_services;
};
} // namespace Scrobbler
} // namespace Fooyin
