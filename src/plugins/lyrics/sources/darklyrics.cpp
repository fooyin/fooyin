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

#include "darklyrics.h"

#include <core/network/networkaccessmanager.h>

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>
#include <QXmlStreamReader>

constexpr auto SearchUrl = "http://darklyrics.com/lyrics/";

namespace {
QString normalise(const QString& str)
{
    if(str.isEmpty()) {
        return {};
    }

    QString result;
    for(QChar ch : str) {
        if(ch.isLetterOrNumber() && ch.unicode() < 128) {
            result.append(ch);
        }
    }

    return result.toLower();
}

QString cleanTitle(const QString& title)
{
    static const QRegularExpression regex{QStringLiteral(R"(^\d+\.\s*)")};
    return title.section(regex, 1, 1);
}
} // namespace

namespace Fooyin::Lyrics {
QString DarkLyrics::name() const
{
    return QStringLiteral("DarkLyrics");
}

void DarkLyrics::search(const SearchParams& params)
{
    resetReply();

    m_params       = params;
    QString artist = normalise(params.artist);
    QString album  = normalise(params.album);

    if(artist.isEmpty() || album.isEmpty()) {
        emit searchResult({});
        return;
    }

    const QUrl url{QString::fromLatin1(SearchUrl) + artist + u"/" + album + u".html"};

    qCInfo(LYRICS) << "Sending request" << url.toString();

    const QNetworkRequest req{url};
    setReply(network()->get(req));
    QObject::connect(reply(), &QNetworkReply::finished, this, &DarkLyrics::handleLyricReply);
}

void DarkLyrics::handleLyricReply()
{
    if(reply()->error() != QNetworkReply::NoError) {
        emit searchResult({});
        resetReply();
        return;
    }

    const QByteArray htmlData = reply()->readAll();
    const QString htmlContent = QString::fromUtf8(htmlData);

    resetReply();

    QXmlStreamReader reader(htmlContent);
    QString lyricsText;
    bool trackFound{false};

    while(!reader.atEnd() && !reader.hasError()) {
        reader.readNext();

        if(!trackFound && reader.isStartElement() && reader.name() == u"h3") {
            reader.readNextStartElement();
            if(reader.name() == u"a" && reader.attributes().hasAttribute(QLatin1String{"name"})) {
                const QString title = cleanTitle(reader.readElementText());
                if(title == m_params.title) {
                    trackFound = true;
                }
            }
        }

        else if(trackFound) {
            if(reader.isEndElement() && reader.name() == u"br") {
                if(!lyricsText.isEmpty()) {
                    lyricsText += u'\n';
                }
            }
            else if(reader.isCharacters() && !reader.isWhitespace()) {
                QString lineText = reader.text().toString();
                if(lineText.startsWith(u"\n")) {
                    lineText = lineText.sliced(1);
                }
                lyricsText.append(lineText);
            }
            else if(reader.isStartElement() && reader.name() == u"h3") {
                break;
            }
        }
    }

    if(reader.hasError()) {
        qCDebug(LYRICS) << "Error parsing HTML:" << reader.errorString();
        emit searchResult({});
    }

    if(lyricsText == u"[Instrumental]") {
        emit searchResult({});
        return;
    }

    LyricData lyrics;
    lyrics.title  = m_params.title;
    lyrics.album  = m_params.album;
    lyrics.artist = m_params.artist;
    lyrics.data   = lyricsText;

    emit searchResult({lyrics});
}
} // namespace Fooyin::Lyrics
