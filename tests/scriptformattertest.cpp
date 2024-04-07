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

#include <gui/scripting/scriptformatter.h>
#include <gui/scripting/scriptformatterregistry.h>

#include <gtest/gtest.h>

namespace Fooyin::Testing {
class ScriptFormatterTest : public ::testing::Test
{
protected:
    ScriptFormatter m_formattter;
};

TEST_F(ScriptFormatterTest, NoFormat)
{
    const auto result = m_formattter.evaluate(QStringLiteral("I am a test."));
    EXPECT_EQ(1, result.size());
}

TEST_F(ScriptFormatterTest, Bold)
{
    const auto result = m_formattter.evaluate(QStringLiteral("<b>I</b> am a test."));
    ASSERT_EQ(2, result.size());
    EXPECT_TRUE(result.front().format.font.bold());
}

TEST_F(ScriptFormatterTest, Italic)
{
    const auto result = m_formattter.evaluate(QStringLiteral("<i>I</i> am a test."));
    ASSERT_EQ(2, result.size());
    EXPECT_TRUE(result.front().format.font.italic());
}

TEST_F(ScriptFormatterTest, Rgb)
{
    const auto result = m_formattter.evaluate(QStringLiteral("<rgb=255,0,0>I am a test."));
    ASSERT_EQ(1, result.size());
    EXPECT_EQ(255, result.front().format.colour.red());
}
} // namespace Fooyin::Testing
