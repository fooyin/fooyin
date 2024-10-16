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
#include "lyricsparser.h"
#include "settings/lyricssettings.h"

#include <core/player/playercontroller.h>
#include <core/scripting/scriptparser.h>
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
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(LYRICS_WIDGET, "fy.lyrics")

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
LyricsWidget::LyricsWidget(PlayerController* playerController, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_playerController{playerController}
    , m_settings{settings}
    , m_scrollArea{new QScrollArea(this)}
    , m_lyricsArea{new LyricsArea(m_settings, this)}
{
    setObjectName(LyricsWidget::name());

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_scrollArea);

    m_scrollArea->setMinimumWidth(150);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setWidget(m_lyricsArea);

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, &LyricsWidget::updateLyrics);
    QObject::connect(m_playerController, &PlayerController::positionChanged, m_lyricsArea, &LyricsArea::setCurrentTime);
    QObject::connect(m_lyricsArea, &LyricsArea::currentLineChanged, this, &LyricsWidget::scrollToCurrentLine);

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

void LyricsWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

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
    menu->addMenu(alignMenu);

    menu->popup(event->globalPos());
}

void LyricsWidget::updateLyrics(const Track& track)
{
    m_scrollArea->verticalScrollBar()->setValue(0);

    if(!track.isValid()) {
        return;
    }

    const auto searchTags = m_settings->value<Settings::Lyrics::SearchTags>();
    for(const QString& tag : searchTags) {
        if(track.hasExtraTag(tag)) {
            const QString lyrics = track.extraTag(tag).constFirst();
            if(!lyrics.isEmpty()) {
                m_lyricsArea->setLyrics(parse(lyrics));
                return;
            }
        }
    }

    const QString dirLrc = findDirectoryLyrics(m_settings->value<Settings::Lyrics::Paths>(), track);
    if(dirLrc.isEmpty()) {
        m_lyricsArea->setLyrics({});
        return;
    }

    QFile lrcFile{dirLrc};
    if(!lrcFile.open(QIODevice::ReadOnly)) {
        qCInfo(LYRICS_WIDGET) << "Could not open file" << dirLrc << "for reading:" << lrcFile.errorString();
        m_lyricsArea->setLyrics({});
        return;
    }

    const QByteArray lrc = toUtf8(&lrcFile);
    if(!lrc.isEmpty()) {
        m_lyricsArea->setLyrics(parse(lrc));
    }
    else {
        m_lyricsArea->setLyrics({});
    }
}

void LyricsWidget::scrollToCurrentLine(int scrollValue)
{
    auto* scrollbar       = m_scrollArea->verticalScrollBar();
    const int targetValue = std::clamp(scrollValue - (m_scrollArea->height() / 2), 0, scrollbar->maximum());

    const int scrollDuration = m_settings->value<Settings::Lyrics::ScrollDuration>();
    if(scrollDuration == 0) {
        scrollbar->setValue(targetValue);
        return;
    }

    auto* animation = new QPropertyAnimation(scrollbar, "value", this);
    animation->setDuration(scrollDuration);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->setStartValue(scrollbar->value());
    animation->setEndValue(targetValue);

    animation->start(QAbstractAnimation::DeleteWhenStopped);
}
} // namespace Fooyin::Lyrics
