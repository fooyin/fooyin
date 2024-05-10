/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include <utils/utils.h>

#include <QApplication>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QRandomGenerator>
#include <QTime>
#include <QWidget>

#include <ranges>

namespace Fooyin::Utils {
int randomNumber(int min, int max)
{
    if(min == max) {
        return max;
    }
    return QRandomGenerator::global()->bounded(min, max + 1);
}

QString msToString(uint64_t ms)
{
    constexpr auto msPerSecond = 1000;
    constexpr auto msPerMinute = msPerSecond * 60;
    constexpr auto msPerHour   = msPerMinute * 60;
    constexpr auto msPerDay    = msPerHour * 24;
    constexpr auto msPerWeek   = msPerDay * 7;

    const uint64_t weeks   = ms / msPerWeek;
    const uint64_t days    = (ms % msPerWeek) / msPerDay;
    const uint64_t hours   = (ms % msPerDay) / msPerHour;
    const uint64_t minutes = (ms % msPerHour) / msPerMinute;
    const uint64_t seconds = (ms % msPerMinute) / msPerSecond;

    QString formattedTime;

    if(weeks > 0) {
        formattedTime = formattedTime + QString::number(weeks) + QStringLiteral("wk ");
    }
    if(days > 0) {
        formattedTime = formattedTime + QString::number(days) + QStringLiteral("d ");
    }
    if(hours > 0) {
        formattedTime = formattedTime + addLeadingZero(static_cast<int>(hours), 2) + QStringLiteral(":");
    }

    formattedTime = formattedTime + addLeadingZero(static_cast<int>(minutes), 2) + QStringLiteral(":")
                  + addLeadingZero(static_cast<int>(seconds), 2);
    return formattedTime;
}

QString secsToString(uint64_t secs)
{
    return msToString(secs * 1000);
}

uint64_t currentDateToInt()
{
    const auto str = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMddHHmmss"));
    return static_cast<uint64_t>(str.toULongLong());
}

QString formatTimeMs(uint64_t time)
{
    const QDateTime dateTime  = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(time));
    QString formattedDateTime = dateTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    return formattedDateTime;
}

QString formatFileSize(uint64_t bytes, bool includeBytes)
{
    static const QStringList units = {QStringLiteral("bytes"), QStringLiteral("KB"), QStringLiteral("MB"),
                                      QStringLiteral("GB"), QStringLiteral("TB")};
    auto size                      = static_cast<double>(bytes);
    int unitIndex{0};

    while(size >= 1024 && unitIndex < units.size() - 1) {
        size /= 1024;
        ++unitIndex;
    }

    if(unitIndex >= units.size()) {
        return {};
    }

    QString formattedSize = QStringLiteral("%1 %2").arg(QString::number(size, 'f', 1), units.at(unitIndex));

    if(unitIndex == 0) {
        return formattedSize;
    }

    if(includeBytes) {
        formattedSize.append(QStringLiteral(" (%3 bytes)").arg(bytes));
    }

    return formattedSize;
}

QString addLeadingZero(int number, int leadingCount)
{
    return QStringLiteral("%1").arg(number, leadingCount, 10, QChar::fromLatin1('0'));
}

QString appendShortcut(const QString& str, const QKeySequence& shortcut)
{
    QString string = str;
    string.remove(QChar::fromLatin1('&'));
    return QString::fromLatin1("<div style=\"white-space:pre\">%1 "
                               "<span style=\"color: gray; font-size: small\">%2</span></div>")
        .arg(string, shortcut.toString(QKeySequence::NativeText));
}

void setMinimumWidth(QLabel* label, const QString& text)
{
    const QString oldText = label->text();
    label->setText(text);
    label->setMinimumWidth(0);
    auto width = label->sizeHint().width();
    label->setText(oldText);

    label->setMinimumWidth(width);
}

QString capitalise(const QString& str)
{
    QStringList parts = str.split(QChar::fromLatin1(' '), Qt::SkipEmptyParts);

    for(auto& part : parts) {
        part.replace(0, 1, part[0].toUpper());
    }

    return parts.join(QStringLiteral(" "));
}

QPixmap scalePixmap(const QPixmap& image, const QSize& size, bool upscale)
{
    const QSize scale = 4 * size;
    const int width   = image.size().width();
    const int height  = image.size().height();

    if(width > size.width() || height > size.height()) {
        return image.scaled(scale).scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    if(upscale) {
        return image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    return image;
}

QPixmap scalePixmap(const QPixmap& image, int width, bool upscale)
{
    return scalePixmap(image, {width, width}, upscale);
}

QImage scaleImage(const QImage& image, const QSize& size, bool upscale)
{
    const QSize scale = 4 * size;
    const int width   = image.size().width();
    const int height  = image.size().height();

    if(width > size.width() || height > size.height()) {
        return image.scaled(scale).scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    if(upscale) {
        return image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    return image;
}

QImage scaleImage(const QImage& image, int width, bool upscale)
{
    return scaleImage(image, {width, width}, upscale);
}

QPixmap changePixmapColour(const QPixmap& orig, const QColor& color)
{
    QPixmap pixmap{orig.size()};
    pixmap.fill(color);
    pixmap.setMask(orig.createMaskFromColor(Qt::transparent));
    return pixmap;
}

void showMessageBox(const QString& text, const QString& infoText)
{
    QMessageBox message;
    message.setText(text);
    message.setInformativeText(infoText);
    message.exec();
}

void appendMenuActions(QMenu* originalMenu, QMenu* menu)
{
    const QList<QAction*> actions = originalMenu->actions();
    for(QAction* action : actions) {
        menu->addAction(action);
    }
}

bool isDarkMode()
{
    const QPalette palette{QApplication::palette()};

    const QColor textColour   = palette.color(QPalette::Active, QPalette::Text);
    const QColor windowColour = palette.color(QPalette::Active, QPalette::Window);

    // Assume dark theme if text is a lighter colour than the window background
    const bool isDark = textColour.value() > windowColour.value();

    return isDark;
}

QIcon iconFromTheme(const QString& icon)
{
    return QIcon::fromTheme(icon);
}

QIcon iconFromTheme(const char* icon)
{
    return iconFromTheme(QString::fromLatin1(icon));
}
} // namespace Fooyin::Utils
