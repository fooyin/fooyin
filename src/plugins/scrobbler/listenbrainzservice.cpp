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

#include "listenbrainzservice.h"

#include "scrobblerauthsession.h"
#include "scrobblersettings.h"

#include <core/coresettings.h>
#include <core/network/networkaccessmanager.h>
#include <utils/settings/settingsmanager.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QTimerEvent>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

constexpr auto ApiUrl             = "https://api.listenbrainz.org/";
constexpr auto AuthUrl            = "https://musicbrainz.org/oauth2/authorize/";
constexpr auto AuthAccessTokenUrl = "https://musicbrainz.org/oauth2/token";
constexpr auto ClientID           = "UDV4ZUkxY2lPRS1xQjNFSHlwOGc5T1p6dzA5cWJnNlM=";
constexpr auto ClientSecret       = "Sm5LMnlTdnl0M1hGanRsLUFRSzBucFQwY1I0NjBfU2U=";

constexpr auto MaxScrobblesPerRequest = 10;

namespace Fooyin::Scrobbler {
ListenBrainzService::ListenBrainzService(NetworkAccessManager* network, SettingsManager* settings, QObject* parent)
    : ScrobblerService{network, settings, parent}
{ }

QString ListenBrainzService::name() const
{
    return u"ListenBrainz"_s;
}

QUrl ListenBrainzService::authUrl() const
{
    return QString::fromLatin1(AuthUrl);
}

bool ListenBrainzService::isAuthenticated() const
{
    return !m_accessToken.isEmpty();
}

void ListenBrainzService::authenticate()
{
    ScrobblerService::authenticate();
}

void ListenBrainzService::loadSession()
{
    FySettings settings;
    settings.beginGroup(name());
    m_accessToken  = settings.value("AccessToken").toString();
    m_tokenType    = settings.value("TokenType").toString();
    m_expiresIn    = settings.value("ExpiresIn").toInt();
    m_refreshToken = settings.value("RefreshToken").toString();
    m_loginTime    = settings.value("LoginTime").toULongLong();
    m_userToken    = settings.value("UserToken").toString();
    settings.endGroup();
}

void ListenBrainzService::logout()
{
    m_accessToken.clear();
    m_tokenType.clear();
    m_refreshToken.clear();
    m_expiresIn = -1;
    m_loginTime = 0;

    FySettings settings;
    settings.beginGroup(name());
    settings.remove("AccessToken");
    settings.remove("TokenType");
    settings.remove("ExpiresIn");
    settings.remove("RefreshToken");
    settings.remove("LoginTime");
    settings.endGroup();
}

void ListenBrainzService::updateNowPlaying()
{
    QJsonObject metaObj;
    metaObj.insert(u"track_metadata"_s, getTrackMetadata(Metadata{currentTrack()}));

    QJsonArray payload;
    payload.append(metaObj);

    QJsonObject object;
    object.insert(u"listen_type"_s, u"playing_now"_s);
    object.insert(u"payload"_s, payload);

    const QJsonDocument doc{object};
    const QUrl url{u"%1/1/submit-listens"_s.arg(QLatin1String{ApiUrl})};

    QNetworkReply* reply = createRequest(url, doc);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { updateNowPlayingFinished(reply); });
}

void ListenBrainzService::submit()
{
    if(!isAuthenticated()) {
        return;
    }

    qCDebug(SCROBBLER) << "Submitting scrobbles (%1)"_L1.arg(name());

    const CacheItemList items = cache()->items();
    CacheItemList sentItems;

    QJsonArray array;
    for(const auto& item : items) {
        if(item->submitted) {
            continue;
        }
        if(item->hasError && !sentItems.empty()) {
            break;
        }
        item->submitted = true;
        sentItems.emplace_back(item);

        QJsonObject obj;
        obj.insert(u"listened_at"_s, QString::number(item->timestamp));
        obj.insert(u"track_metadata"_s, getTrackMetadata(item->metadata));
        array.append(obj);

        if(sentItems.size() >= MaxScrobblesPerRequest || item->hasError) {
            break;
        }
    }

    if(sentItems.empty()) {
        return;
    }

    setSubmitted(true);

    QJsonObject object;
    object.insert(u"listen_type"_s, u"import"_s);
    object.insert(u"payload"_s, array);

    const QJsonDocument doc{object};
    const QUrl url{u"%1/1/submit-listens"_s.arg(QLatin1String{ApiUrl})};
    QNetworkReply* reply = createRequest(url, doc);
    QObject::connect(reply, &QNetworkReply::finished, this,
                     [this, reply, sentItems]() { scrobbleFinished(reply, sentItems); });
}

