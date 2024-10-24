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

namespace {
using ParsedLine = Fooyin::Lyrics::ParsedLine;
using ParsedWord = Fooyin::Lyrics::ParsedWord;

auto lineForTimestamp(const std::vector<ParsedLine>& lines, uint64_t timestamp)
{
    auto line = std::ranges::lower_bound(lines, timestamp, {}, &ParsedLine::endTimestamp);
    if(line == lines.cbegin() && line->timestamp > timestamp) {
        return lines.cend();
    }

    if(line == lines.cend() || line->timestamp > timestamp) {
        --line;
    }

    return line;
}

auto wordForTimestamp(const ParsedLine& line, uint64_t timestamp)
{
    auto word = std::ranges::lower_bound(line.words, timestamp, {}, &ParsedWord::endTimestamp);
    if(word == line.words.cbegin() && word->timestamp > timestamp) {
        return line.words.cend();
    }

    if(word == line.words.cend() || word->timestamp > timestamp) {
        --word;
    }

    return word;
}
} // namespace

namespace Fooyin::Lyrics {
LyricsArea::LyricsArea(SettingsManager* settings, QWidget* parent)
    : QWidget{parent}
    , m_settings{settings}
    , m_resizing{false}
    , m_alignment{m_settings->value<Settings::Lyrics::Alignment>()}
    , m_margins{m_settings->value<Settings::Lyrics::Margins>().value<QMargins>()}
    , m_lineSpacing{m_settings->value<Settings::Lyrics::LineSpacing>()}
    , m_currentTime{0}
    , m_currentLine{-1}
    , m_currentWord{-1}
{
    m_settings->subscribe<Settings::Lyrics::Colours>(this, &LyricsArea::loadSettings);
    m_settings->subscribe<Settings::Lyrics::LineFont>(this, &LyricsArea::loadSettings);
    m_settings->subscribe<Settings::Lyrics::WordLineFont>(this, &LyricsArea::loadSettings);
    m_settings->subscribe<Settings::Lyrics::WordFont>(this, &LyricsArea::loadSettings);
    m_settings->subscribe<Settings::Lyrics::Margins>(this, [this](const QVariant& margins) {
        m_margins = margins.value<QMargins>();
        recalculateArea();
        update();
    });
    m_settings->subscribe<Settings::Lyrics::LineSpacing>(this, [this](const int spacing) {
        m_lineSpacing = spacing;
        recalculateArea();
        update();
    });
    m_settings->subscribe<Settings::Lyrics::Alignment>(this, [this](const int align) {
        m_alignment = static_cast<Qt::Alignment>(align);
        recalculateArea();
        update();
    });

    loadSettings();
}

Lyrics LyricsArea::lyrics() const
{
    return m_lyrics;
}

void LyricsArea::setDisplayString(const QString& string)
{
    m_lyrics        = {};
    m_displayString = string;
    recalculateArea();
    update();
}

void LyricsArea::setLyrics(const Lyrics& lyrics)
{
    m_displayString.clear();

    if(std::exchange(m_lyrics, lyrics) == lyrics) {
        return;
    }

    recalculateArea();
    update();
}

void LyricsArea::setCurrentTime(uint64_t time)
{
    m_currentTime = time;
    highlightCurrentLine();
}

QFont LyricsArea::defaultFont()
{
    return QApplication::font("Fooyin::Lyrics::LyricsArea");
}

QFont LyricsArea::defaultLineFont()
{
    const QFont font{defaultFont()};
    return font;
}

QFont LyricsArea::defaultWordLineFont()
{
    const QFont font{defaultFont()};
    return font;
}

QFont LyricsArea::defaultWordFont()
{
    const QFont font{defaultFont()};
    return font;
}

QSize LyricsArea::sizeHint() const
{
    for(const auto& lineRect : m_lineRects | std::views::reverse) {
        if(!lineRect.empty()) {
            const auto& lastWordRect = lineRect.back();
            const int height         = lastWordRect.y() + lastWordRect.height();
            return {150, height + (m_margins.top() + m_margins.bottom())};
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
        loadSettings();
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
        update();
        m_resizing = false;
    }
    QWidget::timerEvent(event);
}

void LyricsArea::mouseReleaseEvent(QMouseEvent* event)
{
    QWidget::mouseReleaseEvent(event);

    if(!syncedLyrics() || !m_settings->value<Settings::Lyrics::SeekOnClick>()) {
        return;
    }

    const auto pos = event->position().toPoint();

    auto lineIt = std::ranges::find_if(m_lineRects, [pos](const auto& lineRect) {
        return std::ranges::any_of(lineRect, [pos](const auto& wordRect) { return wordRect.contains(pos); });
    });
    if(lineIt == m_lineRects.cend()) {
        return;
    }

    const auto pressedLine = static_cast<int>(std::distance(m_lineRects.begin(), lineIt));
    const auto& lyricLine  = m_lyrics.lines.at(pressedLine);

    if(!syncedWords()) {
        emit linePressed(lyricLine.timestamp);
        return;
    }

    auto wordIt = std::ranges::find_if(*lineIt, [pos](const auto& wordRect) { return wordRect.contains(pos); });
    if(wordIt != lineIt->cend()) {
        const auto pressedWord = static_cast<int>(std::distance(lineIt->begin(), wordIt));
        if(pressedWord >= 0 && std::cmp_less(pressedWord, lyricLine.words.size())) {
            emit linePressed(lyricLine.words.at(pressedWord).timestamp);
        }
    }
}

void LyricsArea::paintEvent(QPaintEvent* event)
{
    QPainter painter{this};
    painter.fillRect(rect(), m_colours.colour(Colours::Type::Background));

    if(!m_displayString.isEmpty()) {
        const QRect boundingRect{m_margins.left(), m_margins.top(), width() - m_margins.right() - m_margins.left(),
                                 height() - m_margins.bottom() - m_margins.top()};
        const QRect textRect = painter.boundingRect(boundingRect, Qt::AlignCenter | Qt::TextWordWrap, m_displayString);
        painter.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, m_displayString);
        return;
    }

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

        QFont font{defaultFont()};
        const bool highlightLine = (m_currentLine == line);
        const bool linePlayed    = (parsedLine.endTimestamp() < m_currentTime);

        for(int word{0}; const auto& parsedWord : parsedLine.words) {
            if(std::cmp_greater_equal(word, lineRects.size())) {
                break;
            }

            const bool highlightWord = highlightLine && syncedWords() && parsedWord.isCurrent(m_currentTime);

            painter.save();
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
                    painter.setPen(m_colours.colour(Colours::Type::LineUnsynced));
                    break;
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

    QFont lineFont{defaultLineFont()};
    const QString lineFontStr = m_settings->value<Settings::Lyrics::LineFont>();
    if(!lineFontStr.isEmpty()) {
        lineFont.fromString(lineFontStr);
    }
    m_lineFont = lineFont;

    QFont wordLineFont{defaultWordLineFont()};
    const QString wordLineFontStr = m_settings->value<Settings::Lyrics::WordLineFont>();
    if(!wordLineFontStr.isEmpty()) {
        wordLineFont.fromString(wordLineFontStr);
    }
    m_wordLineFont = wordLineFont;

    QFont wordFont{defaultWordFont()};
    const QString wordFontStr = m_settings->value<Settings::Lyrics::WordFont>();
    if(!wordFontStr.isEmpty()) {
        wordFont.fromString(wordFontStr);
    }
    m_wordFont = wordFont;

    recalculateArea();
}

void LyricsArea::recalculateArea()
{
    m_lineRects.clear();
    m_lineRects.resize(m_lyrics.lines.size());

    int y{m_margins.top()};
    const int rightEdge{width() - m_margins.right() - m_margins.left()};
    const QRect boundingRect{m_margins.left(), m_margins.top(), rightEdge,
                             height() - m_margins.bottom() - m_margins.top()};

    for(int line{0}; const auto& parsedLine : m_lyrics.lines) {
        int wordX{m_margins.left()};
        int lineHeight{0};

        QFont font{defaultFont()};
        const bool highlightLine = line == m_currentLine || parsedLine.isCurrent(m_currentTime);

        std::vector<QRect> currentLineRects;
        int currentLineWidth{0};

        for(const auto& parsedWord : parsedLine.words) {
            const bool highlightWord = highlightLine && syncedWords() && parsedWord.isCurrent(m_currentTime);

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
            const QRect wordRect = fm.boundingRect(boundingRect, Qt::TextSingleLine, parsedWord.word);

            if(currentLineWidth + wordRect.width() > rightEdge) {
                alignLineRects(currentLineRects, line, wordX, currentLineWidth);
                y += lineHeight + m_lineSpacing;

                currentLineRects.clear();
                currentLineWidth = 0;
                lineHeight       = 0;
            }

            currentLineRects.emplace_back(0, y, wordRect.width(), wordRect.height());
            currentLineWidth += wordRect.width();
            lineHeight = std::max(lineHeight, wordRect.height());
        }

        if(!currentLineRects.empty()) {
            alignLineRects(currentLineRects, line, wordX, currentLineWidth);
            y += lineHeight + m_lineSpacing;
        }

        ++line;
    }

    adjustSize();
}

void LyricsArea::highlightCurrentLine()
{
    if(!syncedLyrics()) {
        return;
    }

    int newLine{-1};
    int newWord{-1};
    bool wordFinished{false};

    auto line = lineForTimestamp(m_lyrics.lines, m_currentTime);
    if(line != m_lyrics.lines.cend()) {
        newLine = static_cast<int>(std::distance(m_lyrics.lines.cbegin(), line));
        if(syncedWords()) {
            auto word = wordForTimestamp(*line, m_currentTime);
            if(word != line->words.cend()) {
                if(word->endTimestamp() > m_currentTime) {
                    wordFinished = true;
                }
                newWord = static_cast<int>(std::distance(line->words.cbegin(), word));
            }
        }
    }

    const auto prevLine    = std::exchange(m_currentLine, newLine);
    const auto prevWord    = std::exchange(m_currentWord, newWord);
    const bool lineChanged = m_currentLine != prevLine;

    if(!lineChanged && (!syncedWords() || (wordFinished && m_currentWord == prevWord))) {
        return;
    }

    setUpdatesEnabled(false);

    const QFont font{defaultFont()};
    if(m_lineFont != font || (syncedWords() && (m_wordLineFont != font || m_wordFont != font))) {
        // We only need to recalculate if the font of the current line/word is different
        recalculateArea();
    }

    QRect updateRect;
    if(prevLine >= 0 && std::cmp_less(prevLine, m_lineRects.size())) {
        updateRect = lineRect(prevLine);
    }

    if(m_currentLine <= 0) {
        emit currentLineChanged(0);
    }
    if(m_currentLine >= 0) {
        const QRect currentRect = lineRect(m_currentLine);
        updateRect              = updateRect.united(currentRect);
        if(lineChanged && currentRect.isValid()) {
            const int y = currentRect.center().y();
            emit currentLineChanged(y);
        }
    }

    if(updateRect.isValid()) {
        update(updateRect);
    }

    setUpdatesEnabled(true);
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
    lineRect.setX(0);
    lineRect.setWidth(width());

    return lineRect;
}

void LyricsArea::alignLineRects(std::vector<QRect>& lineRects, int line, int& wordX, int lineWidth)
{
    const int rightEdge = width() - m_margins.right();

    int lineStartX = 0;
    switch(m_alignment) {
        case(Qt::AlignRight):
            lineStartX = rightEdge - lineWidth;
            break;
        case(Qt::AlignCenter):
            lineStartX = m_margins.left() + (rightEdge - m_margins.right() - lineWidth) / 2;
            break;
        case(Qt::AlignLeft):
        default:
            lineStartX = m_margins.left();
            break;
    }

    wordX = lineStartX;
    for(QRect& rect : lineRects) {
        rect.moveLeft(wordX);
        m_lineRects[line].push_back(rect);
        wordX += rect.width();
    }
}

bool LyricsArea::syncedLyrics() const
{
    return m_lyrics.type == Lyrics::Type::Synced || m_lyrics.type == Lyrics::Type::SyncedWords;
}

bool LyricsArea::syncedWords() const
{
    return m_lyrics.type == Lyrics::Type::SyncedWords;
}
} // namespace Fooyin::Lyrics
