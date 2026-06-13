/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <luket@pm.me>
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

#include "lyricsmodel.h"

#include <QSize>

#include <algorithm>
#include <utility>

constexpr uint64_t ProgressUpdateIntervalMs = 33;

namespace Fooyin::Lyrics {
LyricsModel::LyricsModel(GuiStyleProvider* styleProvider, QObject* parent)
    : QAbstractListModel{parent}
    , m_topViewportPadding{0}
    , m_bottomViewportPadding{0}
    , m_alignment{Qt::AlignCenter}
    , m_lineSpacing{5}
    , m_currentTime{0}
    , m_lastProgressUpdateTime{0}
    , m_currentLine{-1}
    , m_currentLineEnd{-1}
    , m_currentWord{-1}
    , m_styleProvider{styleProvider}
    , m_baseFont{Lyrics::defaultFont(*m_styleProvider)}
    , m_lineFont{Lyrics::defaultLineFont(*m_styleProvider)}
    , m_wordLineFont{Lyrics::defaultWordLineFont(*m_styleProvider)}
    , m_wordFont{Lyrics::defaultWordFont(*m_styleProvider)}
    , m_progressMode{ProgressMode::Off}
{
    setColours(Colours{});
    setFonts({}, {}, {}, {});
}

void LyricsModel::setLyrics(const Lyrics& lyrics)
{
    beginResetModel();

    m_lyrics                 = lyrics;
    m_currentLine            = -1;
    m_currentLineEnd         = -1;
    m_currentWord            = -1;
    m_currentTime            = 0;
    m_lastProgressUpdateTime = 0;

    m_text.clear();

    for(const auto& line : m_lyrics.lines) {
        m_text.push_back(textForLine(line));
    }

    endResetModel();
}

Lyrics LyricsModel::lyrics() const
{
    return m_lyrics;
}

void LyricsModel::setMargins(const QMargins& margins)
{
    if(m_margins == margins) {
        return;
    }

    m_margins = margins;

    // Update padding rows
    if(rowCount({}) > 0) {
        Q_EMIT dataChanged(index(0, 0), index(0, 0));
        Q_EMIT dataChanged(index(rowCount({}) - 1, 0), index(rowCount({}) - 1, 0));
    }
}

void LyricsModel::setViewportPadding(int topPadding, int bottomPadding)
{
    topPadding    = std::max(topPadding, 0);
    bottomPadding = std::max(bottomPadding, 0);
    if(m_topViewportPadding == topPadding && m_bottomViewportPadding == bottomPadding) {
        return;
    }

    m_topViewportPadding    = topPadding;
    m_bottomViewportPadding = bottomPadding;

    if(rowCount({}) > 0) {
        Q_EMIT dataChanged(index(0, 0), index(0, 0));
        Q_EMIT dataChanged(index(rowCount({}) - 1, 0), index(rowCount({}) - 1, 0));
    }
}

void LyricsModel::setAlignment(Qt::Alignment alignment)
{
    m_alignment = alignment;
}

void LyricsModel::setLineSpacing(int spacing)
{
    m_lineSpacing = spacing;
}

void LyricsModel::setColours(const Colours& colours)
{
    m_colours = colours;

    beginResetModel();

    m_text.clear();
    for(const auto& line : m_lyrics.lines) {
        m_text.push_back(textForLine(line));
    }

    endResetModel();
}

void LyricsModel::setFonts(const QString& baseFont, const QString& lineFont, const QString& wordLineFont,
                           const QString& wordFont)
{
    if(baseFont.isEmpty() || !m_baseFont.fromString(baseFont)) {
        m_baseFont = Lyrics::defaultFont(*m_styleProvider);
    }

    if(lineFont.isEmpty() || !m_lineFont.fromString(lineFont)) {
        m_lineFont = Lyrics::defaultLineFont(*m_styleProvider);
    }

    if(wordLineFont.isEmpty() || !m_wordLineFont.fromString(wordLineFont)) {
        m_wordLineFont = Lyrics::defaultWordLineFont(*m_styleProvider);
    }

    if(wordFont.isEmpty() || !m_wordFont.fromString(wordFont)) {
        m_wordFont = Lyrics::defaultWordFont(*m_styleProvider);
    }

    beginResetModel();

    m_text.clear();
    for(const auto& line : m_lyrics.lines) {
        m_text.push_back(textForLine(line));
    }

    endResetModel();
}

void LyricsModel::setProgressMode(ProgressMode mode)
{
    if(std::exchange(m_progressMode, mode) == mode) {
        return;
    }

    if(m_currentLine >= 0) {
        for(int lineIndex{m_currentLine}; lineIndex <= m_currentLineEnd; ++lineIndex) {
            m_text[lineIndex] = textForLine(m_lyrics.lines.at(lineIndex));
        }

        Q_EMIT dataChanged(index(m_currentLine + 1, 0), index(m_currentLineEnd + 1, 0),
                           {RichTextRole, CurrentTimeRole, ProgressColourRole, ProgressBaseColourRole});
    }
}

void LyricsModel::setCurrentTime(uint64_t time)
{
    m_currentTime = time;

    const int prevLine    = m_currentLine;
    const int prevLineEnd = m_currentLineEnd;

    updateCurrentLine();

    if(!shouldFillProgress() || m_currentLine < 0 || prevLine != m_currentLine || prevLineEnd != m_currentLineEnd) {
        m_lastProgressUpdateTime = time;
        return;
    }

    const uint64_t timeDiff
        = time > m_lastProgressUpdateTime ? time - m_lastProgressUpdateTime : m_lastProgressUpdateTime - time;
    if(timeDiff < ProgressUpdateIntervalMs) {
        return;
    }

    m_lastProgressUpdateTime = time;
    Q_EMIT dataChanged(index(m_currentLine + 1, 0), index(m_currentLineEnd + 1, 0), {CurrentTimeRole});
}

uint64_t LyricsModel::currentTime() const
{
    return m_currentTime;
}

int LyricsModel::currentLineIndex() const
{
    if(m_currentLine >= 0) {
        return m_currentLine + 1;
    }

    return -1;
}

int LyricsModel::currentLineLastIndex() const
{
    if(m_currentLineEnd >= 0) {
        return m_currentLineEnd + 1;
    }

    return -1;
}

int LyricsModel::rowCount(const QModelIndex& parent) const
{
    if(parent.isValid()) {
        return 0;
    }

    if(m_lyrics.lines.empty()) {
        return 0;
    }

    // Add padding rows
    return static_cast<int>(m_lyrics.lines.size()) + 2;
}

QVariant LyricsModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid() || index.row() < 0 || index.row() >= rowCount({})) {
        return {};
    }

    const int row              = index.row();
    const bool isTopPadding    = (row == 0);
    const bool isBottomPadding = (row == rowCount({}) - 1);
    const bool isPadding       = isTopPadding || isBottomPadding;

    if(role == IsPaddingRole) {
        return isPadding;
    }
    if(role == Qt::BackgroundRole) {
        return m_colours.colour(Colours::Type::Background);
    }

    if(isPadding) {
        if(role == Qt::SizeHintRole) {
            if(isTopPadding) {
                return QSize(0, m_margins.top() + m_topViewportPadding);
            }
            return QSize(0, m_margins.bottom() + m_bottomViewportPadding);
        }
        return {};
    }

    const int lineIndex = row - 1; // Account for top padding row
    if(lineIndex < 0 || std::cmp_greater_equal(lineIndex, m_lyrics.lines.size())) {
        return {};
    }

    const auto& line = m_lyrics.lines.at(lineIndex);

    switch(role) {
        case Qt::DisplayRole:
            return line.joinedWords();
        case Qt::FontRole:
            return Lyrics::defaultFont(*m_styleProvider);
        case Qt::TextAlignmentRole:
            return QVariant::fromValue(m_alignment);
        case RichTextRole:
            return QVariant::fromValue(m_text.at(lineIndex));
        case TimestampRole:
            return QVariant::fromValue(line.timestamp);
        case DurationRole:
            return QVariant::fromValue(line.duration);
        case WordsRole:
            return QVariant::fromValue(line.words);
        case MarginsRole:
            return QVariant::fromValue(m_margins);
        case LineSpacingRole:
            return QVariant::fromValue(m_lineSpacing);
        case CurrentTimeRole:
            return QVariant::fromValue(m_currentTime);
        case LyricsTypeRole:
            return QVariant::fromValue(static_cast<int>(m_lyrics.type));
        case ProgressColourRole:
            if(!shouldFillProgress()) {
                return {};
            }
            if(m_lyrics.type == Lyrics::Type::SyncedWords) {
                return m_colours.colour(Colours::Type::WordSynced);
            }
            return m_colours.colour(Colours::Type::LineSynced);
        case ProgressBaseColourRole:
            if(!shouldFillProgress()) {
                return {};
            }
            if(m_lyrics.type == Lyrics::Type::SyncedWords) {
                return m_colours.colour(Colours::Type::WordLineSynced);
            }
            return m_colours.colour(Colours::Type::LineUnplayed);
        default:
            return {};
    }
}

