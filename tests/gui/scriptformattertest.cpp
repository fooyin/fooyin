/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <gui/scripting/scriptformatter.h>
#include <gui/scripting/scriptformatterregistry.h>

#include <gtest/gtest.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
class ScriptFormatterTest : public ::testing::Test
{
protected:
    ScriptFormatter m_formattter;
};

TEST_F(ScriptFormatterTest, NoFormat)
{
    const auto result = m_formattter.evaluate(u"I am a test."_s);
    EXPECT_EQ(1, result.size());
    EXPECT_FALSE(result.blocks.front().format.colour.isValid());
}

TEST_F(ScriptFormatterTest, Bold)
{
    const auto result = m_formattter.evaluate(u"<b>I</b> am a test."_s);
    ASSERT_EQ(2, result.size());
    EXPECT_TRUE(result.blocks.front().format.font.bold());
}

TEST_F(ScriptFormatterTest, Italic)
{
    const auto result = m_formattter.evaluate(u"<i>I</i> am a test."_s);
    ASSERT_EQ(2, result.size());
    EXPECT_TRUE(result.blocks.front().format.font.italic());
}

TEST_F(ScriptFormatterTest, Rgb)
{
    const auto result = m_formattter.evaluate(u"<rgb=255,0,0>I am a test."_s);
    ASSERT_EQ(1, result.size());
    EXPECT_EQ(255, result.blocks.front().format.colour.red());
}

TEST_F(ScriptFormatterTest, ColorName)
{
    const auto result = m_formattter.evaluate(u"<color=red>I am a test."_s);
    ASSERT_EQ(1, result.size());
    EXPECT_EQ(QColor(u"red"_s), result.blocks.front().format.colour);
}

TEST_F(ScriptFormatterTest, ColorHex)
{
    const auto result = m_formattter.evaluate(u"<color=#00ff00>I am a test."_s);
    ASSERT_EQ(1, result.size());
    EXPECT_EQ(0, result.blocks.front().format.colour.red());
    EXPECT_EQ(255, result.blocks.front().format.colour.green());
    EXPECT_EQ(0, result.blocks.front().format.colour.blue());
}

TEST_F(ScriptFormatterTest, BaseColour)
{
    const QColor colour{u"#123456"_s};
    m_formattter.setBaseColour(colour);

    const auto result = m_formattter.evaluate(u"I am a test."_s);
    ASSERT_EQ(1, result.size());
    EXPECT_EQ(colour, result.blocks.front().format.colour);
}

TEST_F(ScriptFormatterTest, EscapedLeftAngle)
{
    const auto result = m_formattter.evaluate(u"\\<A"_s);
    ASSERT_EQ(1, result.size());
    EXPECT_EQ(u"<A"_s, result.blocks.front().text);
}

TEST_F(ScriptFormatterTest, Link)
{
    const auto result = m_formattter.evaluate(u"<a href=\"https://example.com\">Example</a>"_s);
    ASSERT_EQ(1, result.size());
    EXPECT_EQ(u"Example"_s, result.blocks.front().text);
    EXPECT_EQ(u"https://example.com"_s, result.blocks.front().format.link);
    EXPECT_TRUE(result.blocks.front().format.font.underline());
}
} // namespace Fooyin::Testing
