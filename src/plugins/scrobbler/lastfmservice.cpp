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

#include "lastfmservice.h"

#include "scrobblerauthsession.h"
#include "scrobblersettings.h"

#include <core/coresettings.h>
#include <core/network/networkaccessmanager.h>
#include <utils/settings/settingsmanager.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

constexpr auto ApiUrl    = "https://ws.audioscrobbler.com/2.0/";
constexpr auto ApiKey    = "YjJkNTdjOTc4YTIyYmUyNzljYzNiZTZkNjc2MjdmZWE=";
constexpr auto ApiSecret = "ODYzZDBiNWI0M2I2NmQ1MmVkOTU4NGFiOWJiZTc3MDY=";

constexpr auto MaxScrobblesPerRequest = 50;

namespace {
enum class ScrobbleError : uint8_t
{
    // Reference: https://www.last.fm/api/errorcodes
    None                   = 1,
    InvalidService         = 2,
    InvalidMethod          = 3,
    AuthenticationFailed   = 4,
    InvalidFormat          = 5,
    InvalidParameters      = 6,
    InvalidResource        = 7,
    OperationFailed        = 8,
    InvalidSessionKey      = 9,
    InvalidApiKey          = 10,
    ServiceOffline         = 11,
    SubscribersOnly        = 12,
    InvalidMethodSignature = 13,
    UnauthorisedToken      = 14,
    ItemUnavailable        = 15,
    TempUnavailable        = 16,
    LoginRequired          = 17,
    TrialExpired           = 18,
    ErrorDoesNotExist      = 19,
    NotEnoughContent       = 20,
    NotEnoughMembers       = 21,
    NotEnoughFans          = 22,
    NotEnoughNeighbours    = 23,
    NoPeakRadio            = 24,
    RadioNotFound          = 25,
    APIKeySuspended        = 26,
    Deprecated             = 27,
    RateLimitExceeded      = 29,
};
}

namespace Fooyin::Scrobbler {
LastFmService::LastFmService(NetworkAccessManager* network, SettingsManager* settings, QObject* parent)
    : ScrobblerService{network, settings, parent}
    , m_apiKey{QString::fromLatin1(QByteArray::fromBase64(ApiKey))}
    , m_secret{QString::fromLatin1(QByteArray::fromBase64(ApiSecret))}
{ }

QString LastFmService::name() const
{
    return QStringLiteral("LastFM");
}

QUrl LastFmService::authUrl() const
{
    return QStringLiteral("https://www.last.fm/api/auth/");
}

QString LastFmService::username() const
{
    return m_username;
}

bool LastFmService::isAuthenticated() const
{
    return !m_username.isEmpty() && !m_sessionKey.isEmpty();
}

void LastFmService::loadSession()
{
    FySettings settings;
    settings.beginGroup(name());
    m_username   = settings.value(QLatin1String{"Username"}).toString();
    m_sessionKey = settings.value(QLatin1String{"SessionKey"}).toString();
    settings.endGroup();
}

void LastFmService::logout()
{
    m_username.clear();
    m_sessionKey.clear();

    FySettings settings;
    settings.beginGroup(name());
    settings.remove(QLatin1String{"Username"});
    settings.remove(QLatin1String{"SessionKey"});
    settings.endGroup();
}

void LastFmService::updateNowPlaying()
{
    const bool preferAlbumArtist = settings()->value<Settings::Scrobbler::PreferAlbumArtist>();

    const Track track = currentTrack();

    std::map<QString, QString> params = {{QStringLiteral("method"), QStringLiteral("track.updateNowPlaying")},
                                         {QStringLiteral("artist"), preferAlbumArtist && !track.albumArtists().empty()
                                                                        ? track.albumArtists().join(u", ")
                                                                        : track.artists().join(u", ")},
                                         {QStringLiteral("track"), track.title()}};

    if(!track.album().isEmpty()) {
        params.emplace(QStringLiteral("album"), track.album());
    }

    if(!preferAlbumArtist && !track.albumArtist().isEmpty()) {
        params.emplace(QStringLiteral("albumArtist"), track.albumArtists().join(u','));
    }

    QNetworkReply* reply = createRequest(params);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { updateNowPlayingFinished(reply); });
}