QString ListenBrainzService::tokenSetting() const
{
    return u"%1/UserToken"_s.arg(name());
}

QUrl ListenBrainzService::tokenUrl() const
{
    return u"https://listenbrainz.org/profile/"_s;
}

void ListenBrainzService::setupAuthQuery(ScrobblerAuthSession* session, QUrlQuery& query)
{
    query.addQueryItem(u"response_type"_s, u"code"_s);
    query.addQueryItem(u"client_id"_s, QString::fromLatin1(QByteArray::fromBase64(ClientID)));
    query.addQueryItem(u"redirect_uri"_s, session->callbackUrl());
    query.addQueryItem(u"scope"_s, u"profile;email;tag;rating;collection;submit_isrc;submit_barcode"_s);

    session->setAuthTokenName(u"code"_s);
}

void ListenBrainzService::requestAuth(const QString& token)
{
    m_loginTimer.stop();

    std::map<QString, QString> queryParams{
        {u"client_id"_s, QString::fromLatin1(QByteArray::fromBase64(ClientID))},
        {u"client_secret"_s, QString::fromLatin1(QByteArray::fromBase64(ClientSecret))}};

    if(!token.isEmpty() && authSession() && !authSession()->callbackUrl().isEmpty()) {
        queryParams.emplace(u"grant_type"_s, u"authorization_code"_s);
        queryParams.emplace(u"code"_s, token);
        queryParams.emplace(u"redirect_uri"_s, authSession()->callbackUrl());
    }
    else if(!m_refreshToken.isEmpty()) {
        queryParams.emplace(u"grant_type"_s, u"refresh_token"_s);
        queryParams.emplace(u"refresh_token"_s, m_refreshToken);
    }
    else {
        return;
    }

    QUrlQuery urlQuery;
    for(const auto& [key, value] : queryParams) {
        urlQuery.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(key)),
                              QString::fromLatin1(QUrl::toPercentEncoding(value)));
    }

    const QUrl sessionUrl{QString::fromLatin1(AuthAccessTokenUrl)};

    QNetworkRequest req{sessionUrl};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_s);

    const QByteArray query = urlQuery.toString(QUrl::FullyEncoded).toUtf8();
    QNetworkReply* reply   = addReply(network()->post(req, query));
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { authFinished(reply); });
}

void ListenBrainzService::authFinished(QNetworkReply* reply)
{
    if(!removeReply(reply)) {
        return;
    }

    QJsonObject obj;
    QString error;
    if(getJsonFromReply(reply, &obj, &error) != ReplyResult::Success) {
        handleAuthError(error.toUtf8().constData());
        return;
    }

    if(!obj.contains("access_token"_L1)) {
        handleAuthError("Json reply from server is missing access_token");
        return;
    }

    if(!obj.contains("token_type"_L1)) {
        handleAuthError("Json reply from server is missing token_type");
        return;
    }

    if(!obj.contains("expires_in"_L1)) {
        handleAuthError("Json reply from server is missing expires_in");
        return;
    }

    m_accessToken = obj.value("access_token"_L1).toString();
    m_tokenType   = obj.value("token_type"_L1).toString();
    m_expiresIn   = obj.value("expires_in"_L1).toInt();
    if(obj.contains("refresh_token"_L1)) {
        m_refreshToken = obj.value("refresh_token"_L1).toString();
    }
    m_loginTime = QDateTime::currentSecsSinceEpoch();

    FySettings settings;
    settings.beginGroup(name());
    settings.setValue("AccessToken", m_accessToken);
    settings.setValue("TokenType", m_tokenType);
    settings.setValue("ExpiresIn", m_expiresIn);
    settings.setValue("RefreshToken", m_refreshToken);
    settings.setValue("LoginTime", m_loginTime);
    settings.endGroup();

    if(m_expiresIn > 0) {
        m_loginTimer.start(static_cast<int>(m_expiresIn * 1000), this);
    }

    emit authenticationFinished(true);
    cleanupAuth();
}

QNetworkReply* ListenBrainzService::createRequest(const QUrl& url, const QJsonDocument& json)
{
    QNetworkRequest req{url};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_s);
    req.setRawHeader("Authorization", u"Token %1"_s.arg(m_userToken).toUtf8());

    return addReply(network()->post(req, json.toJson()));
}

