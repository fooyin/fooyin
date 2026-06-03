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

#include "listenbrainzservice.h"

#include <core/coresettings.h>
#include <core/network/networkaccessmanager.h>
#include <utils/settings/settingsmanager.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QTimerEvent>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

constexpr auto ApiUrl = "https://api.listenbrainz.org";

constexpr auto MaxScrobblesPerRequest = 10;

namespace {
QString normaliseMusicBrainzId(QString value)
{
    value = value.trimmed();
    if(value.isEmpty()) {
        return {};
    }

    static const QRegularExpression mbidRegex{
        uR"(^\{?([0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12})\}?$)"_s};

    const QRegularExpressionMatch match = mbidRegex.match(value);
    if(!match.hasMatch()) {
        return {};
    }

    return match.captured(1).toLower();
}

void insertMusicBrainzId(QJsonObject& object, const QString& key, const QString& value)
{
    if(const QString mbid = normaliseMusicBrainzId(value); !mbid.isEmpty()) {
        object.insert(key, mbid);
    }
    else if(!value.trimmed().isEmpty()) {
        qCDebug(SCROBBLER) << "Skipping invalid ListenBrainz MBID field" << key << value;
    }
}

QJsonObject getTrackMetadata(const Fooyin::Scrobbler::Metadata& metadata)
{
    QJsonObject metaObj;

    metaObj.insert(u"artist_name"_s, metadata.artist);

    if(!metadata.album.isEmpty()) {
        metaObj.insert(u"release_name"_s, metadata.album);
    }

    metaObj.insert(u"track_name"_s, metadata.title);

    QJsonObject infoObj;

    if(metadata.duration > 0) {
        infoObj.insert(u"duration_ms"_s, static_cast<qint64>(metadata.duration) * 1000);
    }
    if(!metadata.trackNum.isEmpty()) {
        infoObj.insert(u"tracknumber"_s, metadata.trackNum);
    }
    insertMusicBrainzId(infoObj, u"recording_mbid"_s, metadata.musicBrainzId);
    insertMusicBrainzId(infoObj, u"release_mbid"_s, metadata.musicBrainzAlbumId);

    infoObj.insert(u"media_player"_s, QCoreApplication::applicationName());
    infoObj.insert(u"media_player_version"_s, QCoreApplication::applicationVersion());
    infoObj.insert(u"submission_client"_s, QCoreApplication::applicationName());
    infoObj.insert(u"submission_client_version"_s, QCoreApplication::applicationVersion());

    metaObj.insert(u"additional_info"_s, infoObj);

    return metaObj;
}

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

int replyApiErrorCode(const QJsonObject& obj)
{
    return obj.value("code"_L1).toInt();
}

QString replyApiErrorText(const QJsonObject& obj)
{
    return obj.value("error"_L1).toString();
}
} // namespace

namespace Fooyin::Scrobbler {
QUrl ListenBrainzService::url() const
{
    return isCustom() ? details().url : QString::fromLatin1(ApiUrl);
}

bool ListenBrainzService::requiresAuthentication() const
{
    return false;
}

bool ListenBrainzService::isAuthenticated() const
{
    return !userToken().trimmed().isEmpty();
}

void ListenBrainzService::saveSession()
{
    FySettings settings;
    settings.beginGroup(isCustom() ? u"Scrobbler-"_s + name() : name());

    settings.setValue("IsEnabled", details().isEnabled);
    if(isCustom()) {
        settings.setValue("URL", details().url.toDisplayString());
    }
    settings.setValue("UserToken", details().token);

    settings.endGroup();
}

void ListenBrainzService::loadSession()
{
    FySettings settings;
    settings.beginGroup(isCustom() ? u"Scrobbler-"_s + name() : name());

    if(settings.contains("IsEnabled")) {
        detailsRef().isEnabled = settings.value("IsEnabled").toBool();
    }
    if(settings.contains("URL")) {
        detailsRef().url = settings.value("URL").toString();
    }
    if(settings.contains("UserToken")) {
        detailsRef().token = settings.value("UserToken").toString();
    }

    settings.endGroup();
}

void ListenBrainzService::deleteSession()
{
    FySettings settings;
    settings.beginGroup(isCustom() ? u"Scrobbler-"_s + name() : name());

    settings.remove("IsEnabled");
    settings.remove("URL");
    settings.remove("UserToken");

    settings.endGroup();
}

void ListenBrainzService::logout()
{
    detailsRef().token.clear();
    saveSession();
}

void ListenBrainzService::testApi()
{
    const QUrl reqUrl{u"%1/1/validate-token"_s.arg(QString::fromUtf8(url().toEncoded()))};

    QNetworkReply* reply = createRequest(RequestType::Get, reqUrl);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { testFinished(reply); });
}