void LastFmService::submit()
{
    if(!isAuthenticated()) {
        return;
    }

    qCDebug(SCROBBLER) << QLatin1String{"Submitting scrobbles (%1)"}.arg(name());

    std::map<QString, QString> params{{QStringLiteral("method"), QStringLiteral("track.scrobble")}};

    const bool preferAlbumArtist = settings()->value<Settings::Scrobbler::PreferAlbumArtist>();
    const CacheItemList items    = cache()->items();
    CacheItemList sentItems;

    for(int i{0}; const auto& item : items) {
        if(item->submitted) {
            continue;
        }
        item->submitted = true;
        sentItems.emplace_back(item);

        const Metadata& md = item->metadata;

        const auto artist = preferAlbumArtist && !md.albumArtist.isEmpty() ? md.albumArtist : item->metadata.artist;

        params.emplace(QStringLiteral("track[%1]").arg(i), md.title);
        params.emplace(QStringLiteral("artist[%1]").arg(i), artist);
        params.emplace(QStringLiteral("duration[%1]").arg(i), QString::number(md.duration));
        params.emplace(QStringLiteral("timestamp[%1]").arg(i), QString::number(item->timestamp));

        if(!md.album.isEmpty()) {
            params.emplace(QStringLiteral("album[%1]").arg(i), md.album);
        }
        if(!preferAlbumArtist && !md.albumArtist.isEmpty()) {
            params.emplace(QStringLiteral("albumArtist[%1]").arg(i), md.albumArtist);
        }
        if(!md.trackNum.isEmpty()) {
            params.emplace(QStringLiteral("trackNumber[%1]").arg(i), md.trackNum);
        }
        if(sentItems.size() >= MaxScrobblesPerRequest) {
            break;
        }
        ++i;
    }

    if(sentItems.empty()) {
        return;
    }

    setSubmitted(true);

    QNetworkReply* reply = createRequest(params);
    QObject::connect(reply, &QNetworkReply::finished, this,
                     [this, reply, sentItems]() { scrobbleFinished(reply, sentItems); });
}

void LastFmService::setupAuthQuery(ScrobblerAuthSession* session, QUrlQuery& query)
{
    query.addQueryItem(QStringLiteral("api_key"), m_apiKey);
    query.addQueryItem(QStringLiteral("cb"), session->callbackUrl());
}

void LastFmService::requestAuth(const QString& token)
{
    QUrl url{QString::fromLatin1(ApiUrl)};

    QUrlQuery urlQuery;
    urlQuery.addQueryItem(QStringLiteral("api_key"), m_apiKey);
    urlQuery.addQueryItem(QStringLiteral("method"), QStringLiteral("auth.getSession"));
    urlQuery.addQueryItem(QStringLiteral("token"), token);

    QString data;
    const auto items = urlQuery.queryItems();
    for(const auto& [key, value] : items) {
        data += key + value;
    }
    data += m_secret;

    const QByteArray digest = QCryptographicHash::hash(data.toUtf8(), QCryptographicHash::Md5);
    const QString signature = QString::fromLatin1(digest.toHex()).rightJustified(32, u'0').toLower();

    urlQuery.addQueryItem(QStringLiteral("api_sig"), signature);
    urlQuery.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(QStringLiteral("format"))),
                          QString::fromLatin1(QUrl::toPercentEncoding(QStringLiteral("json"))));
    url.setQuery(urlQuery);

    QNetworkRequest req{url};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = addReply(network()->get(req));
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { authFinished(reply); });
}

