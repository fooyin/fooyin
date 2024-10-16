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

#include "lyricsarea.h"

#include "settings/lyricssettings.h"

#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QPaintEvent>
#include <QPainter>
#include <QTimerEvent>

#include <ranges>

using namespace std::chrono_literals;

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
constexpr auto ResizeInterval = 50ms;
#else
constexpr auto ResizeInterval = 50;
#endif

namespace Fooyin::Lyrics {
LyricsArea::LyricsArea(SettingsManager* settings, QWidget* parent)
    : QWidget{parent}
    , m_settings{settings}
    , m_resizing{false}
    , m_alignment{m_settings->value<Settings::Lyrics::Alignment>()}
    , m_verticalMargin{20}
    , m_horizontalMargin{20}
    , m_lineHeight{30}
    , m_totalHeight{0}
    , m_currentTime{0}
    , m_currentLine{-1}
    , m_currentWord{-1}
{
    m_settings->subscribe<Settings::Lyrics::Colours>(this, &LyricsArea::loadSettings);
    m_settings->subscribe<Settings::Lyrics::LineFont>(this, &LyricsArea::loadSettings);
    m_settings->subscribe<Settings::Lyrics::WordLineFont>(this, &LyricsArea::loadSettings);
    m_settings->subscribe<Settings::Lyrics::WordFont>(this, &LyricsArea::loadSettings);
    m_settings->subscribe<Settings::Lyrics::Alignment>(this, [this](const int align) {
        m_alignment = static_cast<Qt::Alignment>(align);
        recalculateArea();
        adjustSize();
        update();
    });

    loadSettings();
}

void LyricsArea::setLyrics(const Lyrics& lyrics)
{
    m_lyrics = lyrics;
    recalculateArea();
    adjustSize();
    update();
}

void LyricsArea::setCurrentTime(uint64_t time)
{
    m_currentTime = time;
    highlightCurrentLine();
}

QFont LyricsArea::defaultLineFont()
{
    const QFont font = QApplication::font("Fooyin::Lyrics::LyricsArea");
    return font;
}

QFont LyricsArea::defaultWordLineFont()
{
    const QFont font = QApplication::font("Fooyin::Lyrics::LyricsArea");
    return font;
}

QFont LyricsArea::defaultWordFont()
{
    const QFont font = QApplication::font("Fooyin::Lyrics::LyricsArea");
    return font;
}

QSize LyricsArea::sizeHint() const
{
    for(const auto& lineRect : m_lineRects | std::views::reverse) {
        if(!lineRect.empty()) {
            const auto& lastWordRect = lineRect.back();
            const int height         = lastWordRect.y() + lastWordRect.height();
            return {150, height + (2 * m_verticalMargin)};
        }
    }

    return {150, 100};
}

QSize LyricsArea::minimumSizeHint() const
{
    return sizeHint();
}

void LyricsArea::changeEvent(QEvent* event)
{
    if(event->type() == QEvent::FontChange) {
        recalculateArea();
        update();
    }
    QWidget::changeEvent(event);
}

void LyricsArea::resizeEvent(QResizeEvent* event)
{
    if(m_resizing) {
        return;
    }

    QWidget::resizeEvent(event);

    m_resizeTimer.start(ResizeInterval, this);
}

void LyricsArea::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_resizeTimer.timerId()) {
        m_resizing = true;
        m_resizeTimer.stop();
        recalculateArea();
        adjustSize();
        update();
        m_resizing = false;
    }
    QWidget::timerEvent(event);
}

