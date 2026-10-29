/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <luket@pm.me>
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

#include <QAction>
#include <QApplication>
#include <QHeaderView>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QPointer>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QScreen>
#include <QSettings>
#include <QStatusTipEvent>
#include <QWidget>
#include <QWindow>

using namespace Qt::StringLiterals;

constexpr auto StatusTipsForwardedProp = "fooyin_statusTipsForwarded";

namespace {
std::optional<int> parseDigits(QStringView value, qsizetype position, qsizetype count)
{
    const auto end = position + count;

    if(end > value.size()) {
        return {};
    }

    int result{0};
    for(qsizetype i{position}; i < end; ++i) {
        const char16_t character = value.at(i).unicode();
        if(character < u'0' || character > u'9') {
            return {};
        }
        result = (result * 10) + (character - u'0');
    }
    return result;
}
} // namespace

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

uint64_t currentDateToInt()
{
    const auto str = QDateTime::currentDateTimeUtc().toString("yyyyMMddHHmmss"_L1);
    return static_cast<uint64_t>(str.toULongLong());
}

QString formatTimeMs(uint64_t time)
{
    const QDateTime dateTime  = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(time));
    QString formattedDateTime = dateTime.toString("yyyy-MM-dd HH:mm:ss"_L1);
    return formattedDateTime;
}

std::optional<ParsedDateTime> parseDateTime(QStringView value)
{
    const auto size = value.size();

    DateTimePrecision precision;
    switch(size) {
        case 4:
            precision = DateTimePrecision::Year;
            break;
        case 7:
            precision = DateTimePrecision::Month;
            break;
        case 10:
            precision = DateTimePrecision::Day;
            break;
        case 13:
            precision = DateTimePrecision::Hour;
            break;
        case 16:
            precision = DateTimePrecision::Minute;
            break;
        case 19:
            precision = DateTimePrecision::Second;
            break;
        default:
            return {};
    }

    // clang-format off
    if((size >= 7 && value.at(4) != '-'_L1) ||
       (size >= 10 && value.at(7) != '-'_L1) ||
       (size >= 13 && value.at(10) != ' '_L1) ||
       (size >= 16 && value.at(13) != ':'_L1) ||
       (size >= 19 && value.at(16) != ':'_L1)) {
        return {};
    }
    // clang-format on

    const auto year   = parseDigits(value, 0, 4);
    const auto month  = size >= 7 ? parseDigits(value, 5, 2) : 1;
    const auto day    = size >= 10 ? parseDigits(value, 8, 2) : 1;
    const auto hour   = size >= 13 ? parseDigits(value, 11, 2) : 0;
    const auto minute = size >= 16 ? parseDigits(value, 14, 2) : 0;
    const auto second = size >= 19 ? parseDigits(value, 17, 2) : 0;
    if(!year || !month || !day || !hour || !minute || !second) {
        return {};
    }

    const QDate date{*year, *month, *day};
    const QTime time{*hour, *minute, *second};
    if(!date.isValid() || !time.isValid()) {
        return {};
    }

    return ParsedDateTime{.date = date, .time = time, .precision = precision};
}

QDateTime dateStringToDate(const QString& str)
{
    const auto parsed = parseDateTime(str);
    return parsed ? parsed->toDateTime() : QDateTime{};
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
    QString formattedDateTime = dateTime.toString("yyyy-MM-dd HH:mm:ss"_L1);
    return formattedDateTime;
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

QStringList extensionsToWildcards(const QStringList& extensions)
{
    QStringList wildcards;
    std::ranges::transform(extensions, std::back_inserter(wildcards),
                           [](const QString& extension) { return u"*.%1"_s.arg(extension); });
    return wildcards;
}

QString extensionsToFilterList(const QStringList& extensions, const QString& name)
{
    QStringList filterList;

    for(const QString& ext : extensions) {
        filterList.append(u"%1 %2 (*.%3)"_s.arg(ext, name, ext));
    }

    return filterList.join(";;"_L1);
}

QString extensionFromFilter(const QString& filter)
{
    static const QRegularExpression re{uR"(\*\.(\w+))"_s};
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

QSize proportionateSize(const QWidget* widget, double widthFactor, double heightFactor)
{
    const QPoint center = widget->geometry().center();
    QScreen* screen     = QGuiApplication::screenAt(center);
    if(!screen) {
        screen = QGuiApplication::primaryScreen();
    }

    const QSize screenSize = screen->availableGeometry().size();
    const int width        = static_cast<int>(screenSize.width() * widthFactor);
    const int height       = static_cast<int>(screenSize.height() * heightFactor);

    return {width, height};
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

void forwardMenuStatusTips(QMenu* menu, QWidget* target)
{
    if(!menu || menu->property(StatusTipsForwardedProp).toBool()) {
        return;
    }
    menu->setProperty(StatusTipsForwardedProp, true);

    QObject::connect(menu, &QMenu::hovered, menu, [target = QPointer{target}](QAction* action) {
        if(action) {
            action->showStatusText(target ? target.data() : getMainWindow());
        }
    });
    QObject::connect(menu, &QMenu::aboutToHide, menu, [target = QPointer{target}]() {
        QStatusTipEvent event{QString{}};
        QApplication::sendEvent(target ? target.data() : getMainWindow(), &event);
    });
}

QAction* cloneMenuAction(QMenu* menu, const QAction* source, const std::function<void()>& handler)
{
    auto* action = new QAction(source->icon(), source->text(), menu);
    action->setStatusTip(source->statusTip());
    action->setEnabled(source->isEnabled());
    action->setVisible(source->isVisible());
    QObject::connect(action, &QAction::triggered, menu, handler);
    return action;
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

void saveState(QWidget* widget, QSettings& settings, const QString& name)
{
    const QString keyGroup    = !name.isEmpty() ? name : widget->windowTitle().remove(" "_L1);
    const QString geometryKey = QStringLiteral("%1/Geometry").arg(keyGroup);
    const QString sizeKey     = QStringLiteral("%1/Size").arg(keyGroup);

    settings.setValue(geometryKey, widget->saveGeometry());
    settings.setValue(sizeKey, widget->size());

    if(auto* window = qobject_cast<QMainWindow*>(widget)) {
        const QString stateKey = QStringLiteral("%1/State").arg(keyGroup);
        settings.setValue(stateKey, window->saveState());
    }
}

void restoreState(QWidget* widget, const QSettings& settings, const QString& name)
{
    const QString keyGroup    = !name.isEmpty() ? name : widget->windowTitle().remove(" "_L1);
    const QString geometryKey = QStringLiteral("%1/Geometry").arg(keyGroup);
    const QString sizeKey     = QStringLiteral("%1/Size").arg(keyGroup);

    if(settings.contains(geometryKey)) {
        if(const QByteArray geometry = settings.value(geometryKey).toByteArray(); !geometry.isEmpty()) {
            widget->restoreGeometry(geometry);
        }
    }

    if(auto* window = qobject_cast<QMainWindow*>(widget)) {
        const QString stateKey = QStringLiteral("%1/State").arg(keyGroup);
        const QByteArray state = settings.value(stateKey).toByteArray();
        if(!state.isEmpty()) {
            window->restoreState(state);
        }
    }

    if(settings.contains(sizeKey)) {
        if(const QSize size = settings.value(sizeKey).toSize(); size.isValid()) {
            widget->resize(size);
        }
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

} // namespace Fooyin::Utils