void LastFmService::authFinished(QNetworkReply* reply)
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

    if(!obj.contains(u"session")) {
        handleAuthError("Json reply from server is missing session");
        return;
    }

    const QJsonValue session = obj.value(u"session");
    if(!session.isObject()) {
        handleAuthError("Json session is not an object");
        return;
    }

    obj = session.toObject();
    if(obj.isEmpty()) {
        handleAuthError("Json session object is empty");
        return;
    }

    if(!obj.contains(u"name") || !obj.contains(u"key")) {
        handleAuthError("Json session object is missing values");
        return;
    }

    m_username   = obj.value(u"name").toString();
    m_sessionKey = obj.value(u"key").toString();

    FySettings settings;
    settings.beginGroup(name());
    settings.setValue(QLatin1String{"Username"}, m_username);
    settings.setValue(QLatin1String{"SessionKey"}, m_sessionKey);
    settings.endGroup();

    emit authenticationFinished(true);
    cleanupAuth();
}

ScrobblerService::ReplyResult LastFmService::getJsonFromReply(QNetworkReply* reply, QJsonObject* obj,
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
        int errorCode{0};

        if(!data.isEmpty() && extractJsonObj(data, obj, errorDesc) && obj->contains(u"error")
           && obj->contains(u"message")) {
            errorCode   = obj->value(u"error").toInt();
            *errorDesc  = QStringLiteral("%1 (%2)").arg(obj->value(u"message").toString()).arg(errorCode);
            replyResult = ReplyResult::ApiError;
        }

        const auto lastfmError = static_cast<ScrobbleError>(errorCode);
        if(reply->error() == QNetworkReply::AuthenticationRequiredError
           || lastfmError == ScrobbleError::InvalidSessionKey || lastfmError == ScrobbleError::UnauthorisedToken
           || lastfmError == ScrobbleError::LoginRequired || lastfmError == ScrobbleError::APIKeySuspended) {
            logout();
        }
    }

    return replyResult;
}

QNetworkReply* LastFmService::createRequest(const std::map<QString, QString>& params)
{
    std::map<QString, QString> queryParams{{QStringLiteral("api_key"), m_apiKey},
                                           {QStringLiteral("sk"), m_sessionKey},
                                           {QStringLiteral("lang"), QLocale{}.name().left(2).toLower()}};
    queryParams.insert(params.cbegin(), params.cend());

    QUrlQuery queryUrl;
    QString data;

    for(const auto& [key, value] : std::as_const(queryParams)) {
        queryUrl.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(key)),
                              QString::fromLatin1(QUrl::toPercentEncoding(value)));
        data += key + value;
    }
    data += m_secret;

    const QByteArray digest = QCryptographicHash::hash(data.toUtf8(), QCryptographicHash::Md5);
    const QString signature = QString::fromLatin1(digest.toHex()).rightJustified(32, u'0').toLower();

    queryUrl.addQueryItem(QStringLiteral("api_sig"), QString::fromLatin1(QUrl::toPercentEncoding(signature)));
    queryUrl.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));

    const QUrl url{QString::fromLatin1(ApiUrl)};
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));

    const QByteArray query = queryUrl.toString(QUrl::FullyEncoded).toUtf8();

    QNetworkReply* reply = addReply(network()->post(req, query));
    qCDebug(SCROBBLER) << "Sending request" << queryUrl.toString(QUrl::FullyDecoded);

    return reply;
}

void LastFmService::updateNowPlayingFinished(QNetworkReply* reply)
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

    if(!obj.contains(u"nowplaying")) {
        qCWarning(SCROBBLER) << "Json reply from server is missing nowplaying";
        return;
    }
}

