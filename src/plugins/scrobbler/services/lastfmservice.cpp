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

#include <core/accountstore.h>
#include <core/coresettings.h>
#include <core/network/networkaccessmanager.h>
#include <core/network/networkutils.h>
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

QString trackStatsKey(const Fooyin::Track& track)
{
    if(!track.hash().isEmpty()) {
        return track.hash();
    }
    return QString::number(track.id()) + u':' + track.uniqueFilepath();
}
} // namespace

namespace Fooyin::Scrobbler {
LastFmService::LastFmService(ServiceDetails service, NetworkAccessManager* network, SettingsManager* settings,
                             DbConnectionPoolPtr dbPool, QObject* parent)
    : ScrobblerService{std::move(service), network, settings, std::move(dbPool), parent}
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

bool LastFmService::supportsLoved() const
{
    return true;
}

bool LastFmService::supportsTrackStatsSync() const
{
    return true;
}

void LastFmService::saveSession()
{
    FySettings settings;
    settings.beginGroup(accountName());

    settings.setValue("IsEnabled", details().isEnabled);
    settings.setValue("SubmitLoved", details().submitLoved);
    settings.setValue("SyncPlaybackStats", details().syncPlaybackStats);

    if(!accountStore().save(accountName(), {{u"Username"_s, m_username}, {u"SessionKey"_s, m_sessionKey}})) {
        qCWarning(SCROBBLER) << "Unable to persist account:" << name();
    }

    settings.endGroup();
}

void LastFmService::loadSession()
{
    FySettings settings;
    settings.beginGroup(accountName());

    if(settings.contains("IsEnabled")) {
        detailsRef().isEnabled = settings.value("IsEnabled").toBool();
    }
    detailsRef().submitLoved       = settings.value("SubmitLoved", false).toBool();
    detailsRef().syncPlaybackStats = settings.value("SyncPlaybackStats", false).toBool();

    if(const auto account = accountStore().load(accountName(), {u"Username"_s, u"SessionKey"_s});
       account && !account->empty()) {
        m_username   = account->at(u"Username"_s);
        m_sessionKey = account->at(u"SessionKey"_s);
    }

    settings.endGroup();
    synchroniseAccount();
}

void LastFmService::deleteSession()
{
    if(!accountStore().remove(accountName(), {u"Username"_s, u"SessionKey"_s})) {
        qCWarning(SCROBBLER) << "Unable to delete account:" << name();
    }
}

void LastFmService::logout()
{
    stopSubmissions();
    m_trackStatsReplies.clear();
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
    const Metadata metadata{scriptParser(), settings(), currentTrack()};

    std::map<QString, QString> params
        = {{u"method"_s, u"track.updateNowPlaying"_s}, {u"artist"_s, metadata.artist}, {u"track"_s, metadata.title}};
    if(!metadata.album.isEmpty()) {
        params.emplace(u"album"_s, metadata.album);
    }
    if(settings()->value<Settings::Scrobbler::SendAlbumArtist>() && !metadata.albumArtist.isEmpty()) {
        params.emplace(u"albumArtist"_s, metadata.albumArtist);
    }
    if(!metadata.musicBrainzId.isEmpty()) {
        params.emplace(u"mbid"_s, metadata.musicBrainzId);
    }

    QNetworkReply* reply = createRequest(params);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { updateNowPlayingFinished(reply); });
}

void LastFmService::submit()
{
    if(!synchroniseAccount()) {
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
        if(!md.musicBrainzId.isEmpty()) {
            params.emplace(u"mbid[%1]"_s.arg(i), md.musicBrainzId);
        }
        if(md.duration > 0) {
            params.emplace(u"duration[%1]"_s.arg(i), QString::number(md.duration));
        }
        if(md.radio) {
            params.emplace(u"chosenByUser[%1]"_s.arg(i), u"0"_s);
        }
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

void LastFmService::submitLoved(const LovedItem& item)
{
    const std::map<QString, QString> params{{u"method"_s, item.loved ? u"track.love"_s : u"track.unlove"_s},
                                            {u"artist"_s, item.metadata.artist},
                                            {u"track"_s, item.metadata.title}};

    QNetworkReply* reply = createRequest(params);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, item]() { lovedFinished(reply, item); });
}