void ListenBrainzService::updateNowPlaying()
{
    QJsonObject metaObj;
    metaObj.insert(u"track_metadata"_s, getTrackMetadata(Metadata{scriptParser(), settings(), currentTrack()}));

    QJsonArray payload;
    payload.append(metaObj);

    QJsonObject object;
    object.insert(u"listen_type"_s, u"playing_now"_s);
    object.insert(u"payload"_s, payload);
    object.insert(u"token"_s, userToken());

    const QJsonDocument doc{object};
    const QUrl reqUrl{u"%1/1/submit-listens"_s.arg(QString::fromUtf8(url().toEncoded()))};

    QNetworkReply* reply = createRequest(RequestType::Post, reqUrl, doc);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { updateNowPlayingFinished(reply); });
}

void ListenBrainzService::submit()
{
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
        obj.insert(u"listened_at"_s, static_cast<qint64>(item->timestamp));
        obj.insert(u"track_metadata"_s, getTrackMetadata(item->metadata));
        array.append(obj);

        if(sentItems.size() >= MaxScrobblesPerRequest || item->hasError) {
            break;
        }
    }

    if(sentItems.empty()) {
        return;
    }

    qCDebug(SCROBBLER) << "Preparing scrobble request for" << name() << "count" << sentItems.size() << "pending"
                       << items.size() << "timestamps" << sentItems.front()->timestamp << "to"
                       << sentItems.back()->timestamp;

    setSubmitted(true);

    QJsonObject object;
    object.insert(u"listen_type"_s, u"import"_s);
    object.insert(u"payload"_s, array);
    object.insert(u"token"_s, userToken());

    const QJsonDocument doc{object};
    const QUrl reqUrl{u"%1/1/submit-listens"_s.arg(QString::fromUtf8(url().toEncoded()))};
    QNetworkReply* reply = createRequest(RequestType::Post, reqUrl, doc);
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

QNetworkReply* ListenBrainzService::createRequest(RequestType type, const QUrl& url, const QJsonDocument& json)
{
    QNetworkRequest req{url};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_s);
    req.setRawHeader("Authorization", u"Token %1"_s.arg(userToken()).toUtf8());
    const QByteArray payload = json.toJson();

    qCDebug(SCROBBLER) << (type == RequestType::Get ? "GET queued to network manager for"
                                                    : "POST queued to network manager for")
                       << name() << "url" << url.toString() << "bodyBytes" << payload.size();

    switch(type) {
        case RequestType::Get:
            return addReply(network()->get(req));
        case RequestType::Post:
            return addReply(network()->post(req, payload));
    }

    return nullptr;
}

ScrobblerService::ReplyResult ListenBrainzService::getJsonFromReply(QNetworkReply* reply, QJsonObject* obj,
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
        bool parsed{false};

        if(!data.isEmpty()) {
            parsed = extractJsonObj(data, obj, errorDesc);
        }

        if(parsed) {
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

        if(replyResult != ReplyResult::Success || (!data.isEmpty() && !parsed)) {
            qCWarning(SCROBBLER) << "ListenBrainz reply details:" << describeReply(reply);
            if(!data.isEmpty()) {
                qCWarning(SCROBBLER) << "ListenBrainz reply body:" << previewReplyBody(data);
            }
        }
    }
    else if(replyResult != ReplyResult::Success) {
        qCWarning(SCROBBLER) << "ListenBrainz reply details:" << describeReply(reply);
    }

    return replyResult;
}

void ListenBrainzService::testFinished(QNetworkReply* reply)
{
    if(!removeReply(reply)) {
        return;
    }

    QJsonObject obj;
    QString errorStr;
    if(getJsonFromReply(reply, &obj, &errorStr) != ReplyResult::Success) {
        handleTestError(errorStr.toUtf8().constData());
        return;
    }

    if(!obj.contains("valid"_L1)) {
        handleTestError("Json reply from server is missing valid");
        return;
    }

    const bool valid = obj.value("valid"_L1).toBool();
    if(!valid) {
        handleTestError("Token could not be authenticated");
    }
    else {
        Q_EMIT testApiFinished(true);
    }
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
        const int apiErrorCode = replyApiErrorCode(obj);
        const QString apiError = replyApiErrorText(obj);

        if(apiErrorCode == 400 && items.size() == 1 && items.front()->hasError) {
            qCWarning(SCROBBLER) << "Discarding cached ListenBrainz scrobble after repeated validation failure:"
                                 << items.front()->metadata.artist << u"-"_s << items.front()->metadata.title
                                 << "timestamp" << items.front()->timestamp << "error" << apiError;
            cache()->flush(items);
            setSubmitError(false);
            doDelayedSubmit();
            return;
        }

        setSubmitError(true);
        qCWarning(SCROBBLER) << "Unable to scrobble for" << name() << "count" << items.size() << ":" << errorStr;
        std::ranges::for_each(items, [](const auto& item) {
            item->submitted = false;
            item->hasError  = true;
        });
    }

    doDelayedSubmit();
}

QString ListenBrainzService::userToken() const
{
    return details().token;
}
} // namespace Fooyin::Scrobbler
