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

#include <utils/stringutils.h>

#include <gtest/gtest.h>

#include <QByteArray>

namespace Fooyin::Testing {
TEST(UtilsStringTest, DetectEncodingKeepsHighConfidenceUtf8WithPreferredFallback)
{
    const QByteArray data = QByteArray::fromHex("48656c6c6f20e4b896e7958c");

    EXPECT_EQ(Utils::detectEncoding(data, {.preferredFallbackEncoding = "GB18030"}), "UTF-8");
}

TEST(UtilsStringTest, DetectEncodingUsesPreferredFallbackForLowConfidenceCjkMatch)
{
    const QByteArray data = QByteArray::fromHex("b0aec4e3b2bbcac7c1bdc8fdccec");

    EXPECT_EQ(Utils::detectEncoding(data, {.preferredFallbackEncoding = "GB18030"}), "GB18030");
}

TEST(UtilsStringTest, DetectEncodingKeepsTopMatchWithoutPreferredFallback)
{
    const QByteArray data = QByteArray::fromHex("b0aec4e3b2bbcac7c1bdc8fdccec");

    EXPECT_EQ(Utils::detectEncoding(data), "windows-1256");
}

TEST(UtilsStringTest, DetectEncodingStillPrefersFallbackForLatinTopMatch)
{
    const QByteArray data
        = QByteArray::fromHex("5449544c452022cff0e5e4e2e5f1f2ede8ea220a504552464f524d45522022caedff5a7a22");

    EXPECT_EQ(Utils::detectEncoding(data, {.preferredFallbackEncoding = "windows-1251"}), "windows-1251");
}

TEST(UtilsStringTest, DetectEncodingReturnsEmptyForEmptyContent)
{
    EXPECT_TRUE(Utils::detectEncoding({}, {.preferredFallbackEncoding = "GB18030"}).isEmpty());
}
} // namespace Fooyin::Testing
