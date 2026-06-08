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

QIcon placeholderIcon(const RadioStation& station, int size)
{
    const int iconSize = std::clamp(size, 32, 384);
    const auto hue     = static_cast<int>(qHash(station.streamUrl.isEmpty() ? station.name : station.streamUrl) % 360);
    const QColor background = QColor::fromHsv(hue, 180, 220);

    QPixmap pixmap{iconSize, iconSize};
    pixmap.fill(Qt::transparent);

    QPainter painter{&pixmap};
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(background);
    painter.drawRoundedRect(pixmap.rect().adjusted(4, 4, -4, -4), iconSize / 12, iconSize / 12);

    const QString initials = stationInitials(station.name);

    QFont font = painter.font();
    font.setBold(true);
    font.setPointSize(std::max(8, (iconSize * 9) / 32));
    painter.setFont(font);
    painter.setPen(Qt::black);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, initials);

    return QIcon{pixmap};
}
} // namespace Fooyin::RadioBrowser::Utils
