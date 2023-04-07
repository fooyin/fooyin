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

#include <core/models/track.h>
#include <core/scripting/scriptparser.h>

#include <gtest/gtest.h>

namespace Fy::Testing {
class ScriptParserTest : public ::testing::Test
{
protected:
    Core::Scripting::Parser m_parser;
};

TEST_F(ScriptParserTest, BasicLiteral)
{
    m_parser.parse("I am a test.");
    EXPECT_EQ("I am a test.", m_parser.evaluate());
}

TEST_F(ScriptParserTest, EscapeComment)
{
    m_parser.parse(R"("I am a \% test.")");
    EXPECT_EQ("I am a % test.", m_parser.evaluate());

    m_parser.parse(R"("I am an \"escape test.")");
    EXPECT_EQ("I am an \"escape test.", m_parser.evaluate());
}

TEST_F(ScriptParserTest, Quote)
{
    m_parser.parse(R"("I %am% a $test$.")");
    EXPECT_EQ("I %am% a $test$.", m_parser.evaluate());
}

TEST_F(ScriptParserTest, MathTest)
{
    m_parser.parse("$add(1,2)");
    EXPECT_EQ(3, m_parser.evaluate().toInt());

    m_parser.parse("$sub(10,8)");
    EXPECT_EQ(2, m_parser.evaluate().toInt());

    m_parser.parse("$mul(3,33)");
    EXPECT_EQ(99, m_parser.evaluate().toInt());

    m_parser.parse("$div(33,3)");
    EXPECT_EQ(11, m_parser.evaluate().toInt());

    m_parser.parse("$mod(10,3)");
    EXPECT_EQ(1, m_parser.evaluate().toInt());

    m_parser.parse("$min(3,2,3,9,23,100,4)");
    EXPECT_EQ(2, m_parser.evaluate().toInt());

    m_parser.parse("$max(3,2,3,9,23,100,4)");
    EXPECT_EQ(100, m_parser.evaluate().toInt());
}

TEST_F(ScriptParserTest, MetadataTest)
{
    Core::Track track;
    track.setTitle("A Test");
    m_parser.setMetadata(&track);

    m_parser.parse("%title%");
    EXPECT_EQ("A Test", m_parser.evaluate());

    m_parser.parse("%title%[ - %album%]");
    EXPECT_EQ("A Test", m_parser.evaluate());

    track.setAlbum("A Test Album");
    EXPECT_EQ("A Test - A Test Album", m_parser.evaluate());

    track.setGenres({"Pop", "Rock"});
    m_parser.parse("%genre%");
    EXPECT_EQ("Pop, Rock", m_parser.evaluate());

    track.setArtists({"Me", "You"});
    m_parser.parse("%genre% - %artist%");
    EXPECT_EQ("Pop, Rock - Me, You", m_parser.evaluate());

    m_parser.parse("[%disc% - %track%]");
    EXPECT_EQ("", m_parser.evaluate());
}
} // namespace Fy::Testing