void LyricsModel::updateCurrentLine()
{
    const bool isSynced = m_lyrics.type == Lyrics::Type::Synced || m_lyrics.type == Lyrics::Type::SyncedWords;

    if(!isSynced) {
        return;
    }

    int newLine{-1};
    int newLineEnd{-1};
    int newWord{-1};

    const auto line = lineForTimestamp(m_currentTime);
    if(line != m_lyrics.lines.cend()) {
        newLine = static_cast<int>(std::distance(m_lyrics.lines.cbegin(), line));

        while(newLine > 0 && m_lyrics.lines.at(newLine - 1).timestamp == line->timestamp) {
            --newLine;
        }

        newLineEnd = newLine;
        while(std::cmp_less(newLineEnd + 1, m_lyrics.lines.size())
              && m_lyrics.lines.at(newLineEnd + 1).timestamp == line->timestamp) {
            ++newLineEnd;
        }

        if(m_lyrics.type == Lyrics::Type::SyncedWords) {
            const auto word = wordForTimestamp(*line, m_currentTime);

            if(word != line->words.cend()) {
                newWord = static_cast<int>(std::distance(line->words.cbegin(), word));
            }
        }
    }

    const int prevLine    = std::exchange(m_currentLine, newLine);
    const int prevLineEnd = std::exchange(m_currentLineEnd, newLineEnd);
    const int prevWord    = std::exchange(m_currentWord, newWord);

    if(prevLine != m_currentLine || prevLineEnd != m_currentLineEnd || prevWord != m_currentWord) {
        int changedStart{-1};
        int changedEnd{-1};

        if(prevLine >= 0) {
            changedStart = prevLine;
            changedEnd   = prevLineEnd;
        }

        if(m_currentLine >= 0) {
            changedStart = changedStart < 0 ? m_currentLine : std::min(changedStart, m_currentLine);
            changedEnd   = std::max(changedEnd, m_currentLineEnd);
        }

        if(changedStart >= 0 && std::cmp_less(changedEnd, m_lyrics.lines.size())) {
            for(int lineIndex{changedStart}; lineIndex <= changedEnd; ++lineIndex) {
                m_text[lineIndex] = textForLine(m_lyrics.lines.at(lineIndex));
            }

            Q_EMIT dataChanged(index(changedStart + 1, 0), index(changedEnd + 1, 0), {RichTextRole});
        }
    }
}

