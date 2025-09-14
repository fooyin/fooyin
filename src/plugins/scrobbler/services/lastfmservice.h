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
class LastFmService : public ScrobblerService
{
public:
    LastFmService(ServiceDetails service, NetworkAccessManager* network, SettingsManager* settings,
                  QObject* parent = nullptr);

    [[nodiscard]] QUrl url() const override;
    [[nodiscard]] QUrl authUrl() const override;
    [[nodiscard]] QString username() const override;
    [[nodiscard]] bool requiresAuthentication() const override;
    [[nodiscard]] bool isAuthenticated() const override;

    void saveSession() override;
    void loadSession() override;
    void deleteSession() override;
    void logout() override;

    void testApi() override;
    void updateNowPlaying() override;
    void submit() override;

protected:
    void setupAuthQuery(ScrobblerAuthSession* session, QUrlQuery& query) override;
    void requestAuth(const QString& token) override;
    void authFinished(QNetworkReply* reply) override;

    ReplyResult getJsonFromReply(QNetworkReply* reply, QJsonObject* obj, QString* errorDesc) override;

private:
    QNetworkReply* createRequest(const std::map<QString, QString>& params);
    void updateNowPlayingFinished(QNetworkReply* reply);
    void scrobbleFinished(QNetworkReply* reply, const CacheItemList& items);

    QString m_apiKey;
    QString m_secret;
    QString m_username;
    QString m_sessionKey;
};
} // namespace Fooyin::Scrobbler
