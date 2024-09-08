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

#include "scrobblerservice.h"

#include "scrobblerauthsession.h"
#include "scrobblercache.h"
#include "scrobblersettings.h"

#include <core/network/networkaccessmanager.h>
#include <utils/paths.h>
#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocale>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QPointer>
#include <QPushButton>
#include <QTcpServer>
#include <QUrlQuery>

Q_LOGGING_CATEGORY(SCROBBLER, "fy.scrobbler")

constexpr auto MaxScrobblesPerRequest  = 50;
constexpr auto MinScrobbleDelay        = 5000;
constexpr auto MinScrobbleDelayOnError = 30000;

namespace {
enum class ReplyResult : uint8_t
{
    Success = 0,
    ServerError,
    ApiError,
};

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

bool extractJsonObj(const QByteArray& data, QJsonObject* obj, QString* errorDesc)
{
    QJsonParseError error;
    const QJsonDocument json = QJsonDocument::fromJson(data, &error);

    if(errorDesc && error.error != QJsonParseError::NoError) {
        *errorDesc = error.errorString();
        return false;
    }

    if(json.isObject()) {
        *obj = json.object();
    }

    return true;
}

bool canBeScrobbled(const Fooyin::Track& track)
{
    return track.isValid() && !track.artists().empty() && !track.title().isEmpty() && track.duration() >= 30000;
}
} // namespace

namespace Fooyin::Scrobbler {
class ScrobblerServicePrivate
{
public:
    ScrobblerServicePrivate(ScrobblerService* self, NetworkAccessManager* network, SettingsManager* settings)
        : m_self{self}
        , m_network{network}
        , m_settings{settings}
    { }

    void deleteAll();
    void cleanupAuth();

    void handleAuthError(const char* error);

    ReplyResult getJsonFromReply(QNetworkReply* reply, QJsonObject* obj, QString* errorDesc) const;
    QNetworkReply* createRequest(const std::map<QString, QString>& params);

    void requestAuth(const QString& token);
    void authFinished(QNetworkReply* reply);

    void updateNowPlayingFinished(QNetworkReply* reply);
    void scrobbleFinished(QNetworkReply* reply, const CacheItemList& items);

    void doDelayedSubmit(bool initial = false);
    void submit();

    ScrobblerService* m_self;
    NetworkAccessManager* m_network;
    SettingsManager* m_settings;

    QString m_name;
    QUrl m_apiUrl;
    QUrl m_authUrl;
    QString m_apiKey;
    QString m_secret;

    ScrobblerAuthSession* m_authSession{nullptr};
    std::vector<QNetworkReply*> m_replies;
    ScrobblerCache* m_cache{nullptr};
    QString m_username;
    QString m_sessionKey;

    QBasicTimer m_submitTimer;
    bool m_submitError{false};

    Track m_currentTrack;
    uint64_t m_timestamp{0};
    bool m_scrobbled{false};
    bool m_submitted{false};
};

void ScrobblerServicePrivate::deleteAll()
{
    for(auto* reply : m_replies) {
        QObject::disconnect(reply, nullptr, m_self, nullptr);
        reply->abort();
        reply->deleteLater();
    }

    m_replies.clear();
    cleanupAuth();
}

void ScrobblerServicePrivate::cleanupAuth()
{
    if(m_authSession) {
        QObject::disconnect(m_authSession, nullptr, m_self, nullptr);
        m_authSession->deleteLater();
        m_authSession = nullptr;
    }
}

void ScrobblerServicePrivate::handleAuthError(const char* error)
{
    qCWarning(SCROBBLER) << error;
    emit m_self->authenticationFinished(false, QString::fromUtf8(error));
    cleanupAuth();
}

ReplyResult ScrobblerServicePrivate::getJsonFromReply(QNetworkReply* reply, QJsonObject* obj, QString* errorDesc) const
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
            m_self->logout();
        }
    }

    return replyResult;
}

QNetworkReply* ScrobblerServicePrivate::createRequest(const std::map<QString, QString>& params)
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

    const QUrl url{m_apiUrl};
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));

    const QByteArray query = queryUrl.toString(QUrl::FullyEncoded).toUtf8();

    QNetworkReply* reply = m_replies.emplace_back(m_network->post(req, query));
    qCDebug(SCROBBLER) << "Sending request" << queryUrl.toString(QUrl::FullyDecoded);

    return reply;
}

