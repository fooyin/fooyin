/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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
    m_parser.parse(u"I am a test."_s);
    EXPECT_EQ("I am a test.", m_parser.evaluate());
}

TEST_F(ScriptParserTest, EscapeComment)
{
    m_parser.parse(uR"("I am a \% test.")"_s);
    EXPECT_EQ("I am a % test.", m_parser.evaluate());

    m_parser.parse(uR"("I am an \"escape test.")"_s);
    EXPECT_EQ("I am an \"escape test.", m_parser.evaluate());
}

TEST_F(ScriptParserTest, Quote)
{
    m_parser.parse(uR"("I %am% a $test$.")"_s);
    EXPECT_EQ("I %am% a $test$.", m_parser.evaluate());
}

TEST_F(ScriptParserTest, StringTest)
{
    m_parser.parse(u"$num(1,2)"_s);
    EXPECT_EQ("01", m_parser.evaluate());

    m_parser.parse(u"$num(04,2)"_s);
    EXPECT_EQ("04", m_parser.evaluate());

    m_parser.parse(u"$replace(A replace test,t,c)"_s);
    EXPECT_EQ("A replace cesc", m_parser.evaluate());

    m_parser.parse(u"$slice(A slice test,8)"_s);
    EXPECT_EQ("test", m_parser.evaluate());

    m_parser.parse(u"$slice(A slice test,2,5)"_s);
    EXPECT_EQ("slice", m_parser.evaluate());

    m_parser.parse(u"$chop(A chop test,5)"_s);
    EXPECT_EQ("A chop", m_parser.evaluate());

    m_parser.parse(u"$left(Left test,1)"_s);
    EXPECT_EQ("L", m_parser.evaluate());

    m_parser.parse(u"$right(Right test,3)"_s);
    EXPECT_EQ("est", m_parser.evaluate());

    m_parser.parse(u"$if($strcmpi(cmp,cMp),true,false)"_s);
    EXPECT_EQ("true", m_parser.evaluate());

    m_parser.parse(u"$if($strcmp(cmp,cMp),true,false)"_s);
    EXPECT_EQ("false", m_parser.evaluate());
}

TEST_F(ScriptParserTest, MathTest)
{
    m_parser.parse(u"$add(1,2)"_s);
    EXPECT_EQ(3, m_parser.evaluate().toInt());

    m_parser.parse(u"$sub(10,8)"_s);
    EXPECT_EQ(2, m_parser.evaluate().toInt());

    m_parser.parse(u"$mul(3,33)"_s);
    EXPECT_EQ(99, m_parser.evaluate().toInt());

    m_parser.parse(u"$div(33,3)"_s);
    EXPECT_EQ(11, m_parser.evaluate().toInt());

    m_parser.parse(u"$mod(10,3)"_s);
    EXPECT_EQ(1, m_parser.evaluate().toInt());

    m_parser.parse(u"$min(3,2,3,9,23,100,4)"_s);
    EXPECT_EQ(2, m_parser.evaluate().toInt());

    m_parser.parse(u"$max(3,2,3,9,23,100,4)"_s);
    EXPECT_EQ(100, m_parser.evaluate().toInt());
}

TEST_F(ScriptParserTest, ConditionalTest)
{
    m_parser.parse(u"$ifequal(1,1,true,false)"_s);
    EXPECT_EQ("true", m_parser.evaluate());

    m_parser.parse(u"$ifgreater(23,32,true,false)"_s);
    EXPECT_EQ("false", m_parser.evaluate());

    m_parser.parse(u"$iflonger(aaa,bb,true,false)"_s);
    EXPECT_EQ("true", m_parser.evaluate());
}

TEST_F(ScriptParserTest, MetadataTest)
{
    Track track;
    track.setTitle(u"A Test"_s);
    m_parser.setMetadata(track);

    m_parser.parse(u"%title%"_s);
    EXPECT_EQ("A Test", m_parser.evaluate());

    m_parser.parse(u"%title%[ - %album%]"_s);
    EXPECT_EQ("A Test", m_parser.evaluate());

    track.setAlbum(u"A Test Album"_s);
    m_parser.setMetadata(track);
    EXPECT_EQ("A Test - A Test Album", m_parser.evaluate());

    track.setGenres({"Pop", "Rock"});
    m_parser.setMetadata(track);
    m_parser.parse(u"%genre%"_s);
    EXPECT_EQ("Pop, Rock", m_parser.evaluate());

    m_parser.parse(u"%<genre>%"_s);
    EXPECT_EQ("Pop\037Rock", m_parser.evaluate());

    track.setArtists({"Me", "You"});
    m_parser.setMetadata(track);
    m_parser.parse(u"%genre% - %artist%"_s);
    EXPECT_EQ("Pop, Rock - Me, You", m_parser.evaluate());

    m_parser.parse(u"%<genre>% - %<artist>%"_s);
    EXPECT_EQ("Pop - Me\037Rock - Me\037Pop - You\037Rock - You", m_parser.evaluate());

    m_parser.parse(u"[%disc% - %track%]"_s);
    EXPECT_EQ("", m_parser.evaluate());
}
} // namespace Fooyin::Testing