void LastFmService::fetchTrackStats(const Track& track)
{
    if(!isAuthenticated()) {
        return;
    }

    const Metadata metadata{scriptParser(), settings(), track};
    if(metadata.artist.isEmpty() || metadata.title.isEmpty()) {
        return;
    }

    const QString key = trackStatsKey(track);
    if(const auto reply = m_trackStatsReplies.find(key);
       reply != m_trackStatsReplies.cend() && !reply->second.isNull()) {
        return;
    }

    QUrl requestUrl{url()};
    QUrlQuery query;
    query.addQueryItem(u"api_key"_s, apiKey());
    query.addQueryItem(u"method"_s, u"track.getInfo"_s);
    query.addQueryItem(u"username"_s, username());
    query.addQueryItem(u"artist"_s, metadata.artist);
    query.addQueryItem(u"track"_s, metadata.title);
    query.addQueryItem(u"autocorrect"_s, u"0"_s);
    query.addQueryItem(u"format"_s, u"json"_s);
    requestUrl.setQuery(query);

    const QNetworkRequest request = makeNetworkRequest(requestUrl);
    QNetworkReply* reply          = addReply(network()->get(request));
    m_trackStatsReplies.insert_or_assign(key, reply);
    QObject::connect(reply, &QNetworkReply::finished, this,
                     [this, reply, key, track]() { trackStatsFinished(reply, key, track); });
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

    const QNetworkRequest req = makeNetworkRequest(reqUrl);
    QNetworkReply* reply      = addReply(network()->get(req));
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

    synchroniseAccount();
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
    QNetworkRequest req = makeNetworkRequest(reqUrl);
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
    std::ranges::for_each(items, [](auto* item) { item->submitted = false; });

    const auto retry = [this](const QString& message) {
        qCWarning(SCROBBLER) << message;
        setSubmitError(true);
        doDelayedSubmit();
    };

    QJsonObject obj;
    QString errorStr;
    if(getJsonFromReply(reply, &obj, &errorStr) != ReplyResult::Success) {
        const ReplyErrorInfo errorInfo = getReplyErrorInfo(reply, obj);

        if(errorInfo.apiErrorCode == static_cast<int>(ScrobbleError::InvalidMethodSignature)) {
            if(items.size() == 1 && items.front()->hasError) {
                qCWarning(SCROBBLER) << "Discarding cached Last.fm scrobble after repeated signature failure:"
                                     << items.front()->metadata.artist << items.front()->metadata.title;
                cache()->flush(items);
                cache()->writeCache();
                setSubmitError(false);
                doDelayedSubmit();
                return;
            }
            std::ranges::for_each(items, [](auto* item) { item->hasError = true; });
        }

        if(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 429
           || errorInfo.apiErrorCode == static_cast<int>(ScrobbleError::RateLimitExceeded)) {
            bool ok{false};
            const auto seconds = reply->rawHeader("Retry-After").toLongLong(&ok);
            const int cooldown
                = ok ? static_cast<int>(std::clamp<qint64>(seconds, 0, 24LL * 60 * 60) * 1000) : 6 * 60 * 1000;
            setSubmitCooldown(cooldown);
        }
        retry(errorStr);
        return;
    }

    const auto integer = [](const QJsonValue& value) -> std::optional<qint64> {
        if(!value.isString() && !value.isDouble()) {
            return {};
        }

        bool ok{false};
        const qint64 number = value.toVariant().toString().toLongLong(&ok);
        return ok ? std::optional{number} : std::nullopt;
    };

    const QJsonObject scrobbles = obj.value("scrobbles"_L1).toObject();
    const QJsonObject attr      = scrobbles.value("@attr"_L1).toObject();

    const auto accepted      = integer(attr.value("accepted"_L1));
    const auto ignored       = integer(attr.value("ignored"_L1));
    const QJsonValue results = scrobbles.value("scrobble"_L1);
    const QJsonArray array   = results.isObject() ? QJsonArray{results.toObject()} : results.toArray();

    if(!accepted || !ignored || *accepted < 0 || *ignored < 0 || std::cmp_greater(*accepted, items.size())
       || std::cmp_greater(*ignored, items.size()) || *accepted + *ignored != static_cast<qint64>(items.size())
       || array.size() != static_cast<qsizetype>(items.size())) {
        retry(u"Invalid Last.fm scrobble response; keeping the submitted batch"_s);
        return;
    }

    std::vector<qint64> codes;
    qint64 acceptedResults{0};
    for(int i{0}; std::cmp_less(i, array.size()); ++i) {
        const QJsonObject result = array.at(i).toObject();
        const auto code          = integer(result.value("ignoredMessage"_L1).toObject().value("code"_L1));
        const auto timestamp     = integer(result.value("timestamp"_L1));
        if(!code || *code < 0 || !timestamp || *timestamp < 0
           || std::cmp_not_equal(*timestamp, items.at(i)->timestamp)) {
            retry(u"Ambiguous Last.fm scrobble result; keeping the submitted batch"_s);
            return;
        }
        codes.push_back(*code);
        acceptedResults += (*code == 0);
    }

    if(acceptedResults != *accepted) {
        retry(u"Inconsistent Last.fm scrobble counts; keeping the submitted batch"_s);
        return;
    }

    CacheItemList completed;
    bool deferred{false};
    for(int i{0}; std::cmp_less(i, items.size()); ++i) {
        const auto code = codes.at(i);
        if(code == 0) {
            completed.push_back(items.at(i));
        }
        else {
            qCWarning(SCROBBLER) << "Last.fm ignored scrobble:" << items.at(i)->metadata.title << "code" << code;
            if(code >= 1 && code <= 4) {
                completed.push_back(items.at(i));
            }
            else {
                deferred = true;
                if(code == 5) {
                    setSubmitCooldown(60 * 60 * 1000);
                }
            }
        }
    }

    cache()->flush(completed);
    cache()->writeCache();
    setSubmitError(deferred);
    doDelayedSubmit();
}

