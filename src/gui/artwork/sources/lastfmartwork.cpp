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

#include "lastfmartwork.h"

#include <core/network/networkaccessmanager.h>
#include <utils/settings/settingsmanager.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

constexpr auto SearchUrl = "https://ws.audioscrobbler.com/2.0/";
constexpr auto ApiKey    = "YjJkNTdjOTc4YTIyYmUyNzljYzNiZTZkNjc2MjdmZWE=";
constexpr auto ApiSecret = "ODYzZDBiNWI0M2I2NmQ1MmVkOTU4NGFiOWJiZTc3MDY=";

namespace {
enum class CoverSize : uint16_t
{
    Unknown    = 0,
    Small      = 34,
    Medium     = 64,
    Large      = 174,
    ExtraLarge = 300
};

CoverSize coverSizeFromString(const QString& size)
{
    if(size == "small"_L1) {
        return CoverSize::Small;
    }
    if(size == "medium"_L1) {
        return CoverSize::Medium;
    }
    if(size == "large"_L1) {
        return CoverSize::Large;
    }
    if(size == "extralarge"_L1) {
        return CoverSize::ExtraLarge;
    }

    return CoverSize::Unknown;
}
} // namespace

namespace Fooyin {
LastFmArtwork::LastFmArtwork(NetworkAccessManager* network, SettingsManager* settings, int index, bool enabled,
                             QObject* parent)
    : ArtworkSource{network, settings, index, enabled, parent}
    , m_apiKey{QString::fromLatin1(QByteArray::fromBase64(ApiKey))}
    , m_secret{QString::fromLatin1(QByteArray::fromBase64(ApiSecret))}
{ }

QString Fooyin::LastFmArtwork::name() const
{
    return u"LastFM"_s;
}

CoverTypes LastFmArtwork::supportedTypes() const
{
    return {Track::Cover::Front};
}

void LastFmArtwork::search(const SearchParams& params)
{
    resetReply();
    m_queryType.clear();

    m_type = params.type;

    QString method;
    QString searchQuery{params.artist};

    if(m_type == Track::Cover::Artist) {
        method      = "artist.search"_L1;
        m_queryType = "artist"_L1;
    }
    else if(params.album.isEmpty() && !params.title.isEmpty()) {
        method      = "track.search"_L1;
        m_queryType = "track"_L1;
        if(!searchQuery.isEmpty()) {
            searchQuery.append(' '_L1);
            searchQuery.append(params.title);
        }
    }
    else {
        method      = "album.search"_L1;
        m_queryType = "album"_L1;
        if(!params.album.isEmpty()) {
            if(!searchQuery.isEmpty()) {
                searchQuery.append(' '_L1);
            }
            searchQuery.append(params.album);
        }
    }

    std::map<QString, QString> queryParams{{u"api_key"_s, m_apiKey},
                                           {u"lang"_s, QLocale{}.name().left(2).toLower()},
                                           {u"method"_s, method},
                                           {m_queryType, searchQuery}};

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

    queryUrl.addQueryItem(u"api_sig"_s, QString::fromLatin1(QUrl::toPercentEncoding(signature)));
    queryUrl.addQueryItem(u"format"_s, u"json"_s);

    QUrl reqUrl{QString::fromLatin1(SearchUrl)};
    reqUrl.setQuery(queryUrl);

    QNetworkRequest req{reqUrl};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_s);

    qCDebug(ARTWORK) << "Sending request" << queryUrl.toString(QUrl::FullyDecoded);

    setReply(network()->get(req));
    QObject::connect(reply(), &QNetworkReply::finished, this, &LastFmArtwork::handleSearchReply);
}

void LastFmArtwork::handleSearchReply()
{
    QJsonObject obj;
    if(!getJsonFromReply(reply(), &obj)) {
        resetReply();
        emit searchResult({});
        return;
    }

    resetReply();

    QJsonObject results;
    if(obj.contains("results"_L1)) {
        results = obj.value("results"_L1).toObject();
    }
    else if(obj.contains("error"_L1) && obj.contains("message"_L1)) {
        const QString error = obj.value("error"_L1).toString();
        qCDebug(ARTWORK) << "Error:" << error;
        emit searchResult({});
        return;
    }
    else {
        emit searchResult({});
        return;
    }

    if(results.empty()) {
        emit searchResult({});
        return;
    }

    QJsonObject match;

    if(m_queryType == "album"_L1) {
        if(results.contains("albummatches"_L1)) {
            match = results.value("albummatches"_L1).toObject();
        }
        else {
            qCWarning(ARTWORK) << "Reply from server is missing albummatches";
            emit searchResult({});
            return;
        }
    }
    else if(m_queryType == "track"_L1) {
        if(results.contains("trackmatches"_L1)) {
            match = results.value("trackmatches"_L1).toObject();
        }
        else {
            qCWarning(ARTWORK) << "Reply from server is missing trackmatches";
            emit searchResult({});
            return;
        }
    }

    if(match.empty()) {
        emit searchResult({});
        return;
    }

    if(!match.contains(m_queryType)) {
        qCWarning(ARTWORK) << "Json is missing " << m_queryType;
        emit searchResult({});
        return;
    }

    const QJsonArray matchResults = match.value(m_queryType).toArray();

    SearchResults searchResults;

    for(const auto& result : matchResults) {
        if(!result.isObject()) {
            continue;
        }

        const QJsonObject resultObj = result.toObject();

        const QString artist = resultObj.value("artist"_L1).toString();
        QString album;
        if(m_queryType == "album"_L1) {
            album = resultObj.value("name"_L1).toString();
        }

        if(!resultObj.contains("image"_L1)) {
            continue;
        }

        const QJsonArray images = resultObj.value("image"_L1).toArray();

        QString lastCoverUrl;
        CoverSize lastCoverSize{CoverSize::Unknown};

        for(const auto& image : images) {
            const QJsonObject imageObj = image.toObject();

            if(!imageObj.contains("#text"_L1) || !imageObj.contains("size"_L1)) {
                continue;
            }

            const QString imageUrl = imageObj.value("#text"_L1).toString();
            if(imageUrl.isEmpty()) {
                continue;
            }

            const CoverSize coverSize = coverSizeFromString(imageObj.value("size"_L1).toString().toLower());
            if(lastCoverUrl.isEmpty() || coverSize > lastCoverSize) {
                lastCoverUrl  = imageUrl;
                lastCoverSize = coverSize;
            }
        }

        if(lastCoverUrl.isEmpty()) {
            continue;
        }

        if(lastCoverUrl.contains("/300x300/"_L1)) {
            // Get maximum resolution available
            lastCoverUrl = lastCoverUrl.replace("/300x300/"_L1, "/_x_/"_L1);
        }

        searchResults.emplace_back(artist, album, lastCoverUrl);
    }

    emit searchResult(searchResults);
}
} // namespace Fooyin
