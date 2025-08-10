/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <LukeT1@proton.me>
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

#include "discogsartwork.h"

#include <core/network/networkaccessmanager.h>
#include <utils/stringutils.h>

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

constexpr auto SearchUrl = "https://api.discogs.com/database/search";
constexpr auto AccessKey = "RmxzdXNwWkN5V2liZHJ3aHh1YlA=";
constexpr auto Secret    = "Z0NDTU1LYm9SblpnTmpOU2VxQWR6ek1aZVdxZkJZTVE=";

using namespace Qt::StringLiterals;

namespace Fooyin {
DiscogsArtwork::DiscogsArtwork(NetworkAccessManager* network, SettingsManager* settings, int index, bool enabled,
                               QObject* parent)
    : ArtworkSource{network, settings, index, enabled, parent}
    , m_apiKey{QString::fromLatin1(QByteArray::fromBase64(AccessKey))}
    , m_secret{QString::fromLatin1(QByteArray::fromBase64(Secret))}
{ }

QString DiscogsArtwork::name() const
{
    return u"Discogs"_s;
}

CoverTypes DiscogsArtwork::supportedTypes() const
{
    return {Track::Cover::Front, Track::Cover::Artist};
}

void DiscogsArtwork::search(const SearchParams& params)
{
    m_results.clear();
    m_params = params;

    const std::map<QString, QString> queryParams{{u"key"_s, m_apiKey},
                                                 {u"secret"_s, m_secret},
                                                 {u"format"_s, u"album"_s},
                                                 {u"artist"_s, params.artist.toLower()},
                                                 {u"release_title"_s, params.album.toLower()},
                                                 {u"track"_s, params.title.toLower()},
                                                 {u"type"_s, u"master"_s}};

    const QNetworkRequest req = createRequest(QString::fromLatin1(SearchUrl), queryParams, m_secret);

    setReply(network()->get(req));
    QObject::connect(reply(), &QNetworkReply::finished, this, &DiscogsArtwork::handleSearchReply);
}

void DiscogsArtwork::handleSearchReply()
{
    QJsonObject obj;
    if(!getJsonFromReply(reply(), &obj)) {
        resetReply();
        endSearchIfFinished();
        return;
    }

    resetReply();

    QJsonArray results;
    if(obj.contains("results"_L1)) {
        results = obj.value("results"_L1).toArray();
    }
    else if(obj.contains("error"_L1) && obj.contains("message"_L1)) {
        const QString error = obj.value("error"_L1).toString();
        qCDebug(ARTWORK) << "Error:" << error;
        endSearchIfFinished();
        return;
    }
    else {
        endSearchIfFinished();
        return;
    }

    if(results.empty()) {
        endSearchIfFinished();
        return;
    }

    for(const auto& result : std::as_const(results)) {
        const QJsonObject resultObj = result.toObject();

        if(!resultObj.contains("id"_L1) || !resultObj.contains("title"_L1) || !resultObj.contains("resource_url"_L1)) {
            qCWarning(ARTWORK) << "Reply from server is missing id, title or resource_url";
            continue;
        }

        const auto releaseId = static_cast<uint64_t>(resultObj.value("id"_L1).toInt());
        const QUrl releaseUrl{resultObj.value("resource_url"_L1).toString()};
        QString title = resultObj.value("title"_L1).toString();

        if(title.contains(" - "_L1)) {
            QStringList splitTitle = title.split(u" - "_s);

            if(splitTitle.size() == 2) {
                title = splitTitle.back();
            }

            if(!releaseUrl.isValid()) {
                continue;
            }
        }

        const QNetworkRequest req
            = createRequest(releaseUrl, {{u"key"_s, m_apiKey}, {u"secret"_s, m_secret}}, m_secret);

        const auto& releaseReply = m_requests.emplace(releaseId, network()->get(req)).first->second;
        QObject::connect(releaseReply, &QNetworkReply::finished, this,
                         [this, releaseId]() { handleReleaseReply(releaseId); });
    }
}

void DiscogsArtwork::handleReleaseReply(uint64_t id)
{
    if(!m_requests.contains(id)) {
        endSearchIfFinished();
        return;
    }

    auto* reply = m_requests.at(id);
    m_requests.erase(id);

    QJsonObject obj;
    if(!getJsonFromReply(reply, &obj)) {
        reply->deleteLater();
        endSearchIfFinished();
        return;
    }

    if(reply) {
        QObject::disconnect(reply);
        reply->deleteLater();
    }

    QJsonArray artists;
    if(obj.contains("artists"_L1)) {
        artists = obj.value("artists"_L1).toArray();
    }

    if(artists.empty()) {
        endSearchIfFinished();
        return;
    }

    QString artistUrl;
    QStringList artistNames;

    for(const auto& artistResult : std::as_const(artists)) {
        const QJsonObject artistObj = artistResult.toObject();

        if(!artistObj.contains("name"_L1)) {
            qCWarning(ARTWORK) << "Reply from server is missing name.";
            continue;
        }

        QString artistName = artistObj.value("name"_L1).toString();

        static const QRegularExpression re{uR"(^(.*)\(\d+\)$)"_s};
        const QRegularExpressionMatch match = re.match(artistName);
        if(match.hasMatch()) {
            artistName = match.captured(1).simplified();
        }

        artistUrl = artistObj.value("resource_url"_L1).toString();
        artistNames.emplace_back(artistName);
    }

    if(artistNames.empty()) {
        endSearchIfFinished();
        return;
    }

    if(m_params.type == Track::Cover::Artist) {
        if(!artistUrl.isEmpty()) {
            const QNetworkRequest req
                = createRequest(artistUrl, {{u"key"_s, m_apiKey}, {u"secret"_s, m_secret}}, m_secret);

            auto* artistReply = network()->get(req);
            QObject::connect(artistReply, &QNetworkReply::finished, this,
                             [this, artistNames, artistReply]() { handleArtistReply(artistNames, artistReply); });
        }
        else {
            endSearchIfFinished();
        }
        return;
    }

    const QString album = obj["title"_L1].toString();

    QJsonArray images;
    if(obj.contains("images"_L1)) {
        images = obj.value("images"_L1).toArray();
    }

    if(images.empty()) {
        endSearchIfFinished();
        return;
    }

    for(const auto& image : std::as_const(images)) {
        const QJsonObject imageObj = image.toObject();

        if(!imageObj.contains("resource_url"_L1) || !imageObj.contains("width"_L1) || !imageObj.contains("height"_L1)) {
            qCWarning(ARTWORK) << "Reply from server is missing type, resource_url, width or height";
            continue;
        }

        const auto width  = imageObj.value("width"_L1).toVariant().value<float>();
        const auto height = imageObj.value("height"_L1).toVariant().value<float>();

        if(width < 300 || height < 300) {
            continue;
        }

        const float aspectRatio = std::min(width, height) / std::max(width, height);
        if(aspectRatio < 0.75F) {
            continue;
        }

        const QString imageUrl = imageObj.value("resource_url"_L1).toString();

        m_results.emplace_back(artistNames, album, imageUrl);
    }

    endSearchIfFinished();
}

void DiscogsArtwork::handleArtistReply(const QStringList& artists, QNetworkReply* reply)
{
    QJsonObject obj;
    if(!getJsonFromReply(reply, &obj)) {
        reply->deleteLater();
        endSearchIfFinished();
        return;
    }

    if(reply) {
        QObject::disconnect(reply);
        reply->deleteLater();
    }

    QJsonArray images;
    if(obj.contains("images"_L1)) {
        images = obj.value("images"_L1).toArray();
    }

    if(images.empty()) {
        endSearchIfFinished();
        return;
    }

    for(const auto& image : std::as_const(images)) {
        const QJsonObject imageObj = image.toObject();

        if(!imageObj.contains("resource_url"_L1) || !imageObj.contains("width"_L1) || !imageObj.contains("height"_L1)) {
            qCWarning(ARTWORK) << "Reply from server is missing type, resource_url, width or height";
            continue;
        }

        const auto width  = imageObj.value("width"_L1).toVariant().value<float>();
        const auto height = imageObj.value("height"_L1).toVariant().value<float>();

        if(width < 300 || height < 300) {
            continue;
        }

        const float aspectRatio = std::min(width, height) / std::max(width, height);
        if(aspectRatio < 0.75F) {
            continue;
        }

        const QString imageUrl = imageObj.value("resource_url"_L1).toString();

        m_results.emplace_back(artists, QString{}, imageUrl);
    }

    endSearchIfFinished();
}

void DiscogsArtwork::endSearchIfFinished()
{
    if(m_requests.empty()) {
        emit searchResult(m_results);
    }
}
} // namespace Fooyin
