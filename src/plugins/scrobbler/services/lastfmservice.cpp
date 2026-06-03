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

#include "lastfmservice.h"

#include "scrobblerauthsession.h"
#include "settings/scrobblersettings.h"

#include <core/coresettings.h>
#include <core/network/networkaccessmanager.h>
#include <utils/settings/settingsmanager.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

#include <ranges>

using namespace Qt::StringLiterals;

constexpr auto ApiUrl    = "https://ws.audioscrobbler.com/2.0/";
constexpr auto AuthUrl   = "https://www.last.fm/api/auth/";
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

QString previewReplyBody(const QByteArray& data)
{
    QString body = QString::fromUtf8(data).simplified();

    static constexpr auto MaxPreviewLength = 512;
    if(body.size() > MaxPreviewLength) {
        body = body.left(MaxPreviewLength - 3) + "..."_L1;
    }

    return body;
}

QString describeReply(QNetworkReply* reply)
{
    return u"HTTP %1, network error %2 (%3)"_s.arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt())
        .arg(reply->errorString())
        .arg(reply->error());
}

QString requestMethod(const std::map<QString, QString>& params)
{
    if(const auto methodIt = params.find(u"method"_s); methodIt != params.cend()) {
        return methodIt->second;
    }
    return {};
}

int requestItemCount(const std::map<QString, QString>& params)
{
    int count = 0;
    for(const auto& key : params | std::views::keys) {
        if(key.startsWith(u"track["_s)) {
            ++count;
        }
    }
    return count;
}

QByteArray encodeQueryKey(const QString& key)
{
    return QUrl::toPercentEncoding(key, "[]");
}

QByteArray encodeQueryValue(const QString& value)
{
    return QUrl::toPercentEncoding(value);
}
} // namespace

namespace Fooyin::Scrobbler {
LastFmService::LastFmService(ServiceDetails service, NetworkAccessManager* network, SettingsManager* settings,
                             QObject* parent)
    : ScrobblerService{std::move(service), network, settings, parent}
{ }

QString LastFmService::apiKey() const
{
    return QString::fromLatin1(QByteArray::fromBase64(ApiKey));
}

QString LastFmService::apiSecret() const
{
    return QString::fromLatin1(QByteArray::fromBase64(ApiSecret));
}

QUrl LastFmService::url() const
{
    return QString::fromLatin1(ApiUrl);
}

QUrl LastFmService::authUrl() const
{
    return QString::fromLatin1(AuthUrl);
}

QString LastFmService::username() const
{
    return m_username;
}

bool LastFmService::requiresAuthentication() const
{
    return true;
}

bool LastFmService::isAuthenticated() const
{
    return !m_username.isEmpty() && !m_sessionKey.isEmpty();
}

void LastFmService::saveSession()
{
    FySettings settings;
    settings.beginGroup(isCustom() ? u"Scrobbler-"_s + name() : name());

    settings.setValue("IsEnabled", details().isEnabled);
    settings.setValue("Username", m_username);
    settings.setValue("SessionKey", m_sessionKey);

    settings.endGroup();
}

void LastFmService::loadSession()
{
    FySettings settings;
    settings.beginGroup(isCustom() ? u"Scrobbler-"_s + name() : name());

    if(settings.contains("IsEnabled")) {
        detailsRef().isEnabled = settings.value("IsEnabled").toBool();
    }
    m_username   = settings.value("Username").toString();
    m_sessionKey = settings.value("SessionKey").toString();

    settings.endGroup();
}

void LastFmService::deleteSession()
{
    FySettings settings;
    settings.beginGroup(isCustom() ? u"Scrobbler-"_s + name() : name());

    settings.remove("Username");
    settings.remove("SessionKey");

    settings.endGroup();
}

void LastFmService::logout()
{
    m_username.clear();
    m_sessionKey.clear();

    deleteSession();
}

void LastFmService::testApi()
{
    // TODO
}

void LastFmService::updateNowPlaying()
{
    const Track track = currentTrack();

    std::map<QString, QString> params
        = {{u"method"_s, u"track.updateNowPlaying"_s},
           {u"artist"_s, scriptParser()->evaluate(settings()->value<Settings::Scrobbler::ArtistField>(), track)},
           {u"track"_s, scriptParser()->evaluate(settings()->value<Settings::Scrobbler::TitleField>(), track)}};

    const QString album = scriptParser()->evaluate(settings()->value<Settings::Scrobbler::AlbumField>(), track);
    if(!album.isEmpty()) {
        params.emplace(u"album"_s, album);
    }

    if(settings()->value<Settings::Scrobbler::SendAlbumArtist>()) {
        const QString albumArtist
            = scriptParser()->evaluate(settings()->value<Settings::Scrobbler::AlbumArtistField>(), track);
        if(!albumArtist.isEmpty()) {
            params.emplace(u"albumArtist"_s, albumArtist);
        }
    }

    QNetworkReply* reply = createRequest(params);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { updateNowPlayingFinished(reply); });
}

