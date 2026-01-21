/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <LukeT1@proton.me>
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

#include "settings/lyricssettings.h"

#include <QSize>

namespace Fooyin::Lyrics {
LyricsModel::LyricsModel(SettingsManager* settings, QObject* parent)
    : QAbstractListModel{parent}
    , m_settings{settings}
    , m_alignment{Qt::AlignCenter}
    , m_lineSpacing{5}
    , m_currentTime{0}
    , m_currentLine{-1}
    , m_currentWord{-1}
    , m_lineFont{Lyrics::defaultFont()}
    , m_wordLineFont{Lyrics::defaultFont()}
    , m_wordFont{Lyrics::defaultFont()}
{
    loadColours();
    loadFonts();

    m_settings->subscribe<Settings::Lyrics::Colours>(this, &LyricsModel::loadColours);
    m_settings->subscribe<Settings::Lyrics::LineFont>(this, &LyricsModel::loadFonts);
    m_settings->subscribe<Settings::Lyrics::WordLineFont>(this, &LyricsModel::loadFonts);
    m_settings->subscribe<Settings::Lyrics::WordFont>(this, &LyricsModel::loadFonts);
}

void LyricsModel::setLyrics(const Lyrics& lyrics)
{
    beginResetModel();

    m_lyrics      = lyrics;
    m_currentLine = -1;
    m_currentWord = -1;
    m_currentTime = 0;

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
        emit dataChanged(index(0, 0), index(0, 0));
        emit dataChanged(index(rowCount({}) - 1, 0), index(rowCount({}) - 1, 0));
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

void LyricsModel::setCurrentTime(uint64_t time)
{
    m_currentTime = time;
    updateCurrentLine();
}

uint64_t LyricsModel::currentTime() const
{
    return m_currentTime;
}

int LyricsModel::currentLineIndex() const
{
    // Return row index (including padding offset)
    if(m_currentLine >= 0) {
        return m_currentLine + 1;
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
    else if(role == Qt::BackgroundRole) {
        return m_colours.colour(Colours::Type::Background);
    }

    if(isPadding) {
        if(role == Qt::SizeHintRole) {
            if(isTopPadding) {
                return QSize(0, m_margins.top());
            }
            return QSize(0, m_margins.bottom());
        }
        return {};
    }

    const int lineIndex = row - 1; // Account for top padding row
    if(lineIndex < 0 || std::cmp_greater_equal(lineIndex, m_lyrics.lines.size())) {
        return {};
    }

    const auto& line = m_lyrics.lines.at(lineIndex);

    switch(role) {
        case(Qt::DisplayRole):
            return line.joinedWords();
        case(Qt::FontRole):
            return Lyrics::defaultFont();
        case(Qt::TextAlignmentRole):
            return QVariant::fromValue(m_alignment);
        case(RichTextRole):
            return QVariant::fromValue(m_text.at(lineIndex));
        case(TimestampRole):
            return QVariant::fromValue(line.timestamp);
        case(WordsRole):
            return QVariant::fromValue(line.words);
        case(MarginsRole):
            return QVariant::fromValue(m_margins);
        case(LineSpacingRole):
            return QVariant::fromValue(m_lineSpacing);
        default:
            return {};
    }
}

void LyricsModel::loadColours()
{
    m_colours = m_settings->value<Settings::Lyrics::Colours>().value<Colours>();

    beginResetModel();

    m_text.clear();

    for(const auto& line : m_lyrics.lines) {
        m_text.push_back(textForLine(line));
    }

    endResetModel();
}

void LyricsModel::loadFonts()
{
    const QString lineFontStr = m_settings->value<Settings::Lyrics::LineFont>();
    if(!lineFontStr.isEmpty()) {
        m_lineFont.fromString(lineFontStr);
    }
    else {
        m_lineFont = Lyrics::defaultLineFont();
    }

    const QString wordLineFontStr = m_settings->value<Settings::Lyrics::WordLineFont>();
    if(!wordLineFontStr.isEmpty()) {
        m_wordLineFont.fromString(wordLineFontStr);
    }
    else {
        m_wordLineFont = Lyrics::defaultWordLineFont();
    }

    const QString wordFontStr = m_settings->value<Settings::Lyrics::WordFont>();
    if(!wordFontStr.isEmpty()) {
        m_wordFont.fromString(wordFontStr);
    }
    else {
        m_wordFont = Lyrics::defaultWordFont();
    }

    updateCurrentLine();
}

void LyricsModel::updateCurrentLine()
{
    const bool isSynced = m_lyrics.type == Lyrics::Type::Synced || m_lyrics.type == Lyrics::Type::SyncedWords;

    if(!isSynced) {
        return;
    }

    int newLine{-1};
    int newWord{-1};

    const auto line = lineForTimestamp(m_currentTime);
    if(line != m_lyrics.lines.cend()) {
        newLine = static_cast<int>(std::distance(m_lyrics.lines.cbegin(), line));

        if(m_lyrics.type == Lyrics::Type::SyncedWords) {
            const auto word = wordForTimestamp(*line, m_currentTime);

            if(word != line->words.cend()) {
                newWord = static_cast<int>(std::distance(line->words.cbegin(), word));
            }
        }
    }

    const int prevLine = std::exchange(m_currentLine, newLine);
    const int prevWord = std::exchange(m_currentWord, newWord);

    if(newLine >= 0) {
        auto& text = m_text[newLine];
        text       = textForLine(*line);
    }
    if(prevLine >= 0) {
        auto& text = m_text[prevLine];
        text       = textForLine(m_lyrics.lines.at(prevLine));
    }

    if(prevLine != m_currentLine || prevWord != m_currentWord) {
        if(prevLine >= 0 && std::cmp_less(prevLine, m_lyrics.lines.size())) {
            const int prevRow = prevLine + 1; // padding
            emit dataChanged(index(prevRow, 0), index(prevRow, 0), {RichTextRole});
        }

        if(m_currentLine >= 0 && std::cmp_less(m_currentLine, m_lyrics.lines.size())) {
            const int currentRow = m_currentLine + 1; // padding
            emit dataChanged(index(currentRow, 0), index(currentRow, 0), {RichTextRole});
        }
    }
}

RichText LyricsModel::textForLine(const ParsedLine& line) const
{
    const bool highlightLine = line.isCurrent(m_currentTime);
    const bool linePlayed    = (line.endTimestamp() < m_currentTime);

    RichText formattedText;

    for(const auto& parsedWord : line.words) {
        const bool highlightWord
            = highlightLine && m_lyrics.type == Lyrics::Type::SyncedWords && parsedWord.isCurrent(m_currentTime);

        RichTextBlock block;
        block.text = parsedWord.word;

        RichFormatting format;

        format.font   = Lyrics::defaultFont();
        format.colour = m_colours.colour(linePlayed ? Colours::Type::LinePlayed : Colours::Type::LineUnplayed);

        switch(m_lyrics.type) {
            case(Lyrics::Type::Synced): {
                if(highlightLine) {
                    format.font   = m_lineFont;
                    format.colour = m_colours.colour(Colours::Type::LineSynced);
                }
                break;
            }
            case(Lyrics::Type::SyncedWords): {
                if(highlightWord) {
                    format.font   = m_wordFont;
                    format.colour = m_colours.colour(Colours::Type::WordSynced);
                }
                else if(highlightLine) {
                    format.font   = m_wordLineFont;
                    format.colour = m_colours.colour(Colours::Type::WordLineSynced);
                }
                break;
            }
            case(Lyrics::Type::Unsynced):
                format.colour = m_colours.colour(Colours::Type::LineUnsynced);
                break;
            case(Lyrics::Type::Unknown):
                break;
        }

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

std::vector<ParsedWord>::const_iterator LyricsModel::wordForTimestamp(const Fooyin::Lyrics::ParsedLine& line,
                                                                      uint64_t timestamp) const
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