void LastFmService::scrobbleFinished(QNetworkReply* reply, const CacheItemList& items)
{
    if(!removeReply(reply)) {
        return;
    }
    setSubmitted(false);

    QJsonObject obj;
    QString errorStr;
    if(getJsonFromReply(reply, &obj, &errorStr) != ReplyResult::Success) {
        setSubmitError(true);
        qCWarning(SCROBBLER) << errorStr;
        std::ranges::for_each(items, [](const auto& item) { item->submitted = false; });
        doDelayedSubmit();
        return;
    }

    cache()->flush(items);
    setSubmitError(false);

    const auto handleError = [this](const char* message, const bool submit = false) {
        qCWarning(SCROBBLER) << message;
        if(submit) {
            doDelayedSubmit();
        }
    };

    if(!obj.contains(u"scrobbles")) {
        handleError("Reply from server is missing scrobbles", true);
        return;
    }

    const QJsonValue scrobblesVal = obj.value(u"scrobbles");
    if(!scrobblesVal.isObject()) {
        handleError("Scrobbles is not an object", true);
        return;
    }

    obj = scrobblesVal.toObject();
    if(obj.isEmpty()) {
        handleError("Scrobbles object is empty", true);
        return;
    }

    if(!obj.contains(u"@attr") || !obj.contains(u"scrobble")) {
        handleError("Scrobbles object is missing values", true);
        return;
    }

    const QJsonValue attr = obj.value(u"@attr");
    if(!attr.isObject()) {
        handleError("Scrobbles attr is not an object", true);
        return;
    }

    const QJsonObject attrObj = attr.toObject();
    if(attrObj.isEmpty()) {
        handleError("Scrobbles attr is empty", true);
        return;
    }

    if(!attrObj.contains(u"accepted") || !attrObj.contains(u"ignored")) {
        handleError("Scrobbles attr is missing values", true);
        return;
    }

    const int accepted = attrObj.value(u"accepted").toInt();
    const int ignored  = attrObj.value(u"ignored").toInt();

    qCDebug(SCROBBLER) << "Scrobbles accepted:" << accepted;
    qCDebug(SCROBBLER) << "Scrobbles ignored:" << ignored;

    QJsonArray array;

    const QJsonValue scrobbleVal = obj.value(u"scrobble");
    if(scrobbleVal.isObject()) {
        const QJsonObject scrobbleObj = scrobbleVal.toObject();
        if(scrobbleObj.isEmpty()) {
            handleError("Scrobble object is empty", true);
            return;
        }
        array.append(scrobbleObj);
    }
    else if(scrobbleVal.isArray()) {
        array = scrobbleVal.toArray();
        if(array.isEmpty()) {
            handleError("Scrobble array is empty", true);
            return;
        }
    }
    else {
        handleError("Scrobble is not an object or array", true);
        return;
    }

    for(const auto& value : std::as_const(array)) {
        if(!value.isObject()) {
            handleError("Scrobble array value is not an object");
            continue;
        }

        const QJsonObject trackJson = value.toObject();
        if(trackJson.isEmpty()) {
            continue;
        }

        if(!trackJson.contains(u"artist") || !trackJson.contains(u"album") || !trackJson.contains(u"albumArtist")
           || !trackJson.contains(u"track") || !trackJson.contains(u"timestamp")
           || !trackJson.contains(u"ignoredMessage")) {
            handleError("Scrobble is missing values");
            continue;
        }

        const QJsonValue artist     = trackJson.value(u"artist");
        const QJsonValue album      = trackJson.value(u"album");
        const QJsonValue track      = trackJson.value(u"track");
        const QJsonValue ignoredMsg = trackJson.value(u"ignoredMessage");

        if(!artist.isObject() || !album.isObject() || !track.isObject() || !ignoredMsg.isObject()) {
            handleError("Scrobble values are not objects");
            continue;
        }

        const QJsonObject artistObj    = artist.toObject();
        const QJsonObject albumObj     = album.toObject();
        const QJsonObject trackObj     = track.toObject();
        const QJsonObject ignoreMsgObj = ignoredMsg.toObject();

        if(artistObj.isEmpty() || albumObj.isEmpty() || trackObj.isEmpty() || ignoreMsgObj.isEmpty()) {
            handleError("Scrobble values objects are empty");
            continue;
        }

        if(!artistObj.contains(u"#text") || !albumObj.contains(u"#text") || !trackObj.contains(u"#text")) {
            continue;
        }

        const QString trackText = trackObj.value(u"#text").toString();

        if(ignoreMsgObj.value(u"code").toBool()) {
            const QString ignoredMsgText = QStringLiteral(R"(Scrobble for "%1" ignored: %2)")
                                               .arg(trackText, ignoreMsgObj.value(u"#text").toString());
            handleError(ignoredMsgText.toUtf8().constData());
        }
        else {
            qCDebug(SCROBBLER) << "Scrobble accepted:" << trackText;
        }
    }

    doDelayedSubmit();
}
} // namespace Fooyin::Scrobbler