void LastFmService::submit()
{
    if(!isAuthenticated()) {
        return;
    }

    std::map<QString, QString> params{{u"method"_s, u"track.scrobble"_s}};

    const CacheItemList items = cache()->items();
    CacheItemList sentItems;

    for(int i{0}; const auto& item : items) {
        if(item->submitted) {
            continue;
        }
        if(item->hasError && !sentItems.empty()) {
            break;
        }
        item->submitted = true;
        sentItems.emplace_back(item);

        const Metadata& md = item->metadata;

        params.emplace(u"track[%1]"_s.arg(i), md.title);
        params.emplace(u"artist[%1]"_s.arg(i), md.artist);
        params.emplace(u"duration[%1]"_s.arg(i), QString::number(md.duration));
        params.emplace(u"timestamp[%1]"_s.arg(i), QString::number(item->timestamp));

        if(!md.album.isEmpty()) {
            params.emplace(u"album[%1]"_s.arg(i), md.album);
        }
        if(settings()->value<Settings::Scrobbler::SendAlbumArtist>() && !md.albumArtist.isEmpty()) {
            params.emplace(u"albumArtist[%1]"_s.arg(i), md.albumArtist);
        }
        if(!md.trackNum.isEmpty()) {
            params.emplace(u"trackNumber[%1]"_s.arg(i), md.trackNum);
        }
        if(sentItems.size() >= MaxScrobblesPerRequest || item->hasError) {
            break;
        }
        ++i;
    }

    if(sentItems.empty()) {
        return;
    }

    qCDebug(SCROBBLER) << "Preparing scrobble request for" << name() << "count" << sentItems.size() << "pending"
                       << items.size() << "timestamps" << sentItems.front()->timestamp << "to"
                       << sentItems.back()->timestamp;

    setSubmitted(true);

    QNetworkReply* reply = createRequest(params);
    QObject::connect(reply, &QNetworkReply::finished, this,
                     [this, reply, sentItems]() { scrobbleFinished(reply, sentItems); });
}

void LastFmService::setupAuthQuery(ScrobblerAuthSession* session, QUrlQuery& query)
{
    query.addQueryItem(u"api_key"_s, apiKey());
    query.addQueryItem(u"cb"_s, session->callbackUrl());
}

void LastFmService::requestAuth(const QString& token)
{
    QUrl reqUrl{url()};

    QUrlQuery urlQuery;
    urlQuery.addQueryItem(u"api_key"_s, apiKey());
    urlQuery.addQueryItem(u"method"_s, u"auth.getSession"_s);
    urlQuery.addQueryItem(u"token"_s, token);

    QString data;
    const auto items = urlQuery.queryItems();
    for(const auto& [key, value] : items) {
        data += key + value;
    }
    data += apiSecret();

    const QByteArray digest = QCryptographicHash::hash(data.toUtf8(), QCryptographicHash::Md5);
    const QString signature = QString::fromLatin1(digest.toHex()).rightJustified(32, u'0').toLower();

    urlQuery.addQueryItem(u"api_sig"_s, signature);
    urlQuery.addQueryItem(u"format"_s, u"json"_s);
    reqUrl.setQuery(urlQuery);

    QNetworkRequest req{reqUrl};
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

    if(!obj.contains("session"_L1)) {
        handleAuthError("Json reply from server is missing session");
        return;
    }

    const QJsonValue session = obj.value("session"_L1);
    if(!session.isObject()) {
        handleAuthError("Json session is not an object");
        return;
    }

    obj = session.toObject();
    if(obj.isEmpty()) {
        handleAuthError("Json session object is empty");
        return;
    }

    if(!obj.contains("name"_L1) || !obj.contains("key"_L1)) {
        handleAuthError("Json session object is missing values");
        return;
    }

    m_username   = obj.value("name"_L1).toString();
    m_sessionKey = obj.value("key"_L1).toString();

    saveSession();
    resumePendingSubmissions();

    Q_EMIT authenticationFinished(true);
    cleanupAuth();
}