void LyricsArea::paintEvent(QPaintEvent* event)
{
    QPainter painter{this};
    painter.fillRect(rect(), m_colours.colour(Colours::Type::Background));

    const QRect visibleRect = event->rect();

    for(int line{0}; const auto& parsedLine : m_lyrics.lines) {
        if(std::cmp_greater_equal(line, m_lineRects.size())) {
            break;
        }

        const auto& lineRects = m_lineRects.at(line);

        if(std::ranges::all_of(lineRects,
                               [&visibleRect](const QRect& wordRect) { return !wordRect.intersects(visibleRect); })) {
            // No need to paint lines not visible
            ++line;
            continue;
        }

        QFont font               = painter.font();
        const bool highlightLine = (line == m_currentLine);
        const bool linePlayed    = (parsedLine.timestamp + parsedLine.duration < m_currentTime);

        for(int word{0}; const auto& parsedWord : parsedLine.words) {
            if(std::cmp_greater_equal(word, lineRects.size())) {
                break;
            }

            painter.save();

            const bool highlightWord = highlightLine && m_lyrics.type == Lyrics::Type::SyncedWords
                                    && parsedWord.duration > 0 && (m_currentTime >= parsedWord.timestamp)
                                    && (m_currentTime < parsedWord.timestamp + parsedWord.duration);

            painter.setPen(m_colours.colour(linePlayed ? Colours::Type::LinePlayed : Colours::Type::LineUnplayed));

            switch(m_lyrics.type) {
                case(Lyrics::Type::Synced): {
                    if(highlightLine) {
                        font = m_lineFont;
                        painter.setPen(m_colours.colour(Colours::Type::LineSynced));
                    }
                    break;
                }
                case(Lyrics::Type::SyncedWords): {
                    if(highlightWord) {
                        font = m_wordFont;
                        painter.setPen(m_colours.colour(Colours::Type::WordSynced));
                    }
                    else if(highlightLine) {
                        font = m_wordLineFont;
                        painter.setPen(m_colours.colour(Colours::Type::WordLineSynced));
                    }
                    break;
                }
                case(Lyrics::Type::Unsynced):
                case(Lyrics::Type::Unknown):
                    break;
            }

            painter.setFont(font);

            const QRect& wordRect = lineRects.at(word++);
            const auto flags      = static_cast<int>(m_alignment | Qt::AlignVCenter | Qt::TextWordWrap);
            painter.drawText(wordRect, flags, parsedWord.word);

            painter.restore();
        }
        ++line;
    }
}

void LyricsArea::loadSettings()
{
    m_colours = m_settings->value<Settings::Lyrics::Colours>().value<Colours>();

    QFont lineFont{LyricsArea::defaultLineFont()};
    const QString lineFontStr = m_settings->value<Settings::Lyrics::LineFont>();
    if(!lineFontStr.isEmpty()) {
        lineFont.fromString(lineFontStr);
    }
    m_lineFont = lineFont;

    QFont wordLineFont{LyricsArea::defaultWordLineFont()};
    const QString wordLineFontStr = m_settings->value<Settings::Lyrics::WordLineFont>();
    if(!wordLineFontStr.isEmpty()) {
        wordLineFont.fromString(wordLineFontStr);
    }
    m_wordLineFont = wordLineFont;

    QFont wordFont{LyricsArea::defaultWordFont()};
    const QString wordFontStr = m_settings->value<Settings::Lyrics::WordLineFont>();
    if(!wordFontStr.isEmpty()) {
        wordFont.fromString(wordFontStr);
    }
    m_wordFont = wordFont;

    recalculateArea();
}

void LyricsArea::recalculateArea(int startingLine)
{
    int y{m_verticalMargin};

    if(startingLine <= 0) {
        startingLine = 0;
        m_lineRects.clear();
        m_lineRects.resize(m_lyrics.lines.size());
    }
    else {
        const auto startRext = lineRect(startingLine);
        if(startRext.isValid()) {
            y = startRext.y();
        }
    }

    const int rightEdge{width() - m_horizontalMargin};

    for(int line{startingLine}; const auto& parsedLine : m_lyrics.lines | std::views::drop(startingLine)) {
        int wordX{m_horizontalMargin};
        int lineHeight{m_lineHeight};

        QFont font = this->font();
        const bool highlightLine
            = (m_currentTime >= parsedLine.timestamp) && (m_currentTime < parsedLine.timestamp + parsedLine.duration);

        std::vector<QRect> currentLineRects;
        int currentLineWidth{0};

        for(const auto& parsedWord : parsedLine.words) {
            const bool highlightWord = highlightLine && m_lyrics.type == Lyrics::Type::SyncedWords
                                    && (m_currentTime >= parsedWord.timestamp)
                                    && (m_currentTime < parsedWord.timestamp + parsedWord.duration);

            switch(m_lyrics.type) {
                case(Lyrics::Type::Synced): {
                    if(highlightLine) {
                        font = m_lineFont;
                    }
                    break;
                }
                case(Lyrics::Type::SyncedWords): {
                    if(highlightWord) {
                        font = m_wordFont;
                    }
                    else if(highlightLine) {
                        font = m_wordLineFont;
                    }
                    break;
                }
                case(Lyrics::Type::Unsynced):
                case(Lyrics::Type::Unknown):
                    break;
            }

            const QFontMetrics fm{font};
            QRect wordRect = fm.boundingRect(QRect{0, 0, rightEdge - m_horizontalMargin, m_lineHeight},
                                             Qt::TextSingleLine, parsedWord.word);
            wordRect.setHeight(std::max(wordRect.height(), m_lineHeight));

            if(currentLineWidth + wordRect.width() > rightEdge - m_horizontalMargin) {
                alignLineRects(currentLineRects, line, wordX, currentLineWidth);
                y += lineHeight;

                currentLineRects.clear();
                currentLineWidth = 0;
                lineHeight       = m_lineHeight;
            }

            currentLineRects.emplace_back(0, y, wordRect.width(), wordRect.height());
            currentLineWidth += wordRect.width();
            lineHeight = std::max(lineHeight, wordRect.height());
        }

        if(!currentLineRects.empty()) {
            alignLineRects(currentLineRects, line, wordX, currentLineWidth);
            y += lineHeight;
        }

        ++line;
    }
}

