/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "deezerartwork.h"

#include <core/network/networkaccessmanager.h>
#include <core/network/networkutils.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

constexpr auto SearchUrl = "https://api.deezer.com/search/%1";

namespace Fooyin {
QString DeezerArtwork::name() const
{
    return u"Deezer"_s;
}

CoverTypes DeezerArtwork::supportedTypes() const
{
    return {Track::Cover::Front, Track::Cover::Artist};
}

void DeezerArtwork::search(const SearchParams& params)
{
    resetReply();
    m_type = params.type;

    QString searchQuery{params.artist};
    if(m_type == Track::Cover::Artist && params.album.isEmpty()) {
        m_queryType = u"artist"_s;
    }
    else {
        const bool searchTrack = params.album.isEmpty() && !params.title.isEmpty();
        m_queryType            = searchTrack ? u"track"_s : u"album"_s;
        const QString title    = searchTrack ? params.title : params.album;
        if(!title.isEmpty()) {
            if(!searchQuery.isEmpty()) {
                searchQuery.append(u' ');
            }
            searchQuery.append(title);
        }
    }

    if(searchQuery.trimmed().isEmpty()) {
        Q_EMIT searchResult({});
        return;
    }

    QUrl url{QString::fromLatin1(SearchUrl).arg(m_queryType)};

    QUrlQuery query;
    query.addQueryItem(u"q"_s, encode(searchQuery));
    query.addQueryItem(u"limit"_s, u"10"_s);
    url.setQuery(query);

    auto request = makeNetworkRequest(url);
    request.setTransferTimeout(15000);

    setReply(network()->get(request));
    QObject::connect(reply(), &QNetworkReply::finished, this, &DeezerArtwork::handleSearchReply);
}

void DeezerArtwork::handleSearchReply()
{
    QJsonObject obj;
    if(!getJsonFromReply(reply(), &obj)) {
        resetReply();
        Q_EMIT searchResult({});
        return;
    }

    resetReply();

    if(obj.contains("error"_L1)) {
        qCDebug(ARTWORK) << "Deezer error:" << obj.value("error"_L1).toObject().value("message"_L1).toString();
        Q_EMIT searchResult({});
        return;
    }

    const QJsonArray results = obj.value("data"_L1).toArray();
    SearchResults searchResults;
    std::set<QString> imageUrls;

    for(const auto& result : results) {
        const QJsonObject resultObj = result.toObject();
        QString artist;
        QString album;
        QString imageUrl;

        if(m_type == Track::Cover::Artist) {
            const QJsonObject artistObj
                = m_queryType == "artist"_L1 ? resultObj : resultObj.value("artist"_L1).toObject();
            artist   = artistObj.value("name"_L1).toString();
            imageUrl = artistObj.value("picture_xl"_L1).toString();
            if(m_queryType == "album"_L1) {
                album = resultObj.value("title"_L1).toString();
            }
        }
        else {
            const QJsonObject albumObj = m_queryType == "track"_L1 ? resultObj.value("album"_L1).toObject() : resultObj;
            artist                     = resultObj.value("artist"_L1).toObject().value("name"_L1).toString();
            album                      = albumObj.value("title"_L1).toString();
            imageUrl                   = albumObj.value("cover_xl"_L1).toString();
        }

        if(artist.isEmpty() || imageUrl.isEmpty() || !QUrl{imageUrl}.isValid() || !imageUrls.insert(imageUrl).second) {
            continue;
        }

        searchResults.emplace_back(QStringList{artist}, album, QUrl{imageUrl});
    }

    Q_EMIT searchResult(searchResults);
}
} // namespace Fooyin
