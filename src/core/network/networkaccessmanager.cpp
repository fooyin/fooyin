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

#include <core/network/networkaccessmanager.h>

#include <utils/paths.h>
#include <utils/settings/settingsmanager.h>

#include <QLoggingCategory>
#include <QNetworkCacheMetaData>
#include <QNetworkDiskCache>
#include <QNetworkProxyFactory>

Q_LOGGING_CATEGORY(NETWORK, "fy.network")

namespace Fooyin {
class NetworkCache : public QNetworkDiskCache
{
    Q_OBJECT

public:
    explicit NetworkCache(QObject* parent = nullptr)
        : QNetworkDiskCache{parent}
    {
        setCacheDirectory(Utils::cachePath(QStringLiteral("networkcache")));
        setMaximumCacheSize(50 * 1024 * 1024);
    }
};

class ProxyFactory : public QNetworkProxyFactory
{
public:
    ProxyFactory()
    {
        const QStringList urls
            = {QString::fromLocal8Bit(qgetenv("HTTP_PROXY")), QString::fromLocal8Bit(qgetenv("http_proxy")),
               QString::fromLocal8Bit(qgetenv("ALL_PROXY")), QString::fromLocal8Bit(qgetenv("all_proxy"))};

        for(const QString& url : urls) {
            if(url.isEmpty()) {
                continue;
            }
            m_url = url;
            break;
        }

        if(!m_url.isEmpty()) {
            qCDebug(NETWORK) << "Using system proxy:" << m_url;
        }
    }

    void configure(NetworkAccessManager::Mode mode, const NetworkAccessManager::ProxyConfig& config)
    {
        m_mode   = mode;
        m_config = config;
    }

    QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery& /*query*/) override
    {
        QNetworkProxy proxy;

        switch(m_mode) {
            case(NetworkAccessManager::Mode::None):
                proxy.setType(QNetworkProxy::NoProxy);
                break;
            case(NetworkAccessManager::Mode::System): {
                if(m_url.isEmpty()) {
                    proxy.setType(QNetworkProxy::NoProxy);
                }
                else {
                    proxy.setHostName(m_url.host());
                    proxy.setPort(m_url.port());
                    proxy.setUser(m_url.userName());
                    proxy.setPassword(m_url.password());

                    if(m_url.scheme().startsWith(u"http")) {
                        proxy.setType(QNetworkProxy::HttpProxy);
                    }
                    else {
                        proxy.setType(QNetworkProxy::Socks5Proxy);
                    }
                    qCDebug(NETWORK) << "Using proxy:" << m_url;
                }
                break;
            }
            case(NetworkAccessManager::Mode::Manual): {
                proxy.setType(m_config.type);
                proxy.setHostName(m_config.host);
                proxy.setPort(m_config.port);

                if(m_config.useAuth) {
                    proxy.setUser(m_config.username);
                    proxy.setPassword(m_config.password);
                }
                break;
            }
        }

        return {proxy};
    }

private:
    NetworkAccessManager::Mode m_mode;
    NetworkAccessManager::ProxyConfig m_config;
    QUrl m_url;
};

class NetworkAccessManagerPrivate
{
public:
    explicit NetworkAccessManagerPrivate(NetworkAccessManager* self, SettingsManager* settings)
        : m_self{self}
        , m_settings{settings}
    {
        auto reconfigure = [this]() {
            auto* proxy = new ProxyFactory();
            proxy->configure(
                static_cast<NetworkAccessManager::Mode>(m_settings->value<Settings::Core::ProxyMode>()),
                m_settings->value<Settings::Core::ProxyConfig>().value<NetworkAccessManager::ProxyConfig>());
            m_self->setProxyFactory(proxy); // Takes ownership
        };

        m_settings->subscribe<Settings::Core::ProxyConfig>(m_self, reconfigure);

        reconfigure();
    }

    NetworkAccessManager* m_self;
    SettingsManager* m_settings;
};

NetworkAccessManager::NetworkAccessManager(SettingsManager* settings, QObject* parent)
    : QNetworkAccessManager{parent}
    , p{std::make_unique<NetworkAccessManagerPrivate>(this, settings)}
{
    setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    setCache(new NetworkCache(this));
}

NetworkAccessManager::~NetworkAccessManager() = default;
} // namespace Fooyin

#include "core/network/moc_networkaccessmanager.cpp"
#include "networkaccessmanager.moc"
