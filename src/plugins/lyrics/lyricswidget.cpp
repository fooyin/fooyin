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

#include "lyricswidget.h"

#include "lyricsarea.h"
#include "lyricsconstants.h"
#include "lyricsparser.h"

#include <core/player/playercontroller.h>
#include <core/scripting/scriptparser.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMenu>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QScrollBar>
#include <QStringDecoder>
#include <QTimerEvent>
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(LYRICS_WIDGET, "fy.lyrics")

using namespace std::chrono_literals;

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
constexpr auto ScrollTimeout = 100ms;
#else
constexpr auto ScrollTimeout = 100;
#endif

namespace {
QByteArray toUtf8(QIODevice* file)
{
    const QByteArray data = file->readAll();
    if(data.isEmpty()) {
        return {};
    }

    QStringDecoder toUtf16;

    auto encoding = QStringConverter::encodingForData(data);
    if(encoding) {
        toUtf16 = QStringDecoder{encoding.value()};
    }
    else {
        const auto encodingName = Fooyin::Utils::detectEncoding(data);
        if(encodingName.isEmpty()) {
            return {};
        }

        encoding = QStringConverter::encodingForName(encodingName.constData());
        if(encoding) {
            toUtf16 = QStringDecoder{encoding.value()};
        }
        else {
            toUtf16 = QStringDecoder{encodingName.constData()};
        }
    }

    if(!toUtf16.isValid()) {
        toUtf16 = QStringDecoder{QStringConverter::Utf8};
    }

    QString string = toUtf16(data);
    string.replace(QLatin1String{"\n\n"}, QLatin1String{"\n"});
    string.replace(u'\r', u'\n');

    return string.toUtf8();
}

using LyricPaths = QStringList;
QString findDirectoryLyrics(const LyricPaths& paths, const Fooyin::Track& track)
{
    if(!track.isValid()) {
        return {};
    }

    Fooyin::ScriptParser parser;

    QStringList filters;

    for(const QString& path : paths) {
        filters.emplace_back(parser.evaluate(path.trimmed(), track));
    }

    for(const auto& filter : filters) {
        const QFileInfo fileInfo{QDir::cleanPath(filter)};
        const QDir filePath{fileInfo.path()};
        const QString filePattern  = fileInfo.fileName();
        const QStringList fileList = filePath.entryList({filePattern}, QDir::Files);

        if(!fileList.isEmpty()) {
            return filePath.absoluteFilePath(fileList.constFirst());
        }
    }

    return {};
}
} // namespace

namespace Fooyin::Lyrics {
class LyricsScrollArea : public QScrollArea
{
    Q_OBJECT

public:
    using QScrollArea::QScrollArea;

signals:
    void scrolling();

protected:
    void wheelEvent(QWheelEvent* event) override
    {
        emit scrolling();
        QScrollArea::wheelEvent(event);
    }
};

LyricsWidget::LyricsWidget(PlayerController* playerController, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_playerController{playerController}
    , m_settings{settings}
    , m_scrollArea{new LyricsScrollArea(this)}
    , m_lyricsArea{new LyricsArea(m_settings, this)}
    , m_scrollMode{static_cast<ScrollMode>(m_settings->value<Settings::Lyrics::ScrollMode>())}
    , m_isUserScrolling{false}
{
    setObjectName(LyricsWidget::name());

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_scrollArea);

    m_scrollArea->setVerticalScrollBarPolicy(
        !m_settings->value<Settings::Lyrics::ShowScrollbar>() ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
    m_scrollArea->setMinimumWidth(150);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setWidget(m_lyricsArea);

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, &LyricsWidget::updateLyrics);
    QObject::connect(m_playerController, &PlayerController::positionChanged, m_lyricsArea, &LyricsArea::setCurrentTime);
    QObject::connect(m_playerController, &PlayerController::positionMoved, this,
                     qOverload<uint64_t>(&LyricsWidget::checkStartAutoScrollPos));
    QObject::connect(m_lyricsArea, &LyricsArea::linePressed, m_playerController, &PlayerController::seek);
    QObject::connect(m_lyricsArea, &LyricsArea::currentLineChanged, this, &LyricsWidget::scrollToCurrentLine);
    QObject::connect(m_scrollArea, &LyricsScrollArea::scrolling, this, [this]() {
        m_isUserScrolling = true;
        if(m_scrollAnim) {
            m_scrollAnim->stop();
        }
        m_scrollTimer.start(ScrollTimeout, this);
    });
    QObject::connect(m_scrollArea->verticalScrollBar(), &QScrollBar::sliderPressed, this,
                     [this]() { m_isUserScrolling = true; });
    QObject::connect(m_scrollArea->verticalScrollBar(), &QScrollBar::sliderReleased, this,
                     [this]() { m_isUserScrolling = false; });
    QObject::connect(m_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        if(m_isUserScrolling && m_scrollAnim) {
            m_scrollAnim->stop();
        }
    });

