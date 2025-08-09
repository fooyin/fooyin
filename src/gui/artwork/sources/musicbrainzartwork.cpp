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

#include "musicbrainzartwork.h"

#include <core/network/networkaccessmanager.h>
#include <utils/settings/settingsmanager.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

constexpr auto SearchUrl  = "https://musicbrainz.org/ws/2/release/";
constexpr auto ArtworkUrl = "https://coverartarchive.org/release/%1/%2";

namespace {
QString coverTypeToString(Fooyin::Track::Cover cover)
{
    using Cover = Fooyin::Track::Cover;

    switch(cover) {
        case(Cover::Front):
            return u"front"_s;
        case(Cover::Back):
            return u"back"_s;
        case(Cover::Artist):
        case(Cover::Other):
        default:
            return {};
    }
}
} // namespace

namespace Fooyin {
QString Fooyin::MusicBrainzArtwork::name() const
{
    return u"MusicBrainz (Cover Art Archive)"_s;
}

CoverTypes MusicBrainzArtwork::supportedTypes() const
{
    return {Track::Cover::Front, Track::Cover::Back};
}

void MusicBrainzArtwork::search(const SearchParams& params)
{
    resetReply();

    m_type = params.type;

    QUrl url{QString::fromLatin1(SearchUrl)};

    QUrlQuery urlQuery;
    urlQuery.addQueryItem(u"query"_s, uR"("release:"%1" AND artist:"%2")"_s.arg(params.album, params.artist));
    urlQuery.addQueryItem(u"limit"_s, QString::number(4));
    urlQuery.addQueryItem(u"fmt"_s, u"json"_s);
    url.setQuery(urlQuery);

    QNetworkRequest req{url};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setRawHeader(
        "User-Agent",
        u"fooyin/%1 (https://www.fooyin.org)"_s.arg(settings()->value<Settings::Core::Version>()).toUtf8());

    qCDebug(ARTWORK) << u"Sending request: %1?%2"_s.arg(QLatin1String{SearchUrl}).arg(urlQuery.toString());

    setReply(network()->get(req));
    QObject::connect(reply(), &QNetworkReply::finished, this, &MusicBrainzArtwork::handleSearchReply);
}

void MusicBrainzArtwork::handleSearchReply()
{
    QJsonObject obj;
    if(!getJsonFromReply(reply(), &obj)) {
        resetReply();
        emit searchResult({});
        return;
    }

    resetReply();

    QJsonArray releases;
    if(obj.contains("releases"_L1)) {
        releases = obj.value("releases"_L1).toArray();
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

    if(releases.empty()) {
        emit searchResult({});
        return;
    }

    SearchResults searchResults;

    for(const auto& release : std::as_const(releases)) {
        if(!release.isObject()) {
            continue;
        }

        const QJsonObject releaseObj = release.toObject();

        const QJsonArray artists = releaseObj.value("artist-credit"_L1).toArray();
        if(artists.empty()) {
            continue;
        }

        QString artist;
        for(const auto& artistVal : artists) {
            if(!artistVal.isObject()) {
                continue;
            }

            const QJsonObject artistObj = artistVal.toObject().value("artist"_L1).toObject();
            const QString artistName    = artistObj.value("name"_L1).toString();

            if(!artistName.isEmpty()) {
                if(!artist.isEmpty()) {
                    artist = u"Various Artists"_s;
                }
                else {
                    artist = artistName;
                }
            }
        }

        const QString releaseId = releaseObj.value("id"_L1).toString();
        const QString album     = releaseObj.value("title"_L1).toString();
        const QUrl url          = QString::fromLatin1(ArtworkUrl).arg(releaseId, coverTypeToString(m_type));

        searchResults.emplace_back(artist, album, url);
    }

    emit searchResult(searchResults);
}
} // namespace Fooyin
