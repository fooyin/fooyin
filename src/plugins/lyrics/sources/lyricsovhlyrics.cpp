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

#include "lyricsovhlyrics.h"

#include <core/network/networkaccessmanager.h>
#include <core/network/networkutils.h>

#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimerEvent>

using namespace Qt::StringLiterals;

constexpr auto ApiUrl       = "https://api.lyrics.ovh/v1/";
constexpr auto RequestDelay = 300;

namespace {
QString cleanLyrics(QString lyrics)
{
    lyrics.replace("\r\n"_L1, "\n"_L1);

    // Some lyrics start with a French heading: "Paroles de la chanson <title> par <artist>"
    static const QRegularExpression heading{u"^Paroles de la chanson [^\n]* par [^\n]*\n"_s};
    lyrics.remove(heading);

    return lyrics.trimmed();
}
} // namespace

namespace Fooyin::Lyrics {
QString LyricsOvhLyrics::name() const
{
    return u"lyrics.ovh"_s;
}

void LyricsOvhLyrics::search(const SearchParams& params)
{
    cancel();

    // Songs are looked up by artist and title only
    if(params.artist.isEmpty() || params.title.isEmpty()) {
        Q_EMIT searchResult({});
        return;
    }

    m_params = params;

    // Artist and title are path segments, so slashes (AC/DC) must be encoded too
    const QByteArray url
        = QByteArray{ApiUrl} + QUrl::toPercentEncoding(params.artist) + '/' + QUrl::toPercentEncoding(params.title);
    m_requestUrl = QUrl::fromEncoded(url);
    m_requestTimer.start(RequestDelay, this);
}

void LyricsOvhLyrics::cancel()
{
    m_requestTimer.stop();
    resetReply();
}

void LyricsOvhLyrics::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_requestTimer.timerId()) {
        m_requestTimer.stop();
        sendRequest();
    }
    LyricSource::timerEvent(event);
}

void LyricsOvhLyrics::sendRequest()
{
    const QNetworkRequest req = makeNetworkRequest(m_requestUrl);

    qCDebug(LYRICS) << "Sending request" << m_requestUrl.toString();

    setReply(network()->get(req));
    QObject::connect(reply(), &QNetworkReply::finished, this, &LyricsOvhLyrics::handleLyricReply);
}

void LyricsOvhLyrics::handleLyricReply()
{
    auto* lyricReply = reply();
    if(!lyricReply) {
        return;
    }

    QJsonObject obj;
    const bool success = getJsonFromReply(lyricReply, &obj);
    resetReply();

    if(success) {
        const QString lyrics = cleanLyrics(obj.value("lyrics"_L1).toString());
        if(!lyrics.isEmpty()) {
            // The reply holds only the lyrics, found for the artist and title we asked for
            LyricData data;
            data.title  = m_params.title;
            data.album  = m_params.album;
            data.artist = m_params.artist;
            data.data   = lyrics;

            Q_EMIT searchResult({data});
            return;
        }
    }

    Q_EMIT searchResult({});
}
} // namespace Fooyin::Lyrics