    m_settings->subscribe<Settings::Lyrics::NoLyricsScript>(
        this, [this]() { updateLyrics(m_playerController->currentTrack()); });
    m_settings->subscribe<Settings::Lyrics::ScrollMode>(
        this, [this](const int mode) { updateScrollMode(static_cast<ScrollMode>(mode)); });
    m_settings->subscribe<Settings::Lyrics::ShowScrollbar>(this, [this](const bool show) {
        m_scrollArea->setVerticalScrollBarPolicy(!show ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
    });

    updateLyrics(m_playerController->currentTrack());
}

QString LyricsWidget::name() const
{
    return tr("Lyrics");
}

QString LyricsWidget::layoutName() const
{
    return QStringLiteral("Lyrics");
}

void LyricsWidget::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_scrollTimer.timerId()) {
        m_scrollTimer.stop();
        m_isUserScrolling = false;
        checkStartAutoScroll(m_scrollArea->verticalScrollBar()->value());
    }
    FyWidget::timerEvent(event);
}

void LyricsWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* showScrollbar = new QAction(tr("Show scrollbar"), menu);
    showScrollbar->setCheckable(true);
    showScrollbar->setChecked(m_settings->value<Settings::Lyrics::ShowScrollbar>());
    QObject::connect(showScrollbar, &QAction::triggered, this,
                     [this](const bool checked) { m_settings->set<Settings::Lyrics::ShowScrollbar>(checked); });

    auto* alignMenu = new QMenu(tr("Text-align"), menu);

    auto* alignmentGroup = new QActionGroup(menu);

    auto* alignCenter = new QAction(tr("Align to centre"), alignmentGroup);
    auto* alignLeft   = new QAction(tr("Align to left"), alignmentGroup);
    auto* alignRight  = new QAction(tr("Align to right"), alignmentGroup);

    alignCenter->setCheckable(true);
    alignLeft->setCheckable(true);
    alignRight->setCheckable(true);

    const auto currentAlignment = m_settings->value<Settings::Lyrics::Alignment>();

    alignCenter->setChecked(currentAlignment == Qt::AlignCenter);
    alignLeft->setChecked(currentAlignment == Qt::AlignLeft);
    alignRight->setChecked(currentAlignment == Qt::AlignRight);

    auto changeAlignment = [this](Qt::Alignment alignment) {
        m_settings->set<Settings::Lyrics::Alignment>(static_cast<int>(alignment));
    };

    QObject::connect(alignCenter, &QAction::triggered, this, [changeAlignment]() { changeAlignment(Qt::AlignCenter); });
    QObject::connect(alignLeft, &QAction::triggered, this, [changeAlignment]() { changeAlignment(Qt::AlignLeft); });
    QObject::connect(alignRight, &QAction::triggered, this, [changeAlignment]() { changeAlignment(Qt::AlignRight); });

    alignMenu->addAction(alignCenter);
    alignMenu->addAction(alignLeft);
    alignMenu->addAction(alignRight);

    auto* openSettings = new QAction(tr("Settings…"), menu);
    QObject::connect(openSettings, &QAction::triggered, this,
                     [this]() { m_settings->settingsDialog()->openAtPage(Constants::Page::LyricsGeneral); });

    menu->addAction(showScrollbar);
    menu->addMenu(alignMenu);
    menu->addSeparator();
    menu->addAction(openSettings);

    menu->popup(event->globalPos());
}