bool LyricsModel::isLineHighlighted(const ParsedLine& line) const
{
    return m_currentLine >= 0 && std::cmp_less(m_currentLine, m_lyrics.lines.size())
        && line.timestamp == m_lyrics.lines.at(m_currentLine).timestamp;
}

RichText LyricsModel::textForLine(const ParsedLine& line) const
{
    const bool highlightLine = isLineHighlighted(line);
    const bool linePlayed    = !highlightLine && line.timestamp < m_currentTime;

    RichText formattedText;

    for(const auto& parsedWord : line.words) {
        const bool highlightWord
            = highlightLine && m_lyrics.type == Lyrics::Type::SyncedWords && parsedWord.isCurrent(m_currentTime);
        const bool playedWordInHighlightedLine
            = shouldFillWordProgress() && highlightLine && m_lyrics.type == Lyrics::Type::SyncedWords
           && parsedWord.timestamp < m_currentTime && !parsedWord.isCurrent(m_currentTime);

        RichFormatting format;

        format.font = m_baseFont;
        format.colour.setColour(m_colours.colour(linePlayed ? Colours::Type::LinePlayed : Colours::Type::LineUnplayed));

        switch(m_lyrics.type) {
            case Lyrics::Type::Synced: {
                if(highlightLine) {
                    format.font = m_lineFont;
                    format.colour.setColour(m_colours.colour(Colours::Type::LineSynced));
                }
                break;
            }
            case Lyrics::Type::SyncedWords: {
                if(playedWordInHighlightedLine) {
                    format.font = m_wordLineFont;
                    format.colour.setColour(m_colours.colour(Colours::Type::WordSynced));
                }
                else if(highlightWord) {
                    format.font = m_wordFont;
                    format.colour.setColour(m_colours.colour(Colours::Type::WordSynced));
                }
                else if(highlightLine) {
                    format.font = m_wordLineFont;
                    format.colour.setColour(m_colours.colour(Colours::Type::WordLineSynced));
                }
                break;
            }
            case Lyrics::Type::Unsynced:
                format.colour.setColour(m_colours.colour(Colours::Type::LineUnsynced));
                break;
            case Lyrics::Type::Unknown:
                break;
        }

        RichTextBlock block;
        block.text   = parsedWord.word;
        block.format = format;
        formattedText.blocks.push_back(block);
    }

    return formattedText;
}

