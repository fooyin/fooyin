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

#include "scrobblerservice.h"

namespace Fooyin::Scrobbler {
class ScrobblerAuthSession;
class ScrobblerCache;

class ListenBrainzService : public ScrobblerService
{
public:
    ListenBrainzService(NetworkAccessManager* network, SettingsManager* settings, QObject* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QUrl authUrl() const override;
    [[nodiscard]] bool isAuthenticated() const override;

    void authenticate() override;
    void loadSession() override;
    void logout() override;

    void updateNowPlaying() override;
    void submit() override;

    [[nodiscard]] QString tokenSetting() const override;
    [[nodiscard]] QUrl tokenUrl() const override;

protected:
    void setupAuthQuery(ScrobblerAuthSession* session, QUrlQuery& query) override;
    void requestAuth(const QString& token) override;
    void authFinished(QNetworkReply* reply) override;

    void timerEvent(QTimerEvent* event) override;

private:
    QNetworkReply* createRequest(const QUrl& url, const QJsonDocument& json);
    ReplyResult getJsonFromReply(QNetworkReply* reply, QJsonObject* obj, QString* errorDesc) override;
    [[nodiscard]] QJsonObject getTrackMetadata(const Metadata& metadata) const;

    void updateNowPlayingFinished(QNetworkReply* reply);
    void scrobbleFinished(QNetworkReply* reply, const CacheItemList& items);

    QString m_userToken;
    QString m_accessToken;
    qint64 m_expiresIn;
    quint64 m_loginTime;
    QString m_tokenType;
    QString m_refreshToken;
    QBasicTimer m_loginTimer;
};
} // namespace Fooyin::Scrobbler
