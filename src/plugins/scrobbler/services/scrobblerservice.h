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
#include "servicedetails.h"

#include <core/scripting/scriptparser.h>
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

enum class RequestType : uint8_t
{
    Get = 0,
    Post
};

class ScrobblerService : public QObject
{
    Q_OBJECT

public:
    ScrobblerService(ServiceDetails details, NetworkAccessManager* network, SettingsManager* settings,
                     QObject* parent = nullptr);
    ~ScrobblerService() override;

    [[nodiscard]] bool isEnabled() const;
    [[nodiscard]] QString name() const;
    [[nodiscard]] virtual QUrl url() const;
    [[nodiscard]] virtual QUrl authUrl() const;
    [[nodiscard]] virtual QString username() const;
    [[nodiscard]] virtual bool requiresAuthentication() const;
    [[nodiscard]] virtual bool isAuthenticated() const;

    [[nodiscard]] bool isCustom() const;
    [[nodiscard]] ServiceDetails details() const;
    void updateDetails(const ServiceDetails& service);

    void initialise();
    virtual void authenticate();
    virtual void saveSession();
    virtual void loadSession();
    virtual void deleteSession();
    virtual void logout();
    void saveCache();

    void updateNowPlaying(const Track& track);
    void scrobble(const Track& track);

    virtual void testApi()          = 0;
    virtual void updateNowPlaying() = 0;
    virtual void submit()           = 0;

    [[nodiscard]] virtual QString tokenSetting() const;
    [[nodiscard]] virtual QUrl tokenUrl() const;

signals:
    void testApiFinished(bool success, const QString& error = {});
    void authenticationFinished(bool success, const QString& error = {});

protected:
    virtual void setupAuthQuery(ScrobblerAuthSession* session, QUrlQuery& query);
    virtual void requestAuth(const QString& token);
    virtual void authFinished(QNetworkReply* reply);

    [[nodiscard]] Track currentTrack() const;
    [[nodiscard]] NetworkAccessManager* network() const;
    [[nodiscard]] ScrobblerAuthSession* authSession() const;
    [[nodiscard]] ScrobblerCache* cache() const;
    [[nodiscard]] SettingsManager* settings() const;

    ServiceDetails& detailsRef();

    QNetworkReply* addReply(QNetworkReply* reply);
    bool removeReply(QNetworkReply* reply);

    bool allowedByFilter(const Track& track);

    enum class ReplyResult : uint8_t
    {
        Success = 0,
        ServerError,
        ApiError,
    };
    virtual ReplyResult getJsonFromReply(QNetworkReply* reply, QJsonObject* obj, QString* errorDesc) = 0;
    bool extractJsonObj(const QByteArray& data, QJsonObject* obj, QString* errorDesc);

    void handleTestError(const char* error);
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

    ScriptParser m_scriptParser;

    ServiceDetails m_details;

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
