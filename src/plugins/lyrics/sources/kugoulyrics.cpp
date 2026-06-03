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
#include "kugoulyrics.h"

#include "lyricsparser.h"

#include <core/network/networkaccessmanager.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTextStream>
#include <QUrl>
#include <QUrlQuery>

#include <zlib.h>

#include <array>

using namespace Qt::StringLiterals;

constexpr auto SearchUrl = "https://lyrics.kugou.com/search"_L1;
constexpr auto LyricUrl  = "https://lyrics.kugou.com/download"_L1;

namespace {
QNetworkRequest setupRequest(const QUrl& url)
{
    QNetworkRequest req{url};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return req;
}

QByteArray inflateZlib(QByteArray compressed)
{
    if(compressed.isEmpty()) {
        return {};
    }

    z_stream stream{};
    stream.next_in  = reinterpret_cast<Bytef*>(compressed.data());
    stream.avail_in = static_cast<uInt>(compressed.size());

    if(inflateInit(&stream) != Z_OK) {
        return {};
    }

    QByteArray uncompressed;
    std::array<char, 4096> buffer{};

    int result{Z_OK};
    while(result != Z_STREAM_END) {
        stream.next_out  = reinterpret_cast<Bytef*>(buffer.data());
        stream.avail_out = static_cast<uInt>(buffer.size());

        result = inflate(&stream, Z_NO_FLUSH);
        if(result != Z_OK && result != Z_STREAM_END) {
            uncompressed.clear();
            break;
        }

        uncompressed.append(buffer.data(), static_cast<qsizetype>(buffer.size() - stream.avail_out));
    }

    inflateEnd(&stream);
    return uncompressed;
}

QString decodeKrc(const QString& content)
{
    QByteArray encoded = QByteArray::fromBase64(content.toUtf8());
    if(encoded.isEmpty()) {
        return {};
    }

    static constexpr std::array<unsigned char, 16> DecodeKey{64, 71, 97, 119, 94,  50,  116, 71,
                                                             81, 54, 49, 45,  206, 210, 110, 105};

    if(encoded.startsWith("krc1")) {
        QByteArray compressed = encoded.sliced(4);
        for(qsizetype i{0}; i < compressed.size(); ++i) {
            compressed[i] = static_cast<char>(static_cast<unsigned char>(compressed.at(i)) ^ DecodeKey.at(i & 0x0F));
        }

        encoded = inflateZlib(compressed);
    }

    return QString::fromUtf8(encoded).trimmed();
}

QString krcToEnhancedLrc(const QString& krc)
{
    if(krc.isEmpty()) {
        return {};
    }

    QString converted;
    QTextStream stream{&converted};

    static const QRegularExpression TagPattern{uR"(^\[(ti|ar|al|by|offset):(.*)\]$)"_s};
    static const QRegularExpression LinePattern{uR"(^\[(\d+),(\d+)\](.*)$)"_s};
    static const QRegularExpression WordPattern{uR"(<(\d+),(\d+),\d+>(.*?)(?=<\d+,\d+,\d+>|$))"_s};

    const QStringList lines = krc.split(u'\n', Qt::SkipEmptyParts);
    for(QString line : lines) {
        if(line.endsWith(u'\r')) {
            line.chop(1);
        }
        if(line.isEmpty() || line.startsWith(u"[language:"_s)) {
            continue;
        }

        const QRegularExpressionMatch tagMatch = TagPattern.match(line);
        if(tagMatch.hasMatch()) {
            stream << line << "\n";
            continue;
        }

        const QRegularExpressionMatch lineMatch = LinePattern.match(line);
        if(!lineMatch.hasMatch()) {
            continue;
        }

        bool ok{false};
        const uint64_t lineTimestamp = lineMatch.capturedView(1).toULongLong(&ok);
        if(!ok) {
            return {};
        }

        stream << u"[%1]"_s.arg(Fooyin::Lyrics::formatTimestamp(lineTimestamp));

        bool hasWords{false};
        auto wordIt = WordPattern.globalMatch(lineMatch.captured(3));
        while(wordIt.hasNext()) {
            const QRegularExpressionMatch wordMatch = wordIt.next();

            const uint64_t wordOffset = wordMatch.capturedView(1).toULongLong(&ok);
            if(!ok) {
                return {};
            }

            const QString word = wordMatch.captured(3);
            if(word.isEmpty()) {
                continue;
            }

            stream << u"<%1>%2"_s.arg(Fooyin::Lyrics::formatTimestamp(lineTimestamp + wordOffset), word);
            hasWords = true;
        }

        if(hasWords) {
            stream << "\n";
        }
    }

    return converted.trimmed();
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
    const QString keyword = u"%1-%2"_s.arg(params.artist, params.title);
    QUrl url{SearchUrl};
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
        Q_EMIT searchResult({});
        resetReply();
        return;
    }

