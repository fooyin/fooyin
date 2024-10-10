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
#include <QFontMetrics>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QPixmapCache>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTime>
#include <QWidget>
#include <QWindow>

#include <unicode/ucsdet.h>

constexpr auto DefaultIconSize = 20;
constexpr std::array<const char*, 6> DateFormats{"yyyy-MM-dd hh:mm:ss", "yyyy-MM-dd hh:mm", "yyyy-MM-dd hh",
                                                 "yyyy-MM-dd",          "yyyy-MM",          "yyyy"};

namespace Fooyin::Utils {
int randomNumber(int min, int max)
{
    if(min >= max) {
        return max;
    }

    if(max == std::numeric_limits<int>::max()) {
        --max;
    }

    return QRandomGenerator::global()->bounded(min, max + 1);
}

QString msToString(std::chrono::milliseconds ms, bool includeMs)
{
    const auto weeks = duration_cast<std::chrono::weeks>(ms);
    ms -= duration_cast<std::chrono::milliseconds>(weeks);

    const auto days = duration_cast<std::chrono::days>(ms);
    ms -= duration_cast<std::chrono::milliseconds>(days);

    const auto hours = duration_cast<std::chrono::hours>(ms);
    ms -= duration_cast<std::chrono::milliseconds>(hours);

    const auto minutes = duration_cast<std::chrono::minutes>(ms);
    ms -= duration_cast<std::chrono::milliseconds>(minutes);

    const auto seconds = duration_cast<std::chrono::seconds>(ms);
    ms -= duration_cast<std::chrono::milliseconds>(seconds);

    const auto milliseconds = ms.count();

    QString formattedTime;

    if(weeks.count() > 0) {
        formattedTime += QStringLiteral("%1wk ").arg(weeks.count());
    }
    if(days.count() > 0) {
        formattedTime += QStringLiteral("%1d ").arg(days.count());
    }
    if(hours.count() > 0) {
        formattedTime += QStringLiteral("%1:").arg(hours.count(), 2, 10, QLatin1Char{'0'});
    }

    formattedTime += QStringLiteral("%1:%2")
                         .arg(minutes.count(), 2, 10, QLatin1Char{'0'})
                         .arg(seconds.count(), 2, 10, QLatin1Char{'0'});

    if(includeMs) {
        formattedTime += QStringLiteral(".%1").arg(milliseconds, 3, 10, QLatin1Char{'0'});
    }

    return formattedTime;
}

QString msToString(uint64_t ms)
{
    return msToString(static_cast<std::chrono::milliseconds>(ms), false);
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

std::array<const char*, 6> dateFormats()
{
    return DateFormats;
}

QDateTime dateStringToDate(const QString& str)
{
    for(const auto& format : DateFormats) {
        const QDateTime date = QDateTime::fromString(str, QLatin1String{format});
        if(date.isValid()) {
            return date;
        }
    }

    return {};
}

std::optional<int64_t> dateStringToMs(const QString& str)
{
    const QDateTime date = dateStringToDate(str);
    if(date.isValid()) {
        return date.toMSecsSinceEpoch();
    }
    return {};
}

QString msToDateString(int64_t dateMs)
{
    const QDateTime dateTime  = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(dateMs));
    QString formattedDateTime = dateTime.toString(u"yyyy-MM-dd HH:mm:ss");
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
    return QStringLiteral("%1").arg(number, leadingCount, 10, QLatin1Char{'0'});
}

QString appendShortcut(const QString& str, const QKeySequence& shortcut)
{
    QString string = str;
    string.remove(QChar::fromLatin1('&'));
    return QString::fromLatin1("<div style=\"white-space:pre\">%1 "
                               "<span style=\"color: gray; font-size: small\">%2</span></div>")
        .arg(string, shortcut.toString(QKeySequence::NativeText));
}

QString elideTextWithBreaks(const QString& text, const QFontMetrics& fontMetrics, int maxWidth, Qt::TextElideMode mode)
{
    QStringList lines = text.split(QChar::LineSeparator);
    for(auto i{0}; i < lines.size(); ++i) {
        lines[i] = fontMetrics.elidedText(lines[i], mode, maxWidth);
    }
    return lines.join(QChar::LineSeparator);
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
    QStringList parts = str.split(u' ', Qt::SkipEmptyParts);

    for(auto& part : parts) {
        part.replace(0, 1, part[0].toUpper());
    }

    return parts.join(u' ');
}

QByteArray detectEncoding(const QByteArray& content)
{
    QByteArray encoding;
    UErrorCode status{U_ZERO_ERROR};

    UCharsetDetector* csd = ucsdet_open(&status);
    ucsdet_setText(csd, content.constData(), static_cast<int32_t>(content.length()), &status);

    const UCharsetMatch* ucm = ucsdet_detect(csd, &status);
    if(U_SUCCESS(status) && ucm) {
        const char* cname = ucsdet_getName(ucm, &status);
        if(U_SUCCESS(status)) {
            encoding = QByteArray{cname};
        }
    }

    ucsdet_close(csd);

    return encoding;
}

QStringList extensionsToWildcards(const QStringList& extensions)
{
    QStringList wildcards;
    std::ranges::transform(extensions, std::back_inserter(wildcards),
                           [](const QString& extension) { return QStringLiteral("*.%1").arg(extension); });
    return wildcards;
}

QString extensionsToFilterList(const QStringList& extensions, const QString& name)
{
    QStringList filterList;

    for(const QString& ext : extensions) {
        filterList.append(QStringLiteral("%1 %2 (*.%3)").arg(ext, name, ext));
    }

    return filterList.join(u";;");
}

QString extensionFromFilter(const QString& filter)
{
    static const QRegularExpression re{QStringLiteral(R"(\*\.(\w+))")};
    const QRegularExpressionMatch match = re.match(filter);

    if(match.hasMatch()) {
        return match.captured(1);
    }

    return {};
}

QPixmap scalePixmap(const QPixmap& image, const QSize& size, double dpr, bool upscale)
{
    const QSize scaledSize(static_cast<int>(size.width() * dpr), static_cast<int>(size.height() * dpr));
    const int width  = image.size().width();
    const int height = image.size().height();

    QPixmap scaledPixmap{image};

    if(width > scaledSize.width() || height > scaledSize.height()) {
        scaledPixmap = image.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    if(upscale) {
        scaledPixmap = image.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    scaledPixmap.setDevicePixelRatio(dpr);

    return scaledPixmap;
}

QPixmap scalePixmap(const QPixmap& image, int width, double dpr, bool upscale)
{
    return scalePixmap(image, {width, width}, dpr, upscale);
}

QImage scaleImage(const QImage& image, const QSize& size, double dpr, bool upscale)
{
    const QSize scaledSize(static_cast<int>(size.width() * dpr), static_cast<int>(size.height() * dpr));
    const int width  = image.size().width();
    const int height = image.size().height();

    QImage scaledImage{image};

    if(width > scaledSize.width() || height > scaledSize.height()) {
        scaledImage = image.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    if(upscale) {
        scaledImage = image.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    scaledImage.setDevicePixelRatio(dpr);

    return scaledImage;
}

QImage scaleImage(const QImage& image, int width, double dpr, bool upscale)
{
    return scaleImage(image, {width, width}, dpr, upscale);
}

QPixmap changePixmapColour(const QPixmap& orig, const QColor& color)
{
    QPixmap pixmap{orig.size()};
    pixmap.fill(color);
    pixmap.setMask(orig.createMaskFromColor(Qt::transparent));
    return pixmap;
}

QMainWindow* getMainWindow()
{
    const auto widgets = QApplication::topLevelWidgets();

    for(QWidget* widget : widgets) {
        if(auto* mainWindow = qobject_cast<QMainWindow*>(widget)) {
            return mainWindow;
        }
    }

    return nullptr;
}

double windowDpr()
{
    double dpr{1.0};

    if(auto* window = getMainWindow()) {
        if(auto* handle = window->windowHandle()) {
            dpr = handle->devicePixelRatio();
        }
    }

    return dpr;
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

int visibleSectionCount(const QHeaderView* headerView)
{
    int visibleCount{0};
    const int count = headerView->count();
    for(int section{0}; section < count; ++section) {
        if(!headerView->isSectionHidden(section)) {
            ++visibleCount;
        }
    }
    return visibleCount;
}

int firstVisualIndex(const QHeaderView* headerView)
{
    int visibleCount{0};
    const int count = headerView->count();
    for(int section{0}; section < count; ++section) {
        const int logical = headerView->logicalIndex(section);
        if(!headerView->isSectionHidden(logical)) {
            return visibleCount;
        }
        ++visibleCount;
    }
    return visibleCount;
}

int realVisualIndex(const QHeaderView* headerView, int logicalIndex)
{
    if(headerView->isSectionHidden(logicalIndex)) {
        return -1;
    }

    int realIndex{0};

    const int count = headerView->count();
    for(int i{0}; i < count; ++i) {
        const int section = headerView->logicalIndex(i);
        if(!headerView->isSectionHidden(section)) {
            if(section == logicalIndex) {
                return realIndex;
            }
            ++realIndex;
        }
    }

    return -1;
}

std::vector<int> logicalIndexOrder(const QHeaderView* headerView)
{
    std::vector<int> indexes;

    const int count = headerView->count();
    for(int section{0}; section < count; ++section) {
        const int logical = headerView->logicalIndex(section);
        if(!headerView->isSectionHidden(logical)) {
            indexes.emplace_back(logical);
        }
    }

    return indexes;
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

QPixmap pixmapFromTheme(const char* icon)
{
    return pixmapFromTheme(icon, {DefaultIconSize, DefaultIconSize});
}

QPixmap pixmapFromTheme(const char* icon, const QSize& size)
{
    const QString key = QStringLiteral("ThemeIcon|%1|%2x%3").arg(QLatin1String{icon}).arg(size.width(), size.height());

    QPixmap pixmap;
    if(QPixmapCache::find(key, &pixmap)) {
        return pixmap;
    }

    pixmap = QIcon::fromTheme(QString::fromLatin1(icon)).pixmap(size);
    QPixmapCache::insert(key, pixmap);

    return pixmap;
}
} // namespace Fooyin::Utils
