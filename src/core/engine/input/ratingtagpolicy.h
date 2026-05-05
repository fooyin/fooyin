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

#pragma once

#include "fycore_export.h"

#include <QString>

#include <cstdint>
#include <optional>
#include <span>

namespace Fooyin {
namespace RatingSettings {
constexpr auto ReadTag              = "Rating/ReadTag";
constexpr auto ReadScale            = "Rating/ReadScale";
constexpr auto WriteTag             = "Rating/WriteTag";
constexpr auto WriteScale           = "Rating/WriteScale";
constexpr auto ReadId3Popm          = "Rating/ReadId3Popm";
constexpr auto WriteId3Popm         = "Rating/WriteId3Popm";
constexpr auto PopmOwner            = "Rating/PopmOwner";
constexpr auto PopmMapping          = "Rating/PopmMapping";
constexpr auto ReadAsfSharedRating  = "Rating/ReadAsfSharedRating";
constexpr auto WriteAsfSharedRating = "Rating/WriteAsfSharedRating";

constexpr auto DefaultAutomatic             = "Automatic";
constexpr auto DefaultFmpsRatingTag         = "FMPS_RATING";
constexpr auto DefaultNormalizedRatingScale = "Normalized01";
constexpr auto DefaultPopmMapping           = "Default";
constexpr auto DefaultReadId3Popm           = true;
constexpr auto DefaultWriteId3Popm          = true;
constexpr auto DefaultReadAsfSharedRating   = true;
constexpr auto DefaultWriteAsfSharedRating  = true;
} // namespace RatingSettings

enum class RatingScale : uint8_t
{
    Automatic = 0,
    Normalized01,
    OneToFive,
    OneToTen,
    OneToHundred,
};

enum class PopmMapping : uint8_t
{
    Default = 0,
    CommonFiveStar,
    LinearByte,
};

struct RatingScaleDescriptor
{
    RatingScale scale;
    const char* label;
    bool availableForRead;
    bool availableForWrite;
};

struct PopmMappingDescriptor
{
    PopmMapping mapping;
    const char* label;
};

struct FYCORE_EXPORT RatingTagPolicy
{
    QString readTag;
    RatingScale readScale{RatingScale::Automatic};
    QString writeTag;
    RatingScale writeScale{RatingScale::Automatic};
    bool readId3Popm{true};
    bool writeId3Popm{true};
    QString popmOwner;
    PopmMapping popmMapping{PopmMapping::Default};
    bool readAsfSharedRating{true};
    bool writeAsfSharedRating{true};

    [[nodiscard]] bool automaticRead() const;
    [[nodiscard]] bool shouldReadTextTag(const QString& tag, bool currentRatingSet) const;
    [[nodiscard]] bool shouldWriteTextTag() const;
    [[nodiscard]] QString effectiveWriteTag() const;
};

[[nodiscard]] FYCORE_EXPORT std::span<const RatingScaleDescriptor> ratingScaleDescriptors();
[[nodiscard]] FYCORE_EXPORT std::span<const PopmMappingDescriptor> popmMappingDescriptors();
[[nodiscard]] FYCORE_EXPORT QString ratingScaleKey(RatingScale scale);
[[nodiscard]] FYCORE_EXPORT QString popmMappingKey(PopmMapping mapping);
[[nodiscard]] FYCORE_EXPORT RatingScale ratingScaleFromString(QString scale);
[[nodiscard]] FYCORE_EXPORT PopmMapping popmMappingFromString(QString mapping);
[[nodiscard]] FYCORE_EXPORT RatingTagPolicy ratingTagPolicy();
[[nodiscard]] FYCORE_EXPORT std::optional<float> normalisedTextRating(const QString& rawRating, RatingScale scale,
                                                                      bool ratingTagFallback);
[[nodiscard]] FYCORE_EXPORT QString formatTextRating(float rating, RatingScale scale);
[[nodiscard]] FYCORE_EXPORT float asfSharedUserRatingToRating(int rating);
[[nodiscard]] FYCORE_EXPORT int ratingToAsfSharedUserRating(float rating);
[[nodiscard]] FYCORE_EXPORT float popmToRating(int popm, PopmMapping mapping);
[[nodiscard]] FYCORE_EXPORT int ratingToPopm(float rating, PopmMapping mapping);
} // namespace Fooyin
