/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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

#include "plugins/tageditor/tagfillpattern.h"

#include <core/constants.h>

#include <gtest/gtest.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
TEST(FillPatternTest, MatchesLiteralAndCaptureFields)
{
    const auto pattern = TagEditor::FillPattern::parse(u"%ARTIST% - %TITLE%"_s);

    ASSERT_TRUE(pattern.isValid());

    const auto match = pattern.match(u"Massive Attack - Teardrop"_s);
    ASSERT_TRUE(match.has_value());
    ASSERT_EQ(match->values.size(), 2);
    EXPECT_EQ(match->values.at(0).first, u"ARTIST"_s);
    EXPECT_EQ(match->values.at(0).second, u"Massive Attack"_s);
    EXPECT_EQ(match->values.at(1).first, u"TITLE"_s);
    EXPECT_EQ(match->values.at(1).second, u"Teardrop"_s);
}

TEST(FillPatternTest, SupportsIgnoredSections)
{
    const auto pattern = TagEditor::FillPattern::parse(u"%TRACK%. %% - %TITLE%"_s);

    ASSERT_TRUE(pattern.isValid());

    const auto match = pattern.match(u"01. Unused text - Angel"_s);
    ASSERT_TRUE(match.has_value());
    ASSERT_EQ(match->values.size(), 2);
    EXPECT_EQ(match->values.at(0).first, u"TRACK"_s);
    EXPECT_EQ(match->values.at(0).second, u"01"_s);
    EXPECT_EQ(match->values.at(1).first, u"TITLE"_s);
    EXPECT_EQ(match->values.at(1).second, u"Angel"_s);
}

TEST(FillPatternTest, RejectsAdjacentFieldsWithoutLiteralBoundary)
{
    const auto pattern = TagEditor::FillPattern::parse(u"%ARTIST%%TITLE%"_s);

    EXPECT_FALSE(pattern.isValid());
    ASSERT_FALSE(pattern.errors().empty());
}

TEST(FillPatternTest, RejectsNonWritableBuiltInFields)
{
    const auto pattern = TagEditor::FillPattern::parse(u"%FILEPATH%"_s);

    EXPECT_FALSE(pattern.isValid());
    ASSERT_FALSE(pattern.errors().empty());
}

TEST(FillPatternTest, AcceptsWritableRatingAliases)
{
    EXPECT_TRUE(TagEditor::FillPattern::parse(u"%RATING%"_s).isValid());
    EXPECT_TRUE(TagEditor::FillPattern::parse(u"%STARS%"_s).isValid());
    EXPECT_TRUE(TagEditor::FillPattern::parse(u"%RATING_NORMALIZED%"_s).isValid());
}

TEST(FillPatternTest, ConvertsKnownListFieldsToStringLists)
{
    const auto fieldValue = TagEditor::fillFieldValue(QString::fromLatin1(Constants::MetaData::Artist), u"A; B ; C"_s);

    const auto* values = std::get_if<QStringList>(&fieldValue);
    ASSERT_NE(values, nullptr);
    EXPECT_EQ(*values, (QStringList{u"A"_s, u"B"_s, u"C"_s}));
}

TEST(FillPatternTest, LeavesExtraTagsAsStrings)
{
    const auto fieldValue = TagEditor::fillFieldValue(u"CATALOGNUMBER"_s, u"ABC-123"_s);

    const auto* value = std::get_if<QString>(&fieldValue);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, u"ABC-123"_s);
}

TEST(FillPatternTest, MatchesPartialSuffixAndIgnoresRemainingSource)
{
    const auto pattern = TagEditor::FillPattern::parse(u"%TITLE%.f"_s);

    ASSERT_TRUE(pattern.isValid());

    const auto match = pattern.match(u"Fear of a Blank Planet.flac"_s);
    ASSERT_TRUE(match.has_value());
    ASSERT_EQ(match->values.size(), 1);
    EXPECT_EQ(match->values.at(0).first, u"TITLE"_s);
    EXPECT_EQ(match->values.at(0).second, u"Fear of a Blank Planet"_s);
}

TEST(FillPatternTest, ReturnsFieldsMatchedBeforeMissingSuffixLiteral)
{
    const auto pattern = TagEditor::FillPattern::parse(u"%ARTIST% - %TITLE% ["_s);

    ASSERT_TRUE(pattern.isValid());

    const auto match = pattern.match(u"Porcupine Tree - Lazarus"_s);
    ASSERT_TRUE(match.has_value());
    ASSERT_EQ(match->values.size(), 2);
    EXPECT_EQ(match->values.at(0).second, u"Porcupine Tree"_s);
    EXPECT_EQ(match->values.at(1).second, u"Lazarus"_s);
}
} // namespace Fooyin::Testing
