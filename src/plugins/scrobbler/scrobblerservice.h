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
#include <QLoggingCategory>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariant>

Q_DECLARE_LOGGING_CATEGORY(SCROBBLER)

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

    [[nodiscard]] virtual QString name() const = 0;
    [[nodiscard]] virtual QUrl authUrl() const = 0;
    [[nodiscard]] virtual QString username() const;
    [[nodiscard]] virtual bool isAuthenticated() const = 0;

    void initialise();
    virtual void authenticate();
    virtual void loadSession() = 0;
    virtual void logout()      = 0;
    void saveCache();

    void updateNowPlaying(const Track& track);
    void scrobble(const Track& track);

    virtual void updateNowPlaying() = 0;
    virtual void submit()           = 0;

    virtual QString tokenSetting() const;

signals:
    void authenticationFinished(bool success, const QString& error = {});

protected:
    virtual void setupAuthQuery(ScrobblerAuthSession* session, QUrlQuery& query) = 0;
    virtual void requestAuth(const QString& token)                               = 0;
    virtual void authFinished(QNetworkReply* reply)                              = 0;

    [[nodiscard]] Track currentTrack() const;
    [[nodiscard]] NetworkAccessManager* network() const;
    [[nodiscard]] ScrobblerAuthSession* authSession() const;
    [[nodiscard]] ScrobblerCache* cache() const;
    [[nodiscard]] SettingsManager* settings() const;

    QNetworkReply* addReply(QNetworkReply* reply);
    bool removeReply(QNetworkReply* reply);

    enum class ReplyResult : uint8_t
    {
        Success = 0,
        ServerError,
        ApiError,
    };
    virtual ReplyResult getJsonFromReply(QNetworkReply* reply, QJsonObject* obj, QString* errorDesc) = 0;
    bool extractJsonObj(const QByteArray& data, QJsonObject* obj, QString* errorDesc);

    void handleAuthError(const char* error);
    void cleanupAuth();
    void deleteAll();

    void doDelayedSubmit(bool initial = false);
    void setSubmitted(bool submitted);
    void setSubmitError(bool error);
    void setScrobbled(bool scrobbled);

    void timerEvent(QTimerEvent* event) override;

private:
    NetworkAccessManager* m_network;
    SettingsManager* m_settings;

    ScrobblerAuthSession* m_authSession;
    std::vector<QNetworkReply*> m_replies;
    ScrobblerCache* m_cache;

    QBasicTimer m_submitTimer;
    bool m_submitError;

    Track m_currentTrack;
    uint64_t m_timestamp;
    bool m_scrobbled;
    bool m_submitted;
};
} // namespace Scrobbler
} // namespace Fooyin
