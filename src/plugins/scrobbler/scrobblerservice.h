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

#include <core/track.h>

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariant>

class QNetworkReply;

namespace Fooyin {
class NetworkAccessManager;
class SettingsManager;

namespace Scrobbler {
class ScrobblerServicePrivate;

class ScrobblerService : public QObject
{
    Q_OBJECT

public:
    explicit ScrobblerService(NetworkAccessManager* network, SettingsManager* settings, QObject* parent = nullptr);
    ~ScrobblerService() override;

    [[nodiscard]] QString name() const;
    [[nodiscard]] bool isAuthenticated() const;
    [[nodiscard]] QString username() const;

    virtual void authenticate();
    virtual void loadSession();
    virtual void logout();
    void saveCache();

    virtual void updateNowPlaying(const Track& track);
    virtual void scrobble(const Track& track);

signals:
    void authenticationFinished(bool success, const QString& error = {});

protected:
    void setName(const QString& name);
    void setApiUrl(const QUrl& url);
    void setAuthUrl(const QUrl& url);
    void setApiKey(const QString& key);
    void setSecret(const QString& secret);
    void timerEvent(QTimerEvent* event) override;

private:
    std::unique_ptr<ScrobblerServicePrivate> p;
};
} // namespace Scrobbler
} // namespace Fooyin
