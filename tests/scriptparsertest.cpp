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

using namespace Qt::Literals::StringLiterals;

namespace Fooyin::Testing {
class ScriptParserTest : public ::testing::Test
{
protected:
    ScriptRegistry m_registry;
    ScriptParser m_parser{&m_registry};
};

TEST_F(ScriptParserTest, BasicLiteral)
{
    const auto script = m_parser.parse(u"I am a test."_s);
    EXPECT_EQ("I am a test.", m_parser.evaluate(script));
}

TEST_F(ScriptParserTest, EscapeComment)
{
    const auto script1 = m_parser.parse(uR"("I am a \% test.")"_s);
    EXPECT_EQ("I am a % test.", m_parser.evaluate(script1));

    const auto script2 = m_parser.parse(uR"("I am an \"escape test.")"_s);
    EXPECT_EQ("I am an \"escape test.", m_parser.evaluate(script2));
}

TEST_F(ScriptParserTest, Quote)
{
    const auto script = m_parser.parse(uR"("I %am% a $test$.")"_s);
    EXPECT_EQ("I %am% a $test$.", m_parser.evaluate(script));
}

TEST_F(ScriptParserTest, StringTest)
{
    EXPECT_EQ("01", m_parser.evaluate(u"$num(1,2)"_s));
    EXPECT_EQ("04", m_parser.evaluate(u"$num(04,2)"_s));
    EXPECT_EQ("A replace cesc", m_parser.evaluate(u"$replace(A replace test,t,c)"_s));
    EXPECT_EQ("test", m_parser.evaluate(u"$slice(A slice test,8)"_s));
    EXPECT_EQ("slice", m_parser.evaluate(u"$slice(A slice test,2,5)"_s));
    EXPECT_EQ("A chop", m_parser.evaluate(u"$chop(A chop test,5)"_s));
    EXPECT_EQ("L", m_parser.evaluate(u"$left(Left test,1)"_s));
    EXPECT_EQ("est", m_parser.evaluate(u"$right(Right test,3)"_s));
    EXPECT_EQ("true", m_parser.evaluate(u"$if($strcmpi(cmp,cMp),true,false)"_s));
    EXPECT_EQ("false", m_parser.evaluate(u"$if($strcmp(cmp,cMp),true,false)"_s));
}

TEST_F(ScriptParserTest, MathTest)
{
    EXPECT_EQ(3, m_parser.evaluate(u"$add(1,2)"_s).toInt());
    EXPECT_EQ(2, m_parser.evaluate(u"$sub(10,8)"_s).toInt());
    EXPECT_EQ(99, m_parser.evaluate(u"$mul(3,33)"_s).toInt());
    EXPECT_EQ(11, m_parser.evaluate(u"$div(33,3)"_s).toInt());
    EXPECT_EQ(1, m_parser.evaluate(u"$mod(10,3)"_s).toInt());
    EXPECT_EQ(2, m_parser.evaluate(u"$min(3,2,3,9,23,100,4)"_s).toInt());
    EXPECT_EQ(100, m_parser.evaluate(u"$max(3,2,3,9,23,100,4)"_s).toInt());
}

TEST_F(ScriptParserTest, ConditionalTest)
{
    EXPECT_EQ("true", m_parser.evaluate(u"$ifequal(1,1,true,false)"_s));
    EXPECT_EQ("false", m_parser.evaluate(u"$ifgreater(23,32,true,false)"_s));
    EXPECT_EQ("true", m_parser.evaluate(u"$iflonger(aaa,bb,true,false)"_s));
}

TEST_F(ScriptParserTest, MetadataTest)
{
    Track track;
    track.setTitle(u"A Test"_s);

    EXPECT_EQ("A Test", m_parser.evaluate(u"%title%"_s, track));
    EXPECT_EQ("A Test", m_parser.evaluate(u"%title%[ - %album%]"_s, track));

    track.setAlbum(u"A Test Album"_s);

    EXPECT_EQ("A Test - A Test Album", m_parser.evaluate(u"%title%[ - %album%]"_s, track));

    track.setGenres({"Pop", "Rock"});

    EXPECT_EQ("Pop, Rock", m_parser.evaluate(u"%genre%"_s, track));
    EXPECT_EQ("Pop\037Rock", m_parser.evaluate(u"%<genre>%"_s, track));

    track.setArtists({"Me", "You"});

    EXPECT_EQ("Pop, Rock - Me, You", m_parser.evaluate(u"%genre% - %artist%"_s, track));
    EXPECT_EQ("Pop - Me\037Rock - Me\037Pop - You\037Rock - You",
              m_parser.evaluate(u"%<genre>% - %<artist>%"_s, track));

    EXPECT_EQ("", m_parser.evaluate(u"[%disc% - %track%]"_s, track));
}

TEST_F(ScriptParserTest, TrackListTest)
{
    TrackList tracks;

    Track track1;
    track1.setGenres({"Pop"});
    track1.setDuration(2000);
    tracks.push_back(track1);

    Track track2;
    track2.setGenres({"Rock"});
    track2.setDuration(3000);
    tracks.push_back(track2);

    EXPECT_EQ("2", m_parser.evaluate(u"%trackcount%"_s, tracks));
    EXPECT_EQ("00:05", m_parser.evaluate(u"%playtime%"_s, tracks));
    EXPECT_EQ("Pop / Rock", m_parser.evaluate(u"%genres%"_s, tracks));
}
} // namespace Fooyin::Testing
