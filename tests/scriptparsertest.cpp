/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/scripting/scriptparser.h>
#include <core/track.h>

#include <gtest/gtest.h>

namespace Fooyin::Testing {
class ScriptParserTest : public ::testing::Test
{
protected:
    ScriptParser m_parser;
};

TEST_F(ScriptParserTest, BasicLiteral)
{
    const auto script = m_parser.parse(QStringLiteral("I am a test."));
    EXPECT_EQ(u"I am a test.", m_parser.evaluate(script));
}

TEST_F(ScriptParserTest, EscapeComment)
{
    const auto script1 = m_parser.parse(QStringLiteral(R"("I am a \% test.")"));
    EXPECT_EQ(u"I am a % test.", m_parser.evaluate(script1));

    const auto script2 = m_parser.parse(QStringLiteral(R"("I am an \"escape test.")"));
    EXPECT_EQ(u"I am an \"escape test.", m_parser.evaluate(script2));
}

TEST_F(ScriptParserTest, Quote)
{
    const auto script = m_parser.parse(QStringLiteral(R"("I %am% a $test$.")"));
    EXPECT_EQ(u"I %am% a $test$.", m_parser.evaluate(script));
}

TEST_F(ScriptParserTest, StringTest)
{
    EXPECT_EQ(u"01", m_parser.evaluate(QStringLiteral("$num(1,2)")));
    EXPECT_EQ(u"04", m_parser.evaluate(QStringLiteral("$num(04,2)")));
    EXPECT_EQ(u"A replace cesc", m_parser.evaluate(QStringLiteral("$replace(A replace test,t,c)")));
    EXPECT_EQ(u"test", m_parser.evaluate(QStringLiteral("$slice(A slice test,8)")));
    EXPECT_EQ(u"slice", m_parser.evaluate(QStringLiteral("$slice(A slice test,2,5)")));
    EXPECT_EQ(u"A chop", m_parser.evaluate(QStringLiteral("$chop(A chop test,5)")));
    EXPECT_EQ(u"L", m_parser.evaluate(QStringLiteral("$left(Left test,1)")));
    EXPECT_EQ(u"est", m_parser.evaluate(QStringLiteral("$right(Right test,3)")));
    EXPECT_EQ(u"true", m_parser.evaluate(QStringLiteral("$if($stricmp(cmp,cMp),true,false)")));
    EXPECT_EQ(u"false", m_parser.evaluate(QStringLiteral("$if($strcmp(cmp,cMp),true,false)")));
}

TEST_F(ScriptParserTest, MathTest)
{
    EXPECT_EQ(3, m_parser.evaluate(QStringLiteral("$add(1,2)")).toInt());
    EXPECT_EQ(2, m_parser.evaluate(QStringLiteral("$sub(10,8)")).toInt());
    EXPECT_EQ(99, m_parser.evaluate(QStringLiteral("$mul(3,33)")).toInt());
    EXPECT_EQ(11, m_parser.evaluate(QStringLiteral("$div(33,3)")).toInt());
    EXPECT_EQ(1, m_parser.evaluate(QStringLiteral("$mod(10,3)")).toInt());
    EXPECT_EQ(2, m_parser.evaluate(QStringLiteral("$min(3,2,3,9,23,100,4)")).toInt());
    EXPECT_EQ(100, m_parser.evaluate(QStringLiteral("$max(3,2,3,9,23,100,4)")).toInt());
}

TEST_F(ScriptParserTest, ConditionalTest)
{
    EXPECT_EQ(u"true", m_parser.evaluate(QStringLiteral("$ifequal(1,1,true,false)")));
    EXPECT_EQ(u"false", m_parser.evaluate(QStringLiteral("$ifgreater(23,32,true,false)")));
    EXPECT_EQ(u"true", m_parser.evaluate(QStringLiteral("$iflonger(aaa,bb,true,false)")));
}

TEST_F(ScriptParserTest, MetadataTest)
{
    Track track;
    track.setTitle(QStringLiteral("A Test"));

    EXPECT_EQ(u"A Test", m_parser.evaluate(QStringLiteral("%title%"), track));
    EXPECT_EQ(u"A Test", m_parser.evaluate(QStringLiteral("%title%[ - %album%]"), track));

    track.setAlbum(QStringLiteral("A Test Album"));

    EXPECT_EQ(u"A Test - A Test Album", m_parser.evaluate(QStringLiteral("%title%[ - %album%]"), track));

    track.setGenres({QStringLiteral("Pop"), QStringLiteral("Rock")});

    EXPECT_EQ(u"Pop, Rock", m_parser.evaluate(QStringLiteral("%genre%"), track));
    EXPECT_EQ(u"Pop\037Rock", m_parser.evaluate(QStringLiteral("%<genre>%"), track));

    track.setArtists({QStringLiteral("Me"), QStringLiteral("You")});

    EXPECT_EQ(u"Pop, Rock - Me, You", m_parser.evaluate(QStringLiteral("%genre% - %artist%"), track));
    EXPECT_EQ(u"Pop - Me\037Rock - Me\037Pop - You\037Rock - You",
              m_parser.evaluate(QStringLiteral("%<genre>% - %<artist>%"), track));

    EXPECT_EQ(u"", m_parser.evaluate(QStringLiteral("[%disc% - %track%]"), track));
}

TEST_F(ScriptParserTest, TrackListTest)
{
    TrackList tracks;

    Track track1;
    track1.setGenres({QStringLiteral("Pop")});
    track1.setDuration(2000);
    tracks.push_back(track1);

    Track track2;
    track2.setGenres({QStringLiteral("Rock")});
    track2.setDuration(3000);
    tracks.push_back(track2);

    EXPECT_EQ(u"2", m_parser.evaluate(QStringLiteral("%trackcount%"), tracks));
    EXPECT_EQ(u"00:05", m_parser.evaluate(QStringLiteral("%playtime%"), tracks));
    EXPECT_EQ(u"Pop / Rock", m_parser.evaluate(QStringLiteral("%genres%"), tracks));
}

TEST_F(ScriptParserTest, MetaTest)
{
    Track track;
    track.setArtists({QStringLiteral("The Verve")});

    EXPECT_EQ(u"The Verve", m_parser.evaluate(QStringLiteral("%albumartist%"), track));
    EXPECT_EQ(u"", m_parser.evaluate(QStringLiteral("$meta(albumartist)"), track));
    EXPECT_EQ(u"", m_parser.evaluate(QStringLiteral("$meta(%albumartist%)"), track));
}

TEST_F(ScriptParserTest, InfoTest)
{
    Track track;
    track.setChannels(2);

    EXPECT_EQ(u"Stereo", m_parser.evaluate(QStringLiteral("%channels%"), track));
    EXPECT_EQ(u"2", m_parser.evaluate(QStringLiteral("$info(channels)"), track));
}

TEST_F(ScriptParserTest, QueryTest)
{
    TrackList tracks;

    Track track1;
    track1.setTitle(QStringLiteral("ABC"));
    track1.setGenres({QStringLiteral("Pop")});
    track1.setDuration(2000);
    track1.setPlayCount(1);
    tracks.push_back(track1);

    Track track2;
    track2.setTitle(QStringLiteral("DEF"));
    track2.setGenres({QStringLiteral("Rock")});
    track2.setDuration(3000);
    tracks.push_back(track2);

    EXPECT_TRUE(m_parser.filter(QStringLiteral("$info(duration)=2000"), tracks).size() == 1);
    EXPECT_TRUE(m_parser.filter(QStringLiteral("playcount>1"), tracks).empty());
    EXPECT_TRUE(m_parser.filter(QStringLiteral("playcount>=1"), tracks).size() == 1);
    EXPECT_TRUE(m_parser.filter(QStringLiteral("(playcount>0 AND genre=rock) OR title:BC"), tracks).size() == 1);
}
} // namespace Fooyin::Testing
