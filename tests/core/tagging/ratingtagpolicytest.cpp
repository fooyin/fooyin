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

#include <core/engine/input/ratingtagpolicy.h>

#include <gtest/gtest.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
TEST(RatingTagPolicyTest, ParsesCanonicalDescriptorKeys)
{
    for(const auto& descriptor : ratingScaleDescriptors()) {
        const QString key = ratingScaleKey(descriptor.scale);
        EXPECT_FALSE(key.isEmpty());
        EXPECT_EQ(ratingScaleFromString(key), descriptor.scale);
    }

    for(const auto& descriptor : popmMappingDescriptors()) {
        const QString key = popmMappingKey(descriptor.mapping);
        EXPECT_FALSE(key.isEmpty());
        EXPECT_EQ(popmMappingFromString(key), descriptor.mapping);
    }
}

TEST(RatingTagPolicyTest, NormalisesConfiguredTextScales)
{
    EXPECT_FLOAT_EQ(*normalisedTextRating(u"0.7"_s, RatingScale::Normalized01, false), 0.7F);
    EXPECT_FLOAT_EQ(*normalisedTextRating(u"3"_s, RatingScale::OneToFive, false), 0.6F);
    EXPECT_FLOAT_EQ(*normalisedTextRating(u"7"_s, RatingScale::OneToTen, false), 0.7F);
    EXPECT_FLOAT_EQ(*normalisedTextRating(u"70"_s, RatingScale::OneToHundred, false), 0.7F);
    EXPECT_FALSE(normalisedTextRating(u"0"_s, RatingScale::OneToFive, false).has_value());
    EXPECT_FALSE(normalisedTextRating(u"0"_s, RatingScale::OneToTen, false).has_value());
    EXPECT_FALSE(normalisedTextRating(u"0"_s, RatingScale::OneToHundred, false).has_value());
}

TEST(RatingTagPolicyTest, PreservesAutomaticFallbacks)
{
    EXPECT_FLOAT_EQ(*normalisedTextRating(u"0.7"_s, RatingScale::Automatic, false), 0.7F);
    EXPECT_FLOAT_EQ(*normalisedTextRating(u"0.7"_s, RatingScale::Automatic, true), 0.7F);
    EXPECT_FLOAT_EQ(*normalisedTextRating(u"3"_s, RatingScale::Automatic, true), 0.6F);
    EXPECT_FLOAT_EQ(*normalisedTextRating(u"7"_s, RatingScale::Automatic, true), 0.7F);
    EXPECT_FLOAT_EQ(*normalisedTextRating(u"70"_s, RatingScale::Automatic, true), 0.7F);
    EXPECT_FALSE(normalisedTextRating(u"7"_s, RatingScale::Automatic, false).has_value());
    EXPECT_FALSE(normalisedTextRating(u"0"_s, RatingScale::Automatic, true).has_value());
    EXPECT_FALSE(normalisedTextRating(u"101"_s, RatingScale::Automatic, true).has_value());
}

TEST(RatingTagPolicyTest, FormatsConfiguredTextScales)
{
    EXPECT_EQ(formatTextRating(0.7F, RatingScale::Normalized01), u"0.7"_s);
    EXPECT_EQ(formatTextRating(0.6F, RatingScale::OneToFive), u"3"_s);
    EXPECT_EQ(formatTextRating(0.7F, RatingScale::OneToTen), u"7"_s);
    EXPECT_EQ(formatTextRating(0.7F, RatingScale::OneToHundred), u"70"_s);
}

TEST(RatingTagPolicyTest, SelectsConfiguredOrAutomaticReadTags)
{
    RatingTagPolicy policy;
    EXPECT_TRUE(policy.shouldReadTextTag(u"FMPS_RATING"_s, true));
    EXPECT_FALSE(policy.shouldReadTextTag(u"RATING"_s, true));
    EXPECT_TRUE(policy.shouldReadTextTag(u"RATING"_s, false));

    policy.readTag = u"RATING"_s;
    EXPECT_TRUE(policy.shouldReadTextTag(u"RATING"_s, true));
    EXPECT_FALSE(policy.shouldReadTextTag(u"FMPS_RATING"_s, true));
}

TEST(RatingTagPolicyTest, ConvertsConfiguredPopmMappings)
{
    EXPECT_EQ(popmMappingFromString(u"Default"_s), PopmMapping::Default);
    EXPECT_FLOAT_EQ(popmToRating(196, PopmMapping::Default), 0.8F);
    EXPECT_EQ(ratingToPopm(0.75F, PopmMapping::Default), 186);

    EXPECT_FLOAT_EQ(popmToRating(196, PopmMapping::CommonFiveStar), 0.8F);
    EXPECT_EQ(ratingToPopm(0.75F, PopmMapping::CommonFiveStar), 196);

    EXPECT_FLOAT_EQ(popmToRating(128, PopmMapping::LinearByte), 128.0F / 255.0F);
    EXPECT_EQ(ratingToPopm(0.5F, PopmMapping::LinearByte), 128);
}

TEST(RatingTagPolicyTest, ConvertsAsfSharedUserRatings)
{
    EXPECT_FLOAT_EQ(asfSharedUserRatingToRating(0), 0.0F);
    EXPECT_FLOAT_EQ(asfSharedUserRatingToRating(1), 0.2F);
    EXPECT_FLOAT_EQ(asfSharedUserRatingToRating(25), 0.4F);
    EXPECT_FLOAT_EQ(asfSharedUserRatingToRating(50), 0.6F);
    EXPECT_FLOAT_EQ(asfSharedUserRatingToRating(75), 0.8F);
    EXPECT_FLOAT_EQ(asfSharedUserRatingToRating(99), 1.0F);
    EXPECT_FLOAT_EQ(asfSharedUserRatingToRating(100), 1.0F);

    EXPECT_EQ(ratingToAsfSharedUserRating(0.0F), 0);
    EXPECT_EQ(ratingToAsfSharedUserRating(0.2F), 1);
    EXPECT_EQ(ratingToAsfSharedUserRating(0.4F), 25);
    EXPECT_EQ(ratingToAsfSharedUserRating(0.6F), 50);
    EXPECT_EQ(ratingToAsfSharedUserRating(0.8F), 75);
    EXPECT_EQ(ratingToAsfSharedUserRating(1.0F), 99);
}

TEST(RatingTagPolicyTest, SelectsConfiguredWriteTags)
{
    RatingTagPolicy policy;
    policy.writeId3Popm = true;
    policy.writeTag     = u"RATING"_s;

    EXPECT_TRUE(policy.shouldWriteTextTag());
    EXPECT_EQ(policy.effectiveWriteTag(), u"RATING"_s);

    policy.writeTag = u"DONOTWRITE"_s;
    EXPECT_FALSE(policy.shouldWriteTextTag());
    EXPECT_TRUE(policy.effectiveWriteTag().isEmpty());
}
} // namespace Fooyin::Testing
