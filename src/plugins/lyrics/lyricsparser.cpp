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

#include "lyricsparser.h"

#include <QBuffer>
#include <QLoggingCategory>

#include <set>

Q_LOGGING_CATEGORY(PARSER, "fy.lyricsparser")

using namespace Qt::StringLiterals;

// This tries to parse most unsynced/synced lyric formats
// Tokenising each line is a little overkill, but it makes it easier to extend in the future

namespace {
enum TokenType : uint8_t
{
    TokError         = 0,
    TokEos           = 1,
    TokTag           = 2,
    TokOffset        = 3,
    TokComment       = 4,
    TokText          = 5,
    TokTimestamp     = 6,
    TokWordTimestamp = 7,
};

struct Token
{
    TokenType type{TokError};
    QString value;
    uint64_t timestamp{0};
};

struct LineContext
{
    const QChar* start{nullptr};
    const QChar* current{nullptr};

    [[nodiscard]] bool isAtEnd() const
    {
        return *current == u'\0';
    }

    [[nodiscard]] QChar peek() const
    {
        return *current;
    }

    QChar advance()
    {
        std::advance(current, 1);
        return *std::prev(current);
    }

    void consume()
    {
        start = current;
    }
};

bool isLiteral(QChar ch)
{
    switch(ch.unicode()) {
        case(u'['):
        case(u']'):
        case(u'<'):
        case(u'>'):
        case(u'\0'):
            return false;
        default:
            return true;
    }
}

void parseTag(Fooyin::Lyrics::Lyrics& lyrics, LineContext& context)
{
    while(context.peek() != u']' && !context.isAtEnd()) {
        context.advance();
    }

    const auto tag = QStringView{context.start, context.current - context.start}.toString();

    if(context.advance() != u']') {
        return;
    }

    const QStringList parts = tag.split(u':');
    if(parts.size() < 2) {
        return;
    }

    const QString& field = parts.at(0);
    const QString& value = parts.at(1);

    if(field == "ti"_L1) {
        lyrics.metadata.title = value;
    }
    else if(field == "ar"_L1) {
        lyrics.metadata.artist = value;
    }
    else if(field == "al"_L1) {
        lyrics.metadata.album = value;
    }
    else if(field == "au"_L1) {
        lyrics.metadata.author = value;
    }
    else if(field == "length"_L1) {
        lyrics.metadata.length = value;
    }
    else if(field == "by"_L1) {
        lyrics.metadata.lrcAuthor = value;
    }
    else if(field == "offset"_L1) {
        QString offsetStr{value};
        if(value.size() > 1 && value.startsWith("+"_L1)) {
            offsetStr = offsetStr.sliced(1);
        }
        lyrics.offset = offsetStr.toUInt();
    }
    else if(field == "re"_L1 || field == "tool"_L1) {
        lyrics.metadata.tool = value;
    }
    else if(field == "ve"_L1) {
        lyrics.metadata.version = value;
    }
}

Token makeToken(LineContext& context, TokenType type)
{
    Token token;
    token.type  = type;
    token.value = QStringView{context.start, context.current - context.start}.toString();
    return token;
}

Token timestamp(Fooyin::Lyrics::Lyrics& lyrics, LineContext& context)
{
    while(context.peek() != u']' && context.peek() != u'>' && !context.isAtEnd()) {
        context.advance();
    }

    const auto timestamp = QStringView{context.start, context.current - context.start}.toString();

    const bool isWord = context.advance() == u'>';

    if(context.peek().isSpace()) {
        context.advance();
    }

    const QStringList parts = timestamp.split(u':', Qt::SkipEmptyParts);
    if(parts.size() < 2) {
        return {};
    }

    const QStringList secondParts = parts.at(1).split(u'.', Qt::SkipEmptyParts);
    if(secondParts.size() < 2) {
        return {};
    }

    const uint64_t minutes = parts.at(0).toUInt();
    const uint64_t seconds = secondParts.at(0).toUInt();

    uint64_t milliseconds = secondParts.at(1).toUInt();
    if(secondParts.at(1).length() < 3) {
        milliseconds *= 10;
    }

    uint64_t time = (minutes * 60 + seconds) * 1000 + milliseconds;
    time += lyrics.offset;

    Token token;
    token.type      = isWord ? TokWordTimestamp : TokTimestamp;
    token.timestamp = time;
    return token;
}

Token block(Fooyin::Lyrics::Lyrics& lyrics, LineContext& context)
{
    context.consume();

    if(context.peek().isDigit()) {
        return timestamp(lyrics, context);
    }

    parseTag(lyrics, context);

    return {};
}

Token text(LineContext& context)
{
    while(isLiteral(context.peek()) && !context.isAtEnd()) {
        context.advance();
    }

    return makeToken(context, TokText);
}

Token scanNext(Fooyin::Lyrics::Lyrics& lyrics, LineContext& context)
{
    context.start = context.current;

    QChar c = context.advance();

    switch(c.unicode()) {
        case(u'['):
        case(u'<'):
            return block(lyrics, context);
        case(u'\0'):
            return makeToken(context, TokEos);
        default:
            break;
    }

    return text(context);
}

void sortWordsByTimestamp(std::vector<Fooyin::Lyrics::ParsedWord>& words)
{
    std::stable_sort(words.begin(), words.end(),
                     [](const Fooyin::Lyrics::ParsedWord& a, const Fooyin::Lyrics::ParsedWord& b) {
                         return a.timestamp < b.timestamp;
                     });
}

void sortLinesByTimestamp(Fooyin::Lyrics::Lyrics& lyrics)
{
    std::stable_sort(lyrics.lines.begin(), lyrics.lines.end(),
                     [](const Fooyin::Lyrics::ParsedLine& a, const Fooyin::Lyrics::ParsedLine& b) {
                         return a.timestamp < b.timestamp;
                     });

    for(auto& line : lyrics.lines) {
        sortWordsByTimestamp(line.words);
    }
}

void finaliseLines(Fooyin::Lyrics::Lyrics& lyrics)
{
    sortLinesByTimestamp(lyrics);

    auto& lines = lyrics.lines;
    if(lines.size() < 2) {
        return;
    }

    auto it   = lines.begin();
    auto next = std::next(it);

    while(next != lines.end()) {
        if(next->timestamp > it->timestamp) {
            it->duration = next->timestamp - it->timestamp;
        }
        ++it;
        ++next;
    }

    if(lyrics.type != Fooyin::Lyrics::Lyrics::Type::Unsynced) {
        it->duration = 0;
    }
}

void addParsedWord(Fooyin::Lyrics::ParsedLine& parsedLine, Fooyin::Lyrics::ParsedWord& parsedWord,
                   uint64_t nextTimestamp)
{
    if(nextTimestamp > parsedWord.timestamp) {
        parsedWord.duration = nextTimestamp - parsedWord.timestamp;
    }
    parsedLine.words.emplace_back(parsedWord);
    parsedWord = {};
}

Fooyin::Lyrics::ParsedLine splitLine(Fooyin::Lyrics::ParsedLine& parsedLine, Fooyin::Lyrics::ParsedWord& parsedWord)
{
    Fooyin::Lyrics::ParsedLine lyricsLine{parsedLine};

    uint64_t wordTime{0};
    QStringList splitWords;
    if(parsedWord.word.isEmpty() && lyricsLine.words.size() == 1) {
        wordTime = lyricsLine.words.front().timestamp;
        splitWords.append(lyricsLine.words.front().word.split(u' '));
        lyricsLine.words.clear();
    }
    else {
        splitWords = parsedWord.word.split(u' ');
        wordTime   = parsedWord.timestamp;
    }

    for(const QString& splitWord : std::as_const(splitWords)) {
        lyricsLine.words.emplace_back(splitWord + u' ', wordTime);
    }

    return lyricsLine;
};

void parseLine(Fooyin::Lyrics::Lyrics& lyrics, const QString& line)
{
    LineContext context{line.cbegin(), line.cbegin()};

    std::vector<Token> tokens;

    while(!context.isAtEnd()) {
        const Token token = scanNext(lyrics, context);
        if(token.type != TokError && token.type != TokEos) {
            tokens.emplace_back(token);
        }
    }

    if(tokens.empty()) {
        // Most likely unsynced lyrics
        tokens.emplace_back(TokText);
    }

    if(tokens.empty()) {
        return;
    }

    std::set<uint64_t> timestamps;
    Fooyin::Lyrics::ParsedLine parsedLine;
    Fooyin::Lyrics::ParsedWord parsedWord;

    for(const auto& token : tokens) {
        if(token.type == TokEos) {
            break;
        }

        if(token.type == TokWordTimestamp) {
            // A2/Enhanced: [00:00.00] <00:00.04> When <00:00.16> the <00:00.82> truth
            lyrics.type = Fooyin::Lyrics::Lyrics::Type::SyncedWords;

            if(!parsedWord.word.isNull()) {
                if(!timestamps.empty()) {
                    parsedLine.timestamp = *timestamps.cbegin();
                    timestamps.clear();
                }
                addParsedWord(parsedLine, parsedWord, token.timestamp);
            }
            parsedWord.timestamp = token.timestamp;
        }
        else if(token.type == TokTimestamp) {
            if(parsedWord.word.isNull()) {
                // Line-by-line: [00:21.10][00:45.10]Repeating lyrics
                if(lyrics.type == Fooyin::Lyrics::Lyrics::Type::Unknown) {
                    lyrics.type = Fooyin::Lyrics::Lyrics::Type::Synced;
                }
                timestamps.emplace(token.timestamp);
            }
            else {
                // Word-by-word: [01:47.18]每[01:48.09][01:48.09]当[01:48.44]
                lyrics.type = Fooyin::Lyrics::Lyrics::Type::SyncedWords;

                if(!timestamps.empty()) {
                    parsedWord.timestamp = *timestamps.cbegin();
                    timestamps.clear();
                }
                if(parsedLine.words.empty()) {
                    parsedLine.timestamp = parsedWord.timestamp;
                }
                addParsedWord(parsedLine, parsedWord, token.timestamp);
            }
        }
        else if(token.type == TokText) {
            parsedWord.word = token.value;
        }
    }

    // We always split into words so we have more control over wrapping
    Fooyin::Lyrics::ParsedLine lyricsLine = splitLine(parsedLine, parsedWord);
    if(!timestamps.empty()) {
        for(const auto timestamp : timestamps) {
            lyricsLine.timestamp = timestamp;
            lyrics.lines.push_back(lyricsLine);
        }
    }
    else {
        lyrics.lines.push_back(lyricsLine);
    }
}
} // namespace