ScrobblerService::ReplyResult LastFmService::getJsonFromReply(QNetworkReply* reply, QJsonObject* obj,
                                                              QString* errorDesc)
{
    ReplyResult replyResult{ReplyResult::ServerError};
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if(reply->error() == QNetworkReply::NoError) {
        if(httpStatus == 200) {
            replyResult = ReplyResult::Success;
        }
        else {
            *errorDesc = u"Received HTTP code %1"_s.arg(httpStatus);
        }
    }
    else {
        *errorDesc = u"%1 (%2)"_s.arg(reply->errorString()).arg(reply->error());
    }

    if(reply->error() == QNetworkReply::NoError || reply->error() >= 200) {
        const QByteArray data = reply->readAll();
        int errorCode{0};
        bool parsed{false};

        if(!data.isEmpty()) {
            parsed = extractJsonObj(data, obj, errorDesc);
            if(parsed && obj->contains("error"_L1) && obj->contains("message"_L1)) {
                errorCode   = obj->value("error"_L1).toInt();
                *errorDesc  = u"%1 (%2)"_s.arg(obj->value("message"_L1).toString()).arg(errorCode);
                replyResult = ReplyResult::ApiError;
            }
        }

        if(replyResult != ReplyResult::Success || (!data.isEmpty() && !parsed)) {
            qCWarning(SCROBBLER) << "Last.fm reply details:" << describeReply(reply);
            if(!data.isEmpty()) {
                qCWarning(SCROBBLER) << "Last.fm reply body:" << previewReplyBody(data);
            }
        }

        const auto lastfmError = static_cast<ScrobbleError>(errorCode);
        if(reply->error() == QNetworkReply::AuthenticationRequiredError
           || lastfmError == ScrobbleError::InvalidSessionKey || lastfmError == ScrobbleError::UnauthorisedToken
           || lastfmError == ScrobbleError::LoginRequired || lastfmError == ScrobbleError::APIKeySuspended) {
            logout();
        }
    }
    else if(replyResult != ReplyResult::Success) {
        qCWarning(SCROBBLER) << "Last.fm reply details:" << describeReply(reply);
    }

    return replyResult;
}

QNetworkReply* LastFmService::createRequest(const std::map<QString, QString>& params)
{
    std::map<QString, QString> queryParams{
        {u"api_key"_s, apiKey()}, {u"sk"_s, m_sessionKey}, {u"lang"_s, QLocale{}.name().left(2).toLower()}};
    queryParams.insert(params.cbegin(), params.cend());

    QString data;

    for(const auto& [key, value] : std::as_const(queryParams)) {
        data += key + value;
    }
    data += apiSecret();

    const QByteArray digest = QCryptographicHash::hash(data.toUtf8(), QCryptographicHash::Md5);
    const QString signature = QString::fromLatin1(digest.toHex()).rightJustified(32, u'0').toLower();

    const QUrl reqUrl{url()};
    QNetworkRequest req{reqUrl};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_s);

    queryParams.emplace(u"api_sig"_s, signature);
    queryParams.emplace(u"format"_s, u"json"_s);
    const QByteArray query = createRequestBody(queryParams);

    QNetworkReply* reply = addReply(network()->post(req, query));
    qCDebug(SCROBBLER) << "POST queued to network manager for" << name() << "method" << requestMethod(params) << "items"
                       << requestItemCount(params) << "params" << queryParams.size() << "url" << reqUrl.toString()
                       << "bodyBytes" << query.size();

    return reply;
}

QByteArray LastFmService::createRequestBody(const std::map<QString, QString>& params)
{
    QByteArray body;

    for(const auto& [key, value] : params) {
        if(!body.isEmpty()) {
            body += '&';
        }
        body += encodeQueryKey(key);
        body += '=';
        body += encodeQueryValue(value);
    }

    return body;
}

