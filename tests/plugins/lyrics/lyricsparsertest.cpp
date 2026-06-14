/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include <gtest/gtest.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
namespace {
Lyrics::Lyrics parseLyrics(const QString& offsetTag)
{
    return Lyrics::parse(u"%1\n[00:10.00]First\n[00:12.00]Second"_s.arg(offsetTag));
}

QStringList wordsForLine(const Lyrics::ParsedLine& line)
{
    QStringList words;
    for(const auto& word : line.words) {
        words.push_back(word.word);
    }
    return words;
}
} // namespace

TEST(LyricsParserTest, ParsesUnsyncedText)
{
    const auto lyrics = Lyrics::parse(u"First line\nSecond line"_s);

    ASSERT_EQ(lyrics.type, Lyrics::Lyrics::Type::Unsynced);
    ASSERT_EQ(lyrics.lines.size(), 2);
    EXPECT_EQ(lyrics.lines.at(0).timestamp, 0);
    EXPECT_EQ(lyrics.lines.at(0).joinedWords(), u"First line "_s);
    EXPECT_EQ(lyrics.lines.at(1).timestamp, 0);
    EXPECT_EQ(lyrics.lines.at(1).joinedWords(), u"Second line "_s);
}

TEST(LyricsParserTest, HandlesBlankLines)
{
    const auto lyrics = Lyrics::parse(u"\r"_s);

    ASSERT_EQ(lyrics.type, Lyrics::Lyrics::Type::Unsynced);
    ASSERT_EQ(lyrics.lines.size(), 1);
    EXPECT_EQ(lyrics.lines.at(0).timestamp, 0);
    EXPECT_EQ(lyrics.lines.at(0).joinedWords(), u" "_s);
}

TEST(LyricsParserTest, ParsesStandardLrcMetadataAndLines)
{
    const auto lyrics = Lyrics::parse(u"[ti:Song]\n[ar:Artist]\n[al:Album]\n"
                                      "[00:01.00]Hello world\n"
                                      "[00:03.50]Next line"_s);

    ASSERT_EQ(lyrics.type, Lyrics::Lyrics::Type::Synced);
    EXPECT_EQ(lyrics.metadata.title, u"Song"_s);
    EXPECT_EQ(lyrics.metadata.artist, u"Artist"_s);
    EXPECT_EQ(lyrics.metadata.album, u"Album"_s);

    ASSERT_EQ(lyrics.lines.size(), 2);
    EXPECT_EQ(lyrics.lines.at(0).timestamp, 1000);
    EXPECT_EQ(lyrics.lines.at(0).duration, 2500);
    EXPECT_EQ(wordsForLine(lyrics.lines.at(0)), QStringList({u"Hello "_s, u"world "_s}));
    EXPECT_EQ(lyrics.lines.at(1).timestamp, 3500);
    EXPECT_EQ(lyrics.lines.at(1).duration, 0);
    EXPECT_EQ(wordsForLine(lyrics.lines.at(1)), QStringList({u"Next "_s, u"line "_s}));
}

TEST(LyricsParserTest, ParsesRepeatedStandardLrcTimestamps)
{
    const auto lyrics = Lyrics::parse(u"[00:01.00][00:03.00]Repeat me"_s);

    ASSERT_EQ(lyrics.type, Lyrics::Lyrics::Type::Synced);
    ASSERT_EQ(lyrics.lines.size(), 2);
    EXPECT_EQ(lyrics.lines.at(0).timestamp, 1000);
    EXPECT_EQ(lyrics.lines.at(0).duration, 2000);
    EXPECT_EQ(lyrics.lines.at(0).joinedWords(), u"Repeat me "_s);
    EXPECT_EQ(lyrics.lines.at(1).timestamp, 3000);
    EXPECT_EQ(lyrics.lines.at(1).duration, 0);
    EXPECT_EQ(lyrics.lines.at(1).joinedWords(), u"Repeat me "_s);
}

TEST(LyricsParserTest, SortsStandardLrcLinesByTimestamp)
{
    const auto lyrics = Lyrics::parse(u"[00:03.00]Second\n[00:01.00]First"_s);

    ASSERT_EQ(lyrics.type, Lyrics::Lyrics::Type::Synced);
    ASSERT_EQ(lyrics.lines.size(), 2);
    EXPECT_EQ(lyrics.lines.at(0).timestamp, 1000);
    EXPECT_EQ(lyrics.lines.at(0).joinedWords(), u"First "_s);
    EXPECT_EQ(lyrics.lines.at(1).timestamp, 3000);
    EXPECT_EQ(lyrics.lines.at(1).joinedWords(), u"Second "_s);
}