namespace Fooyin::Lyrics {
Lyrics parse(const QString& text)
{
    if(text.isEmpty()) {
        return {};
    }

    const QByteArray bytes{text.toUtf8()};
    return parse(bytes);
}

Lyrics parse(const QByteArray& text)
{
    if(text.isEmpty()) {
        return {};
    }

    Lyrics lyrics;

    QByteArray data{text};
    QBuffer buffer(&data);
    if(!buffer.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCInfo(PARSER) << "Can't open buffer for reading:" << buffer.errorString();
        return {};
    }

    while(!buffer.atEnd()) {
        QString line = QString::fromUtf8(buffer.readLine()).trimmed();
        line.replace("&apos;"_L1, "'"_L1);
        parseLine(lyrics, line);
    }

    finaliseLines(lyrics);

    if(lyrics.type == Fooyin::Lyrics::Lyrics::Type::Unknown) {
        lyrics.type = Fooyin::Lyrics::Lyrics::Type::Unsynced;
    }

    return lyrics;
}

QString formatTimestamp(uint64_t timestampMs)
{
    const uint64_t minutes    = timestampMs / 60000;
    const uint64_t seconds    = (timestampMs % 60000) / 1000;
    const uint64_t hundredths = (timestampMs % 1000) / 10;

    return u"%1:%2.%3"_s.arg(minutes, 2, 10, QLatin1Char{'0'})
        .arg(seconds, 2, 10, QLatin1Char{'0'})
        .arg(hundredths, 2, 10, QLatin1Char{'0'});
}

uint64_t timestampToMs(const QString& timestamp)
{
    QString fixedTimestamp{timestamp};
    fixedTimestamp.remove(u'[');
    fixedTimestamp.remove(u']');

    const QStringList parts = fixedTimestamp.split(u':', Qt::SkipEmptyParts);
    if(parts.size() < 2) {
        return {};
    }

    const QStringList secondParts = parts.at(1).split(u'.', Qt::SkipEmptyParts);
    if(secondParts.size() < 2) {
        return {};
    }

    const uint64_t minutes = parts.at(0).toUInt();
    const uint64_t seconds = secondParts.at(0).toUInt();

    uint64_t milliseconds = secondParts.at(1).toUInt();
    if(secondParts.at(1).length() < 3) {
        milliseconds *= 10;
    }

    const uint64_t time = (minutes * 60 + seconds) * 1000 + milliseconds;
    return time;
}
} // namespace Fooyin::Lyrics