LastFmService::ReplyErrorInfo LastFmService::getReplyErrorInfo(QNetworkReply* reply, const QJsonObject& obj)
{
    ReplyErrorInfo errorInfo;
    errorInfo.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if(obj.contains("error"_L1)) {
        errorInfo.apiErrorCode = obj.value("error"_L1).toInt();
    }
    if(obj.contains("message"_L1)) {
        errorInfo.message = obj.value("message"_L1).toString();
    }

    return errorInfo;
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

    if(!obj.contains("nowplaying"_L1)) {
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
        const ReplyErrorInfo errorInfo = getReplyErrorInfo(reply, obj);

        if(errorInfo.apiErrorCode == static_cast<int>(ScrobbleError::InvalidMethodSignature)) {
            if(items.size() == 1 && items.front()->hasError) {
                qCWarning(SCROBBLER) << "Discarding cached Last.fm scrobble after repeated signature failure:"
                                     << items.front()->metadata.artist << u"-"_s << items.front()->metadata.title
                                     << "timestamp" << items.front()->timestamp;
                cache()->flush(items);
                setSubmitError(false);
                doDelayedSubmit();
                return;
            }

            qCWarning(SCROBBLER) << "Retrying Last.fm scrobbles individually after signature failure for batch of"
                                 << items.size();
            std::ranges::for_each(items, [](const auto& item) {
                item->submitted = false;
                item->hasError  = true;
            });
            setSubmitError(true);
            doDelayedSubmit();
            return;
        }

        setSubmitError(true);
        qCWarning(SCROBBLER) << "Unable to scrobble for" << name() << "count" << items.size() << ":" << errorStr;
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

    if(!obj.contains("scrobbles"_L1)) {
        handleError("Reply from server is missing scrobbles", true);
        return;
    }

    const QJsonValue scrobblesVal = obj.value("scrobbles"_L1);
    if(!scrobblesVal.isObject()) {
        handleError("Scrobbles is not an object", true);
        return;
    }

    obj = scrobblesVal.toObject();
    if(obj.isEmpty()) {
        handleError("Scrobbles object is empty", true);
        return;
    }

    if(!obj.contains("@attr"_L1) || !obj.contains("scrobble"_L1)) {
        handleError("Scrobbles object is missing values", true);
        return;
    }

    const QJsonValue attr = obj.value("@attr"_L1);
    if(!attr.isObject()) {
        handleError("Scrobbles attr is not an object", true);
        return;
    }

    const QJsonObject attrObj = attr.toObject();
    if(attrObj.isEmpty()) {
        handleError("Scrobbles attr is empty", true);
        return;
    }

    if(!attrObj.contains("accepted"_L1) || !attrObj.contains("ignored"_L1)) {
        handleError("Scrobbles attr is missing values", true);
        return;
    }

    const int accepted = attrObj.value("accepted"_L1).toInt();
    const int ignored  = attrObj.value("ignored"_L1).toInt();

    qCDebug(SCROBBLER) << "Scrobbles accepted:" << accepted;
    qCDebug(SCROBBLER) << "Scrobbles ignored:" << ignored;

    QJsonArray array;

    const QJsonValue scrobbleVal = obj.value("scrobble"_L1);
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

        if(!trackJson.contains("artist"_L1) || !trackJson.contains("album"_L1) || !trackJson.contains("albumArtist"_L1)
           || !trackJson.contains("track"_L1) || !trackJson.contains("timestamp"_L1)
           || !trackJson.contains("ignoredMessage"_L1)) {
            handleError("Scrobble is missing values");
            continue;
        }

        const QJsonValue artist     = trackJson.value("artist"_L1);
        const QJsonValue album      = trackJson.value("album"_L1);
        const QJsonValue track      = trackJson.value("track"_L1);
        const QJsonValue ignoredMsg = trackJson.value("ignoredMessage"_L1);

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

        if(!artistObj.contains("#text"_L1) || !albumObj.contains("#text"_L1) || !trackObj.contains("#text"_L1)) {
            continue;
        }

        const QString trackText = trackObj.value("#text"_L1).toString();

        if(ignoreMsgObj.value("code"_L1).toBool()) {
            const QString ignoredMsgText
                = uR"(Scrobble for "%1" ignored: %2)"_s.arg(trackText, ignoreMsgObj.value("#text"_L1).toString());
            handleError(ignoredMsgText.toUtf8().constData());
        }
        else {
            qCDebug(SCROBBLER) << "Scrobble accepted:" << trackText;
        }
    }

    doDelayedSubmit();
}
} // namespace Fooyin::Scrobbler