TEST(LyricsParserTest, ParsesEnhancedLrcWordTimestamps)
{
    const auto lyrics = Lyrics::parse(u"[00:01.00]<00:01.10>Hello <00:01.60>world\n"
                                      "[00:03.00]<00:03.20>Next <00:03.80>line"_s);

    ASSERT_EQ(lyrics.type, Lyrics::Lyrics::Type::SyncedWords);
    ASSERT_EQ(lyrics.lines.size(), 2);

    const auto& firstLine = lyrics.lines.at(0);
    EXPECT_EQ(firstLine.timestamp, 1000);
    EXPECT_EQ(firstLine.duration, 2000);
    ASSERT_EQ(firstLine.words.size(), 2);
    EXPECT_EQ(firstLine.words.at(0).word, u"Hello "_s);
    EXPECT_EQ(firstLine.words.at(0).timestamp, 1100);
    EXPECT_EQ(firstLine.words.at(0).duration, 500);
    EXPECT_EQ(firstLine.words.at(1).word, u"world "_s);
    EXPECT_EQ(firstLine.words.at(1).timestamp, 1600);
    EXPECT_EQ(firstLine.words.at(1).duration, 1400);

    const auto& secondLine = lyrics.lines.at(1);
    EXPECT_EQ(secondLine.timestamp, 3000);
    EXPECT_EQ(secondLine.duration, 0);
    ASSERT_EQ(secondLine.words.size(), 2);
    EXPECT_EQ(secondLine.words.at(0).word, u"Next "_s);
    EXPECT_EQ(secondLine.words.at(0).timestamp, 3200);
    EXPECT_EQ(secondLine.words.at(0).duration, 600);
    EXPECT_EQ(secondLine.words.at(1).word, u"line "_s);
    EXPECT_EQ(secondLine.words.at(1).timestamp, 3800);
    EXPECT_EQ(secondLine.words.at(1).duration, 0);
}

TEST(LyricsParserTest, AppliesOffsetToEnhancedLrcLineAndWordTimestamps)
{
    const auto lyrics = Lyrics::parse(u"[offset:+200]\n"
                                      "[00:01.00]<00:01.10>Hello <00:01.60>world"_s);

    ASSERT_EQ(lyrics.type, Lyrics::Lyrics::Type::SyncedWords);
    EXPECT_EQ(lyrics.offset, 200);
    ASSERT_EQ(lyrics.lines.size(), 1);
    EXPECT_EQ(lyrics.lines.at(0).timestamp, 800);
    ASSERT_EQ(lyrics.lines.at(0).words.size(), 2);
    EXPECT_EQ(lyrics.lines.at(0).words.at(0).timestamp, 900);
    EXPECT_EQ(lyrics.lines.at(0).words.at(1).timestamp, 1400);
}

TEST(LyricsParserTest, PositiveOffsetAdvancesTimestamps)
{
    const auto lyrics = parseLyrics(u"[offset:600]"_s);

    ASSERT_EQ(lyrics.lines.size(), 2);
    EXPECT_EQ(lyrics.offset, 600);
    EXPECT_EQ(lyrics.lines.at(0).timestamp, 9400);
    EXPECT_EQ(lyrics.lines.at(1).timestamp, 11400);
}

TEST(LyricsParserTest, NegativeOffsetDelaysTimestamps)
{
    const auto lyrics = parseLyrics(u"[offset:-600]"_s);

    ASSERT_EQ(lyrics.lines.size(), 2);
    EXPECT_EQ(lyrics.offset, -600);
    EXPECT_EQ(lyrics.lines.at(0).timestamp, 10600);
    EXPECT_EQ(lyrics.lines.at(1).timestamp, 12600);
}

TEST(LyricsParserTest, PositiveOffsetClampsTimestampsAtZero)
{
    const auto lyrics = Lyrics::parse(u"[offset:600]\n[00:00.40]First"_s);

    ASSERT_EQ(lyrics.lines.size(), 1);
    EXPECT_EQ(lyrics.lines.at(0).timestamp, 0);
}
} // namespace Fooyin::Testing