void LyricsWidget::updateLyrics(const Track& track)
{
    if(m_scrollAnim) {
        m_scrollAnim->stop();
    }
    m_scrollArea->verticalScrollBar()->setValue(0);

    if(!track.isValid()) {
        return;
    }

    const auto showNoLyrics = [this, &track]() {
        const auto script = m_settings->value<Settings::Lyrics::NoLyricsScript>();
        m_lyricsArea->setDisplayString(m_parser.evaluate(script, track));
    };

    const auto searchTags = m_settings->value<Settings::Lyrics::SearchTags>();
    for(const QString& tag : searchTags) {
        if(track.hasExtraTag(tag)) {
            const QString lyricsStr = track.extraTag(tag).constFirst();
            if(!lyricsStr.isEmpty()) {
                const Lyrics lyrics = parse(lyricsStr);
                m_lyricsArea->setLyrics(lyrics);
                m_type = lyrics.type;
                checkStartAutoScroll(0);
                return;
            }
        }
    }

    const QString dirLrc = findDirectoryLyrics(m_settings->value<Settings::Lyrics::Paths>(), track);
    if(dirLrc.isEmpty()) {
        showNoLyrics();
        return;
    }

    QFile lrcFile{dirLrc};
    if(!lrcFile.open(QIODevice::ReadOnly)) {
        qCInfo(LYRICS_WIDGET) << "Could not open file" << dirLrc << "for reading:" << lrcFile.errorString();
        showNoLyrics();
        return;
    }

    const QByteArray lrc = toUtf8(&lrcFile);
    if(!lrc.isEmpty()) {
        const Lyrics lyrics = parse(lrc);
        m_lyricsArea->setLyrics(lyrics);
        m_type = lyrics.type;
        checkStartAutoScroll(0);
    }
    else {
        showNoLyrics();
    }
}

void LyricsWidget::scrollToCurrentLine(int scrollValue)
{
    if(m_isUserScrolling || m_scrollMode == ScrollMode::Manual) {
        return;
    }

    auto* scrollbar       = m_scrollArea->verticalScrollBar();
    const int targetValue = std::clamp(scrollValue - (m_scrollArea->height() / 2), 0, scrollbar->maximum());

    const int scrollDuration = m_settings->value<Settings::Lyrics::ScrollDuration>();
    if(scrollDuration == 0) {
        scrollbar->setValue(targetValue);
        return;
    }

    m_scrollAnim = new QPropertyAnimation(scrollbar, "value", this);
    m_scrollAnim->setDuration(scrollDuration);
    m_scrollAnim->setEasingCurve(QEasingCurve::OutCubic);
    m_scrollAnim->setStartValue(scrollbar->value());
    m_scrollAnim->setEndValue(targetValue);

    m_scrollAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

void LyricsWidget::updateScrollMode(ScrollMode mode)
{
    m_scrollMode = mode;
    if(m_scrollMode != ScrollMode::Automatic && m_scrollAnim) {
        m_scrollAnim->stop();
    }
    else if(m_scrollMode == ScrollMode::Automatic) {
        checkStartAutoScroll(m_scrollArea->verticalScrollBar()->value());
    }
}

void LyricsWidget::checkStartAutoScrollPos(uint64_t pos)
{
    if(m_isUserScrolling) {
        return;
    }

    if(m_type == Lyrics::Type::Unsynced && m_scrollMode == ScrollMode::Automatic) {
        const int maxScroll   = m_scrollArea->verticalScrollBar()->maximum();
        const auto duration   = static_cast<int>(m_playerController->currentTrack().duration());
        const auto startValue = static_cast<int>((static_cast<double>(pos) / duration) * maxScroll);
        updateAutoScroll(startValue);
    }
}

void LyricsWidget::checkStartAutoScroll(int startValue)
{
    if(m_isUserScrolling) {
        return;
    }

    if(m_type == Lyrics::Type::Unsynced && m_scrollMode == ScrollMode::Automatic) {
        updateAutoScroll(startValue);
    }
}

void LyricsWidget::updateAutoScroll(int startValue)
{
    if(m_isUserScrolling) {
        return;
    }

    auto* scrollbar = m_scrollArea->verticalScrollBar();

    scrollbar->setValue(startValue >= 0 ? startValue : scrollbar->value());

    const int maxScroll = scrollbar->maximum();
    if(scrollbar->value() == maxScroll) {
        return;
    }

    if(m_scrollAnim) {
        m_scrollAnim->stop();
    }

    m_scrollAnim = new QPropertyAnimation(scrollbar, "value", this);
    m_scrollAnim->setDuration(
        static_cast<int>(m_playerController->currentTrack().duration() - m_playerController->currentPosition()));
    m_scrollAnim->setEasingCurve(QEasingCurve::Linear);
    m_scrollAnim->setStartValue(scrollbar->value());
    m_scrollAnim->setEndValue(maxScroll);

    m_scrollAnim->start(QAbstractAnimation::DeleteWhenStopped);
}
} // namespace Fooyin::Lyrics

#include "lyricswidget.moc"
#include "moc_lyricswidget.cpp"