std::vector<ParsedLine>::const_iterator LyricsArea::lineForTimestamp(uint64_t timestamp) const
{
    auto it = std::lower_bound(m_lyrics.lines.cbegin(), m_lyrics.lines.cend(), timestamp,
                               [](const ParsedLine& line, uint64_t ts) { return line.timestamp + line.duration < ts; });

    if(it != m_lyrics.lines.cbegin() && (it == m_lyrics.lines.cend() || it->timestamp > timestamp)) {
        --it;
    }

    return it;
}

std::vector<ParsedWord>::const_iterator LyricsArea::wordForTimestamp(const ParsedLine& line, uint64_t timestamp) const
{
    auto it = std::lower_bound(line.words.cbegin(), line.words.cend(), timestamp,
                               [](const ParsedWord& word, uint64_t ts) { return word.timestamp + word.duration < ts; });

    if(it != line.words.cbegin() && (it == line.words.cend() || it->timestamp > timestamp)) {
        --it;
    }

    return it;
}

void LyricsArea::highlightCurrentLine()
{
    if(m_lyrics.type == Lyrics::Type::Unsynced) {
        return;
    }

    int newLine{-1};
    int newWord{-1};
    bool wordFinished{false};

    auto line = lineForTimestamp(m_currentTime);
    if(line != m_lyrics.lines.cend()) {
        newLine   = static_cast<int>(std::distance(m_lyrics.lines.cbegin(), line));
        auto word = wordForTimestamp(*line, m_currentTime);
        if(word != line->words.cend()) {
            if(word->timestamp + word->duration < m_currentTime) {
                wordFinished = true;
            }
            newWord = static_cast<int>(std::distance(line->words.cbegin(), word));
        }
    }

    const auto prevLine = std::exchange(m_currentLine, newLine);
    const auto prevWord = std::exchange(m_currentWord, newWord);

    if(m_currentLine == prevLine
       && (!wordFinished && (m_lyrics.type != Lyrics::Type::SyncedWords || m_currentWord == prevWord))) {
        return;
    }

    if(prevLine >= 0 && std::cmp_less(prevLine, m_lineRects.size())) {
        update(lineRect(prevLine));
    }

    recalculateArea(std::min(prevLine, m_currentLine));

    const QRect updateRect = lineRect(m_currentLine);
    if(updateRect.isValid()) {
        const int y = updateRect.center().y();
        emit currentLineChanged(y);
        update(updateRect);
    }
}

QRect LyricsArea::lineRect(int line) const
{
    if(line < 0 || std::cmp_greater_equal(line, m_lineRects.size())) {
        return {};
    }

    QRect lineRect;

    const auto& wordRects = m_lineRects.at(line);
    for(const auto& wordRect : wordRects) {
        lineRect = lineRect.united(wordRect);
    }

    return lineRect;
}

void LyricsArea::alignLineRects(std::vector<QRect>& lineRects, int line, int& wordX, int lineWidth)
{
    const int rightEdge = width() - m_horizontalMargin;

    int lineStartX = 0;
    switch(m_alignment) {
        case(Qt::AlignLeft):
            lineStartX = m_horizontalMargin;
            break;
        case(Qt::AlignRight):
            lineStartX = rightEdge - lineWidth;
            break;
        case(Qt::AlignCenter):
            lineStartX = m_horizontalMargin + (rightEdge - m_horizontalMargin - lineWidth) / 2;
            break;
        default:
            lineStartX = m_horizontalMargin;
            break;
    }

    wordX = lineStartX;
    for(QRect& rect : lineRects) {
        rect.moveLeft(wordX);
        m_lineRects[line].push_back(rect);
        wordX += rect.width();
    }
}
} // namespace Fooyin::Lyrics
