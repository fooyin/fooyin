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

#include "fycore_export.h"

#include <core/coresettings.h>

#include <QNetworkAccessManager>

namespace Fooyin {
class NetworkAccessManagerPrivate;
class SettingsManager;

class FYCORE_EXPORT NetworkAccessManager : public QNetworkAccessManager
{
    Q_OBJECT

public:
    enum class Mode : uint8_t
    {
        None = 0,
        System,
        Manual
    };

    struct ProxyConfig
    {
        QNetworkProxy::ProxyType type{QNetworkProxy::HttpProxy};
        QString host;
        int port{8080};
        bool useAuth{false};
        QString username;
        QString password;
    };

    explicit NetworkAccessManager(SettingsManager* settings, QObject* parent = nullptr);
    ~NetworkAccessManager() override;

private:
    std::unique_ptr<NetworkAccessManagerPrivate> p;
};
} // namespace Fooyin