void LastFmService::lovedFinished(QNetworkReply* reply, const LovedItem& item)
{
    if(!removeReply(reply)) {
        return;
    }

    QJsonObject object;
    QString error;
    const ReplyResult result = getJsonFromReply(reply, &object, &error);
    if(result == ReplyResult::Success) {
        lovedUpdateFinished(item, LovedUpdateResult::Success);
        return;
    }

    const ReplyErrorInfo errorInfo = getReplyErrorInfo(reply, object);
    const auto apiError            = static_cast<ScrobbleError>(errorInfo.apiErrorCode);
    const bool retry               = result == ReplyResult::ServerError || apiError == ScrobbleError::OperationFailed
                                  || apiError == ScrobbleError::ServiceOffline || apiError == ScrobbleError::TempUnavailable
                                  || apiError == ScrobbleError::RateLimitExceeded;

    qCWarning(SCROBBLER) << "Unable to update Loved state for" << name() << item.metadata.artist << u"-"_s
                         << item.metadata.title << ':' << error;
    lovedUpdateFinished(item, retry ? LovedUpdateResult::Retry : LovedUpdateResult::Discard);
}

void LastFmService::trackStatsFinished(QNetworkReply* reply, const QString& key, const Track& track)
{
    m_trackStatsReplies.erase(key);
    if(!removeReply(reply)) {
        return;
    }

    QJsonObject object;
    QString error;
    if(getJsonFromReply(reply, &object, &error) != ReplyResult::Success) {
        qCWarning(SCROBBLER) << "Unable to fetch track statistics for" << name() << track.filepath() << ':' << error;
        return;
    }

    const QJsonObject trackObject = object.value("track"_L1).toObject();
    if(trackObject.isEmpty()) {
        qCWarning(SCROBBLER) << "Track statistics response from" << name() << "is missing track data";
        return;
    }

    RemoteTrackStats stats{.track = track, .loved = {}, .playCount = {}};
    if(const QJsonValue loved = trackObject.value("userloved"_L1); !loved.isUndefined()) {
        stats.loved = loved.toVariant().toInt() != 0;
    }
    if(const QJsonValue playCount = trackObject.value("userplaycount"_L1); !playCount.isUndefined()) {
        stats.playCount = playCount.toVariant().toInt();
    }

    Q_EMIT trackStatsFetched(stats);
}
} // namespace Fooyin::Scrobbler
