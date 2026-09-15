/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include "lovedcache.h"
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

#include <optional>

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

struct RemoteTrackStats
{
    Track track;
    std::optional<bool> loved;
    std::optional<int> playCount;
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
    [[nodiscard]] virtual bool supportsLoved() const;
    [[nodiscard]] virtual bool supportsTrackStatsSync() const;

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
    void resumePendingSubmissions();

    void restartScrobbleSession(const Track& track);
    void updateNowPlaying(const Track& track);
    void refreshNowPlaying();
    void scrobble(const Track& track);
    void updateLoved(const Track& track);
    [[nodiscard]] bool hasPendingLoved(const Track& track);
    virtual void fetchTrackStats(const Track& track);

    virtual void testApi()          = 0;
    virtual void updateNowPlaying() = 0;
    virtual void submit()           = 0;

    [[nodiscard]] virtual QString tokenSetting() const;
    [[nodiscard]] virtual QUrl tokenUrl() const;

Q_SIGNALS:
    void testApiFinished(bool success, const QString& error = {});
    void authenticationFinished(bool success, const QString& error = {});
    void trackStatsFetched(const Fooyin::Scrobbler::RemoteTrackStats& stats);

protected:
    virtual void setupAuthQuery(ScrobblerAuthSession* session, QUrlQuery& query);
    virtual void requestAuth(const QString& token);
    virtual void authFinished(QNetworkReply* reply);

    [[nodiscard]] Track currentTrack() const;
    [[nodiscard]] NetworkAccessManager* network() const;
    [[nodiscard]] ScrobblerAuthSession* authSession() const;
    [[nodiscard]] ScrobblerCache* cache() const;
    [[nodiscard]] LovedCache* lovedCache() const;
    [[nodiscard]] SettingsManager* settings() const;

    ServiceDetails& detailsRef();
    ScriptParser* scriptParser();

    QNetworkReply* addReply(QNetworkReply* reply);
    bool removeReply(QNetworkReply* reply);

    bool shouldUpdateNowPlaying(const Track& track);
    bool allowedByFilter(const Track& track);

    enum class ReplyResult : uint8_t
    {
        Success = 0,
        ServerError,
        ApiError,
    };
    virtual ReplyResult getJsonFromReply(QNetworkReply* reply, QJsonObject* obj, QString* errorDesc) = 0;
    bool extractJsonObj(const QByteArray& data, QJsonObject* obj, QString* errorDesc);

    enum class LovedUpdateResult : uint8_t
    {
        Success = 0,
        Retry,
        Discard,
    };
    virtual void submitLoved(const LovedItem& item);
    void lovedUpdateFinished(const LovedItem& item, LovedUpdateResult result);

    void handleTestError(const char* error);
    void handleAuthError(const char* error);
    void cleanupAuth();
    void deleteAll();

    void doDelayedSubmit(bool initial = false);
    void doDelayedLovedSubmit(bool initial = false);
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
    LovedCache* m_lovedCache;

    QBasicTimer m_submitTimer;
    QBasicTimer m_lovedSubmitTimer;
    bool m_submitError;
    bool m_lovedSubmitError;

    Track m_currentTrack;
    uint64_t m_timestamp;
    bool m_scrobbled;
    bool m_submitted;
    bool m_lovedSubmitted;
};
} // namespace Scrobbler
} // namespace Fooyin

Q_DECLARE_METATYPE(Fooyin::Scrobbler::RemoteTrackStats)
