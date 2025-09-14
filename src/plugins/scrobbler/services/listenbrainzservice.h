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
    using ScrobblerService::ScrobblerService;

    [[nodiscard]] QUrl url() const override;
    [[nodiscard]] bool requiresAuthentication() const override;

    void saveSession() override;
    void loadSession() override;
    void deleteSession() override;

    void testApi() override;
    void updateNowPlaying() override;
    void submit() override;

    [[nodiscard]] QString tokenSetting() const override;
    [[nodiscard]] QUrl tokenUrl() const override;

private:
    QNetworkReply* createRequest(RequestType type, const QUrl& url, const QJsonDocument& json);
    ReplyResult getJsonFromReply(QNetworkReply* reply, QJsonObject* obj, QString* errorDesc) override;
    [[nodiscard]] QJsonObject getTrackMetadata(const Metadata& metadata) const;

    void testFinished(QNetworkReply* reply);
    void updateNowPlayingFinished(QNetworkReply* reply);
    void scrobbleFinished(QNetworkReply* reply, const CacheItemList& items);

    [[nodiscard]] QString userToken() const;
};
} // namespace Fooyin::Scrobbler
