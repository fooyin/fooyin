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
    return QStringLiteral("ListenBrainz");
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
    m_accessToken  = settings.value(QLatin1String{"AccessToken"}).toString();
    m_tokenType    = settings.value(QLatin1String{"TokenType"}).toString();
    m_expiresIn    = settings.value(QLatin1String{"ExpiresIn"}).toInt();
    m_refreshToken = settings.value(QLatin1String{"RefreshToken"}).toString();
    m_loginTime    = settings.value(QLatin1String{"LoginTime"}).toULongLong();
    m_userToken    = settings.value(QLatin1String{"UserToken"}).toString();
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
    settings.remove(QLatin1String{"AccessToken"});
    settings.remove(QLatin1String{"TokenType"});
    settings.remove(QLatin1String{"ExpiresIn"});
    settings.remove(QLatin1String{"RefreshToken"});
    settings.remove(QLatin1String{"LoginTime"});
    settings.endGroup();
}

void ListenBrainzService::updateNowPlaying()
{
    QJsonObject metaObj;
    metaObj.insert(QStringLiteral("track_metadata"), getTrackMetadata(Metadata{currentTrack()}));

    QJsonArray payload;
    payload.append(metaObj);

    QJsonObject object;
    object.insert(QStringLiteral("listen_type"), QStringLiteral("playing_now"));
    object.insert(QStringLiteral("payload"), payload);

    const QJsonDocument doc{object};
    const QUrl url{QStringLiteral("%1/1/submit-listens").arg(QLatin1String{ApiUrl})};

    QNetworkReply* reply = createRequest(url, doc);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { updateNowPlayingFinished(reply); });
}

void ListenBrainzService::submit()
{
    if(!isAuthenticated()) {
        return;
    }

    qCDebug(SCROBBLER) << QLatin1String{"Submitting scrobbles (%1)"}.arg(name());

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
        obj.insert(QStringLiteral("listened_at"), QString::number(item->timestamp));
        obj.insert(QStringLiteral("track_metadata"), getTrackMetadata(item->metadata));
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
    object.insert(QStringLiteral("listen_type"), QStringLiteral("import"));
    object.insert(QStringLiteral("payload"), array);

    const QJsonDocument doc{object};
    const QUrl url{QStringLiteral("%1/1/submit-listens").arg(QLatin1String{ApiUrl})};
    QNetworkReply* reply = createRequest(url, doc);
    QObject::connect(reply, &QNetworkReply::finished, this,
                     [this, reply, sentItems]() { scrobbleFinished(reply, sentItems); });
}

QString ListenBrainzService::tokenSetting() const
{
    return QStringLiteral("%1/UserToken").arg(name());
}

QUrl ListenBrainzService::tokenUrl() const
{
    return QStringLiteral("https://listenbrainz.org/profile/");
}

void ListenBrainzService::setupAuthQuery(ScrobblerAuthSession* session, QUrlQuery& query)
{
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("client_id"), QString::fromLatin1(QByteArray::fromBase64(ClientID)));
    query.addQueryItem(QStringLiteral("redirect_uri"), session->callbackUrl());
    query.addQueryItem(QStringLiteral("scope"),
                       QStringLiteral("profile;email;tag;rating;collection;submit_isrc;submit_barcode"));

    session->setAuthTokenName(QStringLiteral("code"));
}