std::vector<ParsedLine>::const_iterator LyricsModel::lineForTimestamp(uint64_t timestamp) const
{
    auto line = std::ranges::lower_bound(m_lyrics.lines, timestamp, {}, &Fooyin::Lyrics::ParsedLine::endTimestamp);
    if(line == m_lyrics.lines.cbegin() && line->timestamp > timestamp) {
        return m_lyrics.lines.cend();
    }

    if(line == m_lyrics.lines.cend() || line->timestamp > timestamp) {
        --line;
    }

    return line;
}

bool LyricsModel::shouldFillLineProgress() const
{
    return m_lyrics.type == Lyrics::Type::Synced
        && (m_progressMode == ProgressMode::SyncedLines || m_progressMode == ProgressMode::AllSynced);
}

bool LyricsModel::shouldFillWordProgress() const
{
    return m_lyrics.type == Lyrics::Type::SyncedWords
        && (m_progressMode == ProgressMode::SyncedWords || m_progressMode == ProgressMode::AllSynced);
}

bool LyricsModel::shouldFillProgress() const
{
    return shouldFillLineProgress() || shouldFillWordProgress();
}

std::vector<ParsedWord>::const_iterator LyricsModel::wordForTimestamp(const ParsedLine& line, uint64_t timestamp) const
{
    auto word = std::ranges::lower_bound(line.words, timestamp, {}, &Fooyin::Lyrics::ParsedWord::endTimestamp);
    if(word == line.words.cbegin() && word->timestamp > timestamp) {
        return line.words.cend();
    }

    if(word == line.words.cend() || word->timestamp > timestamp) {
        --word;
    }

    return word;
}
} // namespace Fooyin::Lyrics