void ScrobblerServicePrivate::requestAuth(const QString& token)
{
    QUrl url{m_apiUrl};

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
    QNetworkReply* reply = m_replies.emplace_back(m_network->get(req));
    QObject::connect(reply, &QNetworkReply::finished, m_self, [this, reply]() { authFinished(reply); });
}

void ScrobblerServicePrivate::authFinished(QNetworkReply* reply)
{
    if(std::erase(m_replies, reply) == 0) {
        return;
    }

    QObject::disconnect(reply, nullptr, m_self, nullptr);
    reply->deleteLater();

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
    settings.beginGroup(m_name);
    settings.setValue(QLatin1String{"Username"}, m_username);
    settings.setValue(QLatin1String{"SessionKey"}, m_sessionKey);
    settings.endGroup();

    emit m_self->authenticationFinished(true);
    cleanupAuth();
}

void ScrobblerServicePrivate::updateNowPlayingFinished(QNetworkReply* reply)
{
    if(std::erase(m_replies, reply) == 0) {
        return;
    }

    QObject::disconnect(reply, nullptr, m_self, nullptr);
    reply->deleteLater();

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

void ScrobblerServicePrivate::scrobbleFinished(QNetworkReply* reply, const CacheItemList& items)
{
    if(std::erase(m_replies, reply) == 0) {
        return;
    }

    QObject::disconnect(reply, nullptr, m_self, nullptr);
    reply->deleteLater();

    m_submitted = false;

    QJsonObject obj;
    QString errorStr;
    if(getJsonFromReply(reply, &obj, &errorStr) != ReplyResult::Success) {
        m_submitError = true;
        qCWarning(SCROBBLER) << errorStr;
        std::ranges::for_each(items, [](const auto& item) { item->submitted = false; });
        doDelayedSubmit();
        return;
    }

    m_cache->flush(items);
    m_submitError = false;

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

void ScrobblerServicePrivate::doDelayedSubmit(bool initial)
{
    if(m_submitted || m_cache->count() == 0) {
        return;
    }

    const int scrobbleDelay = m_settings->value<Settings::Scrobbler::ScrobblingDelay>();

    if(initial && !m_submitError && scrobbleDelay <= 0) {
        if(m_submitTimer.isActive()) {
            m_submitTimer.stop();
        }
        submit();
    }
    else if(!m_submitTimer.isActive()) {
        const int delay = std::max(scrobbleDelay, m_submitError ? MinScrobbleDelayOnError : MinScrobbleDelay);
        m_submitTimer.start(delay, m_self);
    }
}

void ScrobblerServicePrivate::submit()
{
    if(!m_self->isAuthenticated()) {
        return;
    }

    qCDebug(SCROBBLER) << m_name << "Submitting scrobbles";

    std::map<QString, QString> params{{QStringLiteral("method"), QStringLiteral("track.scrobble")}};

    const bool preferAlbumArtist = m_settings->value<Settings::Scrobbler::PreferAlbumArtist>();
    const CacheItemList items    = m_cache->items();
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

    m_submitted = true;

    QNetworkReply* reply = createRequest(params);
    QObject::connect(reply, &QNetworkReply::finished, m_self,
                     [this, reply, sentItems]() { scrobbleFinished(reply, sentItems); });
}

ScrobblerService::ScrobblerService(NetworkAccessManager* network, SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , p{std::make_unique<ScrobblerServicePrivate>(this, network, settings)}
{ }

ScrobblerService::~ScrobblerService()
{
    p->deleteAll();
}

QString ScrobblerService::name() const
{
    return p->m_name;
}

bool ScrobblerService::isAuthenticated() const
{
    return !p->m_username.isEmpty() && !p->m_sessionKey.isEmpty();
}

QString ScrobblerService::username() const
{
    return p->m_username;
}

void ScrobblerService::authenticate()
{
    if(!p->m_authSession) {
        p->m_authSession = new ScrobblerAuthSession(this);
        QObject::connect(p->m_authSession, &ScrobblerAuthSession::tokenReceived, this,
                         [this](const QString& token) { p->requestAuth(token); });
    }

    const QUrlQuery query{{QStringLiteral("api_key"), p->m_apiKey},
                          {QStringLiteral("cb"), p->m_authSession->callbackUrl()}};

    QUrl url{p->m_authUrl};
    url.setQuery(query);

    const QString messageTitle    = tr("%1 Authentication").arg(p->m_name);
    const QString messageSubtitle = tr("Open url in web browser?")
                                  + QStringLiteral("<br /><br /><a href=\"%1\">%1</a><br />").arg(url.toString());

    const QPointer<QMessageBox> message
        = new QMessageBox(QMessageBox::Information, messageTitle, messageSubtitle, QMessageBox::Cancel);
    message->setAttribute(Qt::WA_DeleteOnClose);
    message->setTextFormat(Qt::RichText);

    auto* openButton = new QPushButton(tr("Open"), message);
    message->addButton(openButton, QMessageBox::AcceptRole);
    auto* copyButton = new QPushButton(tr("Copy URL"), message);
    message->addButton(copyButton, QMessageBox::ActionRole);

    QObject::connect(openButton, &QPushButton::clicked, this, [url]() { QDesktopServices::openUrl(url); });
    QObject::connect(copyButton, &QPushButton::clicked, this,
                     [url]() { QApplication::clipboard()->setText(url.toString()); });

    QObject::connect(this, &ScrobblerService::authenticationFinished, this, [this, message](const bool success) {
        if(success) {
            if(message) {
                message->deleteLater();
            }
            p->cleanupAuth();
        }
    });
    QObject::connect(message, &QMessageBox::finished, this, [this, url](const int result) {
        switch(result) {
            case(QMessageBox::Cancel):
                p->cleanupAuth();
                emit authenticationFinished(false);
            default:
                break;
        }
    });

    message->show();
}

void ScrobblerService::loadSession()
{
    FySettings settings;
    settings.beginGroup(p->m_name);
    p->m_username   = settings.value(QLatin1String{"Username"}).toString();
    p->m_sessionKey = settings.value(QLatin1String{"SessionKey"}).toString();
    settings.endGroup();
}

void ScrobblerService::logout()
{
    p->m_username.clear();
    p->m_sessionKey.clear();

    FySettings settings;
    settings.beginGroup(p->m_name);
    settings.remove(QLatin1String{"Username"});
    settings.remove(QLatin1String{"SessionKey"});
    settings.endGroup();
}

void ScrobblerService::saveCache()
{
    p->m_cache->writeCache();
}

void ScrobblerService::updateNowPlaying(const Track& track)
{
    p->m_currentTrack = track;
    p->m_timestamp    = static_cast<uint64_t>(QDateTime::currentSecsSinceEpoch());
    p->m_scrobbled    = false;

    if(!p->m_settings->value<Settings::Scrobbler::ScrobblingEnabled>()) {
        return;
    }

    if(!isAuthenticated() || !canBeScrobbled(track)) {
        return;
    }

    const bool preferAlbumArtist = p->m_settings->value<Settings::Scrobbler::PreferAlbumArtist>();

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

    QNetworkReply* reply = p->createRequest(params);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { p->updateNowPlayingFinished(reply); });
}

void ScrobblerService::scrobble(const Track& track)
{
    if(!p->m_settings->value<Settings::Scrobbler::ScrobblingEnabled>()) {
        return;
    }

    if(!canBeScrobbled(track)) {
        return;
    }

    if(track.id() != p->m_currentTrack.id() || track.uniqueFilepath() != p->m_currentTrack.uniqueFilepath()) {
        return;
    }

    p->m_scrobbled = true;
    p->m_cache->add(track, p->m_timestamp);

    if(!isAuthenticated()) {
        return;
    }

    p->doDelayedSubmit(true);
}

void ScrobblerService::setName(const QString& name)
{
    p->m_name = name;
    if(!p->m_cache) {
        p->m_cache = new ScrobblerCache(Utils::cachePath() + u"/" + name.toLower() + u".cache", this);
    }
}

void ScrobblerService::setApiUrl(const QUrl& url)
{
    p->m_apiUrl = url;
}

void ScrobblerService::setAuthUrl(const QUrl& url)
{
    p->m_authUrl = url;
}

void ScrobblerService::setApiKey(const QString& key)
{
    p->m_apiKey = key;
}

void ScrobblerService::setSecret(const QString& secret)
{
    p->m_secret = secret;
}

void ScrobblerService::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == p->m_submitTimer.timerId()) {
        p->m_submitTimer.stop();
        p->submit();
    }
    QObject::timerEvent(event);
}
} // namespace Fooyin::Scrobbler