void ListenBrainzService::requestAuth(const QString& token)
{
    m_loginTimer.stop();

    std::map<QString, QString> queryParams{
        {QStringLiteral("client_id"), QString::fromLatin1(QByteArray::fromBase64(ClientID))},
        {QStringLiteral("client_secret"), QString::fromLatin1(QByteArray::fromBase64(ClientSecret))}};

    if(!token.isEmpty() && authSession() && !authSession()->callbackUrl().isEmpty()) {
        queryParams.emplace(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
        queryParams.emplace(QStringLiteral("code"), token);
        queryParams.emplace(QStringLiteral("redirect_uri"), authSession()->callbackUrl());
    }
    else if(!m_refreshToken.isEmpty()) {
        queryParams.emplace(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
        queryParams.emplace(QStringLiteral("refresh_token"), m_refreshToken);
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
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));

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

    if(!obj.contains(u"access_token")) {
        handleAuthError("Json reply from server is missing access_token");
        return;
    }

    if(!obj.contains(u"token_type")) {
        handleAuthError("Json reply from server is missing token_type");
        return;
    }

    if(!obj.contains(u"expires_in")) {
        handleAuthError("Json reply from server is missing expires_in");
        return;
    }

    m_accessToken = obj.value(u"access_token").toString();
    m_tokenType   = obj.value(u"token_type").toString();
    m_expiresIn   = obj.value(u"expires_in").toInt();
    if(obj.contains(u"refresh_token")) {
        m_refreshToken = obj.value(u"refresh_token").toString();
    }
    m_loginTime = QDateTime::currentSecsSinceEpoch();

    FySettings settings;
    settings.beginGroup(name());
    settings.setValue(QLatin1String{"AccessToken"}, m_accessToken);
    settings.setValue(QLatin1String{"TokenType"}, m_tokenType);
    settings.setValue(QLatin1String{"ExpiresIn"}, m_expiresIn);
    settings.setValue(QLatin1String{"RefreshToken"}, m_refreshToken);
    settings.setValue(QLatin1String{"LoginTime"}, m_loginTime);
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
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Authorization", QStringLiteral("Token %1").arg(m_userToken).toUtf8());

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
            *errorDesc = QStringLiteral("Received HTTP code %1")
                             .arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        }
    }
    else {
        *errorDesc = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
    }

    if(reply->error() == QNetworkReply::NoError || reply->error() >= 200) {
        const QByteArray data = reply->readAll();

        if(!data.isEmpty() && extractJsonObj(data, obj, errorDesc)) {
            if(obj->contains(u"error") && obj->contains(u"error_description")) {
                *errorDesc  = obj->value(u"error_description").toString();
                replyResult = ReplyResult::ApiError;
            }
            else if(obj->contains(u"code") && obj->contains(u"error")) {
                *errorDesc
                    = QStringLiteral("%1 (%2)").arg(obj->value(u"error").toString()).arg(obj->value(u"code").toInt());
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
        metaObj.insert(QStringLiteral("artist_name"), metadata.albumArtist);
    }
    else {
        metaObj.insert(QStringLiteral("artist_name"), metadata.artist);
    }

    if(!metadata.album.isEmpty()) {
        metaObj.insert(QStringLiteral("release_name"), metadata.album);
    }

    metaObj.insert(QStringLiteral("track_name"), metadata.title);

    QJsonObject infoObj;

    if(metadata.duration > 0) {
        infoObj.insert(QStringLiteral("duration_ms"), QString::number(metadata.duration * 1000));
    }
    if(!metadata.trackNum.isEmpty()) {
        infoObj.insert(QStringLiteral("tracknumber"), metadata.trackNum);
    }
    if(!metadata.musicBrainzId.isEmpty()) {
        infoObj.insert(QStringLiteral("track_mbid"), metadata.musicBrainzId);
    }

    infoObj.insert(QStringLiteral("media_player"), QCoreApplication::applicationName());
    infoObj.insert(QStringLiteral("media_player_version"), QCoreApplication::applicationVersion());
    infoObj.insert(QStringLiteral("submission_client"), QCoreApplication::applicationName());
    infoObj.insert(QStringLiteral("submission_client_version"), QCoreApplication::applicationVersion());

    metaObj.insert(QStringLiteral("additional_info"), infoObj);

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

    if(!obj.contains(u"status")) {
        qCWarning(SCROBBLER) << "Json reply from server is missing status";
        return;
    }

    const QString status = obj.value(u"status").toString();
    if(status.compare(u"ok", Qt::CaseInsensitive) != 0) {
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