ScrobblerService::ReplyResult ListenBrainzService::getJsonFromReply(QNetworkReply* reply, QJsonObject* obj,
                                                                    QString* errorDesc)
{
    ReplyResult replyResult{ReplyResult::ServerError};

    if(reply->error() == QNetworkReply::NoError) {
        if(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
            replyResult = ReplyResult::Success;
        }
        else {
            *errorDesc
                = u"Received HTTP code %1"_s.arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        }
    }
    else {
        *errorDesc = u"%1 (%2)"_s.arg(reply->errorString()).arg(reply->error());
    }

    if(reply->error() == QNetworkReply::NoError || reply->error() >= 200) {
        const QByteArray data = reply->readAll();

        if(!data.isEmpty() && extractJsonObj(data, obj, errorDesc)) {
            if(obj->contains("error"_L1) && obj->contains("error_description"_L1)) {
                *errorDesc  = obj->value("error_description"_L1).toString();
                replyResult = ReplyResult::ApiError;
            }
            else if(obj->contains("code"_L1) && obj->contains("error"_L1)) {
                *errorDesc  = u"%1 (%2)"_s.arg(obj->value("error"_L1).toString()).arg(obj->value("code"_L1).toInt());
                replyResult = ReplyResult::ApiError;
            }

            const auto error = reply->error();
            if(error == QNetworkReply::ContentAccessDenied || error == QNetworkReply::AuthenticationRequiredError
               || error == QNetworkReply::ContentOperationNotPermittedError) {
                logout();
            }
        }
    }

    return replyResult;
}

QJsonObject ListenBrainzService::getTrackMetadata(const Metadata& metadata) const
{
    QJsonObject metaObj;

    if(settings()->value<Settings::Scrobbler::PreferAlbumArtist>() && !metadata.albumArtist.isEmpty()) {
        metaObj.insert(u"artist_name"_s, metadata.albumArtist);
    }
    else {
        metaObj.insert(u"artist_name"_s, metadata.artist);
    }

    if(!metadata.album.isEmpty()) {
        metaObj.insert(u"release_name"_s, metadata.album);
    }

    metaObj.insert(u"track_name"_s, metadata.title);

    QJsonObject infoObj;

    if(metadata.duration > 0) {
        infoObj.insert(u"duration_ms"_s, QString::number(metadata.duration * 1000));
    }
    if(!metadata.trackNum.isEmpty()) {
        infoObj.insert(u"tracknumber"_s, metadata.trackNum);
    }
    if(!metadata.musicBrainzId.isEmpty()) {
        infoObj.insert(u"track_mbid"_s, metadata.musicBrainzId);
    }

    infoObj.insert(u"media_player"_s, QCoreApplication::applicationName());
    infoObj.insert(u"media_player_version"_s, QCoreApplication::applicationVersion());
    infoObj.insert(u"submission_client"_s, QCoreApplication::applicationName());
    infoObj.insert(u"submission_client_version"_s, QCoreApplication::applicationVersion());

    metaObj.insert(u"additional_info"_s, infoObj);

    return metaObj;
}

void ListenBrainzService::updateNowPlayingFinished(QNetworkReply* reply)
{
    if(!removeReply(reply)) {
        return;
    }

    QJsonObject obj;
    QString errorStr;
    if(getJsonFromReply(reply, &obj, &errorStr) != ReplyResult::Success) {
        qCWarning(SCROBBLER) << errorStr;
        return;
    }

    if(!obj.contains("status"_L1)) {
        qCWarning(SCROBBLER) << "Json reply from server is missing status";
        return;
    }

    const QString status = obj.value("status"_L1).toString();
    if(status.compare("ok"_L1, Qt::CaseInsensitive) != 0) {
        qCWarning(SCROBBLER) << "Error on receiving status for now playing:" << status;
    }
}

void ListenBrainzService::scrobbleFinished(QNetworkReply* reply, const CacheItemList& items)
{
    if(!removeReply(reply)) {
        return;
    }
    setSubmitted(false);

    QJsonObject obj;
    QString errorStr;
    if(getJsonFromReply(reply, &obj, &errorStr) == ReplyResult::Success) {
        cache()->flush(items);
        setSubmitError(false);
    }
    else {
        setSubmitError(true);
        qCWarning(SCROBBLER) << "Unable to scrobble:" << errorStr;
        std::ranges::for_each(items, [](const auto& item) {
            item->submitted = false;
            item->hasError  = true;
        });
    }

    doDelayedSubmit();
}

void ListenBrainzService::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_loginTimer.timerId()) {
        requestAuth({});
    }
    ScrobblerService::timerEvent(event);
}
} // namespace Fooyin::Scrobbler
