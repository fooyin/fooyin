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

#include "scrobblercache.h"

#include <core/track.h>

#include <QBasicTimer>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariant>

class QNetworkReply;

namespace Fooyin {
class NetworkAccessManager;
class SettingsManager;

namespace Scrobbler {
class ScrobblerAuthSession;
class ScrobblerCache;

class ScrobblerService : public QObject
{
    Q_OBJECT

public:
    explicit ScrobblerService(NetworkAccessManager* network, SettingsManager* settings, QObject* parent = nullptr);
    ~ScrobblerService() override;

    [[nodiscard]] QString name() const;
    [[nodiscard]] bool isAuthenticated() const;
    [[nodiscard]] QString apiKey() const;
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
    virtual void setupAuthQuery(ScrobblerAuthSession* session, QUrlQuery& query) = 0;
    virtual void requestAuth(const QString& token);
    virtual void authFinished(QNetworkReply* reply);

    void setName(const QString& name);
    void setApiUrl(const QUrl& url);
    void setAuthUrl(const QUrl& url);
    void setApiKey(const QString& key);
    void setSecret(const QString& secret);
    void timerEvent(QTimerEvent* event) override;

private:
    void deleteAll();
    void cleanupAuth();

    void handleAuthError(const char* error);

    enum class ReplyResult : uint8_t
    {
        Success = 0,
        ServerError,
        ApiError,
    };

    ReplyResult getJsonFromReply(QNetworkReply* reply, QJsonObject* obj, QString* errorDesc);
    QNetworkReply* createRequest(const std::map<QString, QString>& params);

    void updateNowPlayingFinished(QNetworkReply* reply);
    void scrobbleFinished(QNetworkReply* reply, const CacheItemList& items);

    void doDelayedSubmit(bool initial = false);
    void submit();

    NetworkAccessManager* m_network;
    SettingsManager* m_settings;

    QString m_name;
    QUrl m_apiUrl;
    QUrl m_authUrl;
    QString m_apiKey;
    QString m_secret;

    ScrobblerAuthSession* m_authSession{nullptr};
    std::vector<QNetworkReply*> m_replies;
    ScrobblerCache* m_cache{nullptr};
    QString m_username;
    QString m_sessionKey;

    QBasicTimer m_submitTimer;
    bool m_submitError{false};

    Track m_currentTrack;
    uint64_t m_timestamp{0};
    bool m_scrobbled{false};
    bool m_submitted{false};
};
} // namespace Scrobbler
} // namespace Fooyin
