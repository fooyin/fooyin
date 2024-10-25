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

#include "neteaselyrics.h"

#include <core/network/networkaccessmanager.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkCookie>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

// TODO: Use offical API once we resolve issues with random search results
// constexpr auto SearchUrl = "https://music.163.com/api/search/get";
constexpr auto SearchUrl = "https://music.xianqiao.wang/neteaseapiv2/search";
// constexpr auto LyricUrl  = "https://music.163.com/api/song/lyric";
constexpr auto LyricUrl = "https://music.xianqiao.wang/neteaseapiv2/lyric";

namespace {
QNetworkRequest setupRequest(const char* url)
{
    QNetworkRequest req{QString::fromLatin1(url)};

    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));

    return req;
}
} // namespace

namespace Fooyin::Lyrics {
QString NeteaseLyrics::name() const
{
    return QStringLiteral("NetEase Cloud Music");
}

void NeteaseLyrics::search(const SearchParams& params)
{
    resetReply();
    m_data.clear();

    QUrlQuery urlQuery;
    urlQuery.addQueryItem(QStringLiteral("keywords"), QStringLiteral("%1+%2").arg(params.artist, params.title));
    urlQuery.addQueryItem(QStringLiteral("type"), QStringLiteral("1"));
    urlQuery.addQueryItem(QStringLiteral("offset"), QStringLiteral("0"));
    urlQuery.addQueryItem(QStringLiteral("sub"), QStringLiteral("false"));
    urlQuery.addQueryItem(QStringLiteral("limit"), QStringLiteral("3"));
    urlQuery.addQueryItem(QStringLiteral("total"), QStringLiteral("true"));

    const QNetworkRequest req = setupRequest(SearchUrl);

    qCDebug(LYRICS) << QStringLiteral("Sending request: %1?%2").arg(QLatin1String{SearchUrl}).arg(urlQuery.toString());

    setReply(network()->post(req, urlQuery.toString().toUtf8()));
    QObject::connect(reply(), &QNetworkReply::finished, this, &NeteaseLyrics::handleSearchReply);
}

void NeteaseLyrics::handleSearchReply()
{
    QJsonObject obj;
    if(!getJsonFromReply(reply(), &obj)) {
        emit searchResult({});
        resetReply();
        return;
    }

    resetReply();

    const QJsonObject result = obj.value(u"result").toObject();
    const QJsonArray songs   = result.value(u"songs").toArray();

    if(songs.isEmpty()) {
        emit searchResult({});
        return;
    }

    for(const auto& song : songs) {
        const QJsonObject songItem = song.toObject();
        if(songItem.contains(u"id")) {
            LyricData songData;
            songData.id    = QString::number(songItem.value(u"id").toInteger());
            songData.title = songItem.value(u"name").toString();
            songData.album = songItem.value(u"album").toObject().value(u"name").toString();

            const auto artists = songItem.value(u"artists").toArray();
            for(const auto& artist : artists) {
                const QString artistName = artist.toObject().value(u"name").toString();
                if(!artistName.isEmpty()) {
                    songData.artists.push_back(artistName);
                }
            }
            m_data.push_back(songData);
        }
    }

    if(m_data.empty()) {
        emit searchResult({});
        return;
    }

    m_currentData = m_data.begin();
    makeLyricRequest();
}

void NeteaseLyrics::makeLyricRequest()
{
    QUrlQuery urlQuery;
    urlQuery.addQueryItem(QStringLiteral("id"), encode(m_currentData->id));
    urlQuery.addQueryItem(encode(QStringLiteral("lv")), encode(QStringLiteral("-1")));
    urlQuery.addQueryItem(encode(QStringLiteral("kv")), encode(QStringLiteral("-1")));
    urlQuery.addQueryItem(encode(QStringLiteral("tv")), encode(QStringLiteral("-1")));
    urlQuery.addQueryItem(encode(QStringLiteral("os")), encode(QStringLiteral("pc")));

    const QNetworkRequest req = setupRequest(LyricUrl);

    qCDebug(LYRICS) << QStringLiteral("Sending request: %1?%2").arg(QLatin1String{LyricUrl}).arg(urlQuery.toString());

    setReply(network()->post(req, urlQuery.toString(QUrl::FullyEncoded).toUtf8()));
    QObject::connect(reply(), &QNetworkReply::finished, this, &NeteaseLyrics::handleLyricReply);
}

void NeteaseLyrics::handleLyricReply()
{
    QJsonObject obj;
    if(getJsonFromReply(reply(), &obj)) {
        resetReply();

        const QJsonObject lrc = obj.value(u"lrc").toObject();
        const QString lyrics  = lrc.value(u"lyric").toString().trimmed();
        if(!lyrics.isEmpty()) {
            m_currentData->data = lyrics.toUtf8();
        }
    }

    ++m_currentData;

    if(m_currentData == m_data.end()) {
        emit searchResult(m_data);
    }
    else {
        makeLyricRequest();
    }
}
} // namespace Fooyin::Lyrics
