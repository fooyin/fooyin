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

#include "theaudiodbartwork.h"

#include <core/network/networkaccessmanager.h>
#include <core/network/networkutils.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QTimerEvent>
#include <QUrlQuery>

#include <chrono>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;

constexpr auto SearchUrl = "https://www.theaudiodb.com/api/v1/json/123/%1.php";

// The free API allows 30 requests per minute
constexpr auto RequestInterval = 2000ms;

namespace Fooyin {
QString TheAudioDbArtwork::name() const
{
    return u"TheAudioDB"_s;
}

CoverTypes TheAudioDbArtwork::supportedTypes() const
{
    return {Track::Cover::Front, Track::Cover::Back, Track::Cover::Artist};
}

void TheAudioDbArtwork::search(const SearchParams& params)
{
    m_searchTimer.stop();
    resetReply();

    if(params.artist.trimmed().isEmpty() || (params.type != Track::Cover::Artist && params.album.trimmed().isEmpty())) {
        Q_EMIT searchResult({});
        return;
    }

    m_params = params;

    const auto elapsed = m_lastRequest.isValid() ? std::chrono::milliseconds{m_lastRequest.elapsed()} : RequestInterval;
    if(elapsed < RequestInterval) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        m_searchTimer.start(RequestInterval - elapsed, Qt::PreciseTimer, this);
#else
        m_searchTimer.start(static_cast<int>((RequestInterval - elapsed).count()), Qt::PreciseTimer, this);
#endif
        return;
    }

    startSearch();
}

void TheAudioDbArtwork::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_searchTimer.timerId()) {
        m_searchTimer.stop();
        startSearch();
        return;
    }
    ArtworkSource::timerEvent(event);
}

void TheAudioDbArtwork::startSearch()
{
    const bool artistSearch = m_params.type == Track::Cover::Artist;

    QUrl url{QString::fromLatin1(SearchUrl).arg(artistSearch ? u"search"_s : u"searchalbum"_s)};

    QUrlQuery query;
    query.addQueryItem(u"s"_s, encode(m_params.artist));
    if(!artistSearch) {
        query.addQueryItem(u"a"_s, encode(m_params.album));
    }
    url.setQuery(query);

    auto request = makeNetworkRequest(url);
    request.setTransferTimeout(15000);

    m_lastRequest.start();
    setReply(network()->get(request));
    QObject::connect(reply(), &QNetworkReply::finished, this, &TheAudioDbArtwork::handleSearchReply);
}

void TheAudioDbArtwork::handleSearchReply()
{
    QJsonObject obj;
    if(!getJsonFromReply(reply(), &obj)) {
        resetReply();
        Q_EMIT searchResult({});
        return;
    }

    resetReply();

    const bool artistSearch = m_params.type == Track::Cover::Artist;

    const QJsonArray results = obj.value(artistSearch ? "artists"_L1 : "album"_L1).toArray();
    SearchResults searchResults;
    std::set<QString> imageUrls;

    for(const auto& result : results) {
        const QJsonObject resultObj = result.toObject();
        const QString artist        = resultObj.value("strArtist"_L1).toString();
        QString album;
        QString imageUrl;

        if(artistSearch) {
            imageUrl = resultObj.value("strArtistThumb"_L1).toString();
        }
        else {
            album = resultObj.value("strAlbum"_L1).toString();
            if(m_params.type == Track::Cover::Back) {
                imageUrl = resultObj.value("strAlbumBack"_L1).toString();
            }
            else {
                imageUrl = resultObj.value("strAlbumThumbHQ"_L1).toString();
                if(imageUrl.isEmpty()) {
                    imageUrl = resultObj.value("strAlbumThumb"_L1).toString();
                }
            }
        }

        if(artist.isEmpty() || imageUrl.isEmpty() || !QUrl{imageUrl}.isValid() || !imageUrls.insert(imageUrl).second) {
            continue;
        }

        searchResults.emplace_back(QStringList{artist}, album, QUrl{imageUrl});
    }

    Q_EMIT searchResult(searchResults);
}
} // namespace Fooyin
