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
#include "kugoulyrics.h"

#include <core/network/networkaccessmanager.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

namespace {
QNetworkRequest setupRequest(const QUrl& url)
{
    QNetworkRequest req{url};
    return req;
}
} // namespace

namespace Fooyin::Lyrics {
QString KugouLyrics::name() const
{
    return u"Kugou Music"_s;
}

void KugouLyrics::search(const SearchParams& params)
{
    resetReply();
    m_metadata.clear();
    m_data.clear();

    // Construct search URL
    QString keyword = u"%1-%2"_s.arg(params.artist, params.title);
    QUrl url(QStringLiteral("http://lyrics.kugou.com/search"));
    QUrlQuery q;
    q.addQueryItem(u"ver"_s, u"1"_s);
    q.addQueryItem(u"man"_s, u"yes"_s);
    q.addQueryItem(u"client"_s, u"pc"_s);
    q.addQueryItem(u"keyword"_s, keyword);

    // duration in milliseconds
    if(params.track.duration() > 0) {
        q.addQueryItem(u"duration"_s, QString::number(params.track.duration()));
    }
    q.addQueryItem(u"hash"_s, u""_s);
    url.setQuery(q);

    const QNetworkRequest req = setupRequest(url);

    qCDebug(LYRICS) << u"Sending request: %1"_s.arg(url.toString());

    setReply(network()->get(req));
    QObject::connect(reply(), &QNetworkReply::finished, this, &KugouLyrics::handleSearchReply);
}

void KugouLyrics::handleSearchReply()
{
    QJsonObject obj;
    if(!getJsonFromReply(reply(), &obj)) {
        emit searchResult({});
        resetReply();
        return;
    }

    resetReply();

    const QJsonArray lyricsArray = obj.value("candidates"_L1).toArray();
    if(lyricsArray.isEmpty()) {
        emit searchResult({});
        return;
    }

    for(const auto& item : lyricsArray) {
        const QJsonObject it = item.toObject();
        if(it.value("id"_L1).isNull() || it.value("accesskey"_L1).isNull()) {
            continue;
        }

        // filter out lyrics with score below 30
        if(it.contains("score"_L1) && it.value("score"_L1).isDouble()) {
            double score = it.value("score"_L1).toDouble();
            if(score < 30) {
                continue;
            }
        }

        KugouMetadata metadata;
        metadata.id        = it.value("id"_L1).toString();
        metadata.accessKey = it.value("accesskey"_L1).toString();
        metadata.title     = it.value("song"_L1).toString();
        metadata.artist    = it.value("singer"_L1).toString();
        m_metadata.push_back(std::move(metadata));
    }

    if(m_metadata.empty()) {
        emit searchResult({});
        return;
    }

    m_currentIndex = 0;
    makeLyricRequest();
}

void KugouLyrics::makeLyricRequest()
{
    if(m_currentIndex < 0 || std::cmp_greater_equal(m_currentIndex, m_metadata.size())) {
        emit searchResult(m_data);
        return;
    }

    const KugouMetadata& metadata = m_metadata.at(m_currentIndex);

    QUrl url(QStringLiteral("http://lyrics.kugou.com/download"));
    QUrlQuery q;
    q.addQueryItem(u"ver"_s, u"1"_s);
    q.addQueryItem(u"client"_s, u"pc"_s);
    q.addQueryItem(u"id"_s, metadata.id);
    q.addQueryItem(u"accesskey"_s, metadata.accessKey);
    q.addQueryItem(u"fmt"_s, u"lrc"_s);
    q.addQueryItem(u"charset"_s, u"utf8"_s);
    url.setQuery(q);

    const QNetworkRequest req = setupRequest(url);

    qCDebug(LYRICS) << u"Sending request: %1"_s.arg(url.toString());

    setReply(network()->get(req));
    QObject::connect(reply(), &QNetworkReply::finished, this, &KugouLyrics::handleLyricReply);
}

void KugouLyrics::handleLyricReply()
{
    QJsonObject obj;
    if(getJsonFromReply(reply(), &obj)) {
        // If content exists, it's base64 encoded
        const QString content = obj.value("content"_L1).toString();
        if(!content.isEmpty()) {
            const QByteArray decoded = QByteArray::fromBase64(content.toUtf8());
            LyricData data;
            const KugouMetadata& metadata = m_metadata.at(m_currentIndex);
            data.title                    = metadata.title;
            data.artist                   = metadata.artist;
            data.data                     = QString::fromUtf8(decoded).trimmed();
            m_data.push_back(std::move(data));
        }
    }

    ++m_currentIndex;

    if(std::cmp_greater_equal(m_currentIndex, m_metadata.size())) {
        emit searchResult(m_data);
        m_metadata.clear();
    }
    else {
        makeLyricRequest();
    }
}
} // namespace Fooyin::Lyrics