    resetReply();

    const QJsonArray lyricsArray = obj.value("candidates"_L1).toArray();
    if(lyricsArray.isEmpty()) {
        Q_EMIT searchResult({});
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
        metadata.id        = it.value("id"_L1).toVariant().toString();
        metadata.accessKey = it.value("accesskey"_L1).toString();
        metadata.title     = it.value("song"_L1).toString();
        metadata.artist    = it.value("singer"_L1).toString();
        if(!metadata.id.isEmpty() && !metadata.accessKey.isEmpty()) {
            m_metadata.push_back(std::move(metadata));
        }
    }

    if(m_metadata.empty()) {
        Q_EMIT searchResult({});
        return;
    }

    m_currentIndex = 0;
    makeLyricRequest();
}

void KugouLyrics::makeLyricRequest()
{
    if(m_currentIndex < 0 || std::cmp_greater_equal(m_currentIndex, m_metadata.size())) {
        Q_EMIT searchResult(m_data);
        return;
    }

    const KugouMetadata& metadata = m_metadata.at(m_currentIndex);

    QUrl url{LyricUrl};
    QUrlQuery q;
    q.addQueryItem(u"ver"_s, u"1"_s);
    q.addQueryItem(u"client"_s, u"pc"_s);
    q.addQueryItem(u"id"_s, metadata.id);
    q.addQueryItem(u"accesskey"_s, metadata.accessKey);
    q.addQueryItem(u"fmt"_s, u"krc"_s);
    q.addQueryItem(u"charset"_s, u"utf8"_s);
    q.addQueryItem(u"var"_s, u"1"_s);
    url.setQuery(q);

    const QNetworkRequest req = setupRequest(url);

    qCDebug(LYRICS) << u"Sending request: %1"_s.arg(url.toString());

    setReply(network()->get(req));
    QObject::connect(reply(), &QNetworkReply::finished, this, &KugouLyrics::handleLyricReply);
}

void KugouLyrics::handleLyricReply()
{
    QJsonObject obj;
    const bool hasJson = getJsonFromReply(reply(), &obj);

    resetReply();

    if(hasJson) {
        const QString content = obj.value("content"_L1).toString();
        if(!content.isEmpty()) {
            const QString lyrics = krcToEnhancedLrc(decodeKrc(content));
            if(!lyrics.isEmpty()) {
                LyricData data;
                const KugouMetadata& metadata = m_metadata.at(m_currentIndex);
                data.title                    = metadata.title;
                data.artist                   = metadata.artist;
                data.data                     = lyrics;
                m_data.push_back(std::move(data));
            }
        }
    }

    ++m_currentIndex;

    if(std::cmp_greater_equal(m_currentIndex, m_metadata.size())) {
        Q_EMIT searchResult(m_data);
        m_metadata.clear();
    }
    else {
        makeLyricRequest();
    }
}
} // namespace Fooyin::Lyrics
