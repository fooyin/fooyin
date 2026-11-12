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

#include "radiobrowserutils.h"

#include <core/constants.h>

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPixmap>

using namespace Qt::StringLiterals;

constexpr auto MaxInitials = 3;

namespace Fooyin::RadioBrowser::Utils {
namespace {
QString stationInitials(const QString& name)
{
    QString initials;
    bool inWord{false};

    for(const QChar& ch : name) {
        if(!ch.isLetterOrNumber()) {
            inWord = false;
            continue;
        }

        if(!inWord) {
            initials += ch.toUpper();
            if(initials.size() >= MaxInitials) {
                break;
            }
        }
        inWord = true;
    }

    return initials.isEmpty() ? u"R"_s : initials;
}
} // namespace

Track trackForStation(const RadioStation& station, const QString& streamUrl)
{
    Track track{streamUrl.isEmpty() ? station.effectiveStreamUrl() : streamUrl};
    track.setTitle(station.name);

    if(!station.tags.isEmpty()) {
        track.setGenres(splitStationTags(station.tags));
    }
    if(!station.codec.isEmpty()) {
        track.setCodec(station.codec);
    }
    if(station.bitrate > 0) {
        track.setBitrate(station.bitrate);
    }
    if(!station.favicon.isEmpty()) {
        track.replaceExtraTag(u"COVERART"_s, station.favicon);
    }

    track.setMetadataWasRead(true);
    track.setExtraProperty(QLatin1StringView{Constants::PlaybackSource::Provider},
                           station.local ? u"local"_s : u"radio-browser.info"_s);
    track.setExtraProperty(QLatin1StringView{Constants::PlaybackSource::ExternalId}, station.uuid);
    track.setExtraProperty(QLatin1StringView{Constants::PlaybackSource::OriginalUri}, station.streamUrl);
    track.setExtraProperty(u"_RADIO_BROWSER_STATIONUUID"_s, station.uuid);
    track.setExtraProperty(u"_RADIO_BROWSER_STATIONNAME"_s, station.name);
    track.setExtraProperty(u"_RADIO_BROWSER_STREAMURL"_s, station.streamUrl);
    track.setExtraProperty(u"_RADIO_BROWSER_HOMEPAGE"_s, station.homepage);
    track.setExtraProperty(u"_RADIO_BROWSER_FAVICON"_s, station.favicon);
    track.setExtraProperty(u"_RADIO_BROWSER_COUNTRY"_s, station.country);
    track.setExtraProperty(u"_RADIO_BROWSER_COUNTRYCODE"_s, station.countryCode);
    track.setExtraProperty(u"_RADIO_BROWSER_LANGUAGE"_s, station.language);
    track.setExtraProperty(u"_RADIO_BROWSER_HLS"_s, station.hls ? u"true"_s : u"false"_s);

    return track;
}

QIcon placeholderIcon(const RadioStation& station, int size)
{
    const int iconSize = std::clamp(size, 32, 384);
    const auto hue     = static_cast<int>(qHash(station.streamUrl.isEmpty() ? station.name : station.streamUrl) % 360);
    const QColor background = QColor::fromHsv(hue, 180, 220);

    QPixmap pixmap{iconSize, iconSize};
    pixmap.fill(background);

    QPainter painter{&pixmap};
    painter.setRenderHint(QPainter::Antialiasing);

    const QString initials = stationInitials(station.name);

    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(std::max(8, (iconSize * 9) / 32));
    painter.setFont(font);
    painter.setPen(Qt::black);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, initials);
    painter.end();

    return QIcon{pixmap};
}

QString displayNetworkError(const QString& error)
{
    static constexpr auto ErrorTransferPrefix    = "Error transferring "_L1;
    static constexpr auto ErrorTransferSeparator = " - "_L1;

    if(!error.startsWith(ErrorTransferPrefix)) {
        return error;
    }

    const qsizetype separator = error.indexOf(ErrorTransferSeparator, ErrorTransferPrefix.size());
    if(separator < 0) {
        return error;
    }

    return error.sliced(separator + ErrorTransferSeparator.size()).trimmed();
}
} // namespace Fooyin::RadioBrowser::Utils
