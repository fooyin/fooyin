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

#include <filters/fieldparser.h>

#include <gtest/gtest.h>

namespace Fy::Testing {
class FieldParserTest : public ::testing::Test
{
public:
    void SetUp() override
    {
        m_parser = std::make_unique<Filters::Scripting::FieldParser>();
    }

protected:
    std::unique_ptr<Core::Scripting::Parser> m_parser;
};

TEST_F(FieldParserTest, MetadataTest)
{
    Core::Track track;
    track.setTitle("A Test");
    m_parser->setMetadata(&track);

    m_parser->parse("%title%");
    EXPECT_EQ("A Test", m_parser->evaluate());

    m_parser->parse("%title%[ - %album%]");
    EXPECT_EQ("A Test", m_parser->evaluate());

    track.setAlbum("A Test Album");
    EXPECT_EQ("A Test - A Test Album", m_parser->evaluate());

    track.setGenres({"Pop", "Rock"});
    m_parser->parse("%genre%");
    EXPECT_EQ("Pop\037Rock", m_parser->evaluate());

    track.setArtists({"Me", "You"});
    m_parser->parse("%genre% - %artist%");
    EXPECT_EQ("Pop - Me\037Rock - Me\037Pop - You\037Rock - You", m_parser->evaluate());

    m_parser->parse("[%disc% - %track%]");
    EXPECT_EQ("", m_parser->evaluate());
}
} // namespace Fy::Testing
