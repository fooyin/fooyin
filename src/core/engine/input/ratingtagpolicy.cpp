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

#include "ratingtagpolicy.h"

#include <core/coresettings.h>

#include <algorithm>
#include <array>
#include <cmath>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
constexpr std::array RatingScaleTable{
    RatingScaleDescriptor{.scale             = RatingScale::Automatic,
                          .label             = "Automatic detection",
                          .availableForRead  = true,
                          .availableForWrite = false},
    RatingScaleDescriptor{.scale             = RatingScale::Normalized01,
                          .label             = "0.0 - 1.0",
                          .availableForRead  = true,
                          .availableForWrite = true},
    RatingScaleDescriptor{.scale             = RatingScale::OneToFive,
                          .label             = "1 - 5",
                          .availableForRead  = true,
                          .availableForWrite = true},
    RatingScaleDescriptor{.scale             = RatingScale::OneToTen,
                          .label             = "1 - 10",
                          .availableForRead  = true,
                          .availableForWrite = true},
    RatingScaleDescriptor{.scale             = RatingScale::OneToHundred,
                          .label             = "1 - 100",
                          .availableForRead  = true,
                          .availableForWrite = true},
};

constexpr std::array PopmMappingTable{
    PopmMappingDescriptor{.mapping = PopmMapping::Default, .label = "Default"},
    PopmMappingDescriptor{.mapping = PopmMapping::CommonFiveStar, .label = "Five star compatibility"},
    PopmMappingDescriptor{.mapping = PopmMapping::LinearByte, .label = "Raw 0-255 byte scale"},
};

QString normalisedConfigKey(QString value)
{
    value = value.trimmed().toUpper();
    value.remove(u'_');
    value.remove(u' ');
    return value;
}
} // namespace

bool RatingTagPolicy::automaticRead() const
{
    return readTag.isEmpty() || readTag == "AUTOMATIC"_L1;
}

bool RatingTagPolicy::shouldReadTextTag(const QString& tag, bool currentRatingSet) const
{
    if(automaticRead()) {
        return tag == "FMPS_RATING"_L1 || !currentRatingSet;
    }
    return tag == readTag;
}

bool RatingTagPolicy::shouldWriteTextTag() const
{
    return !effectiveWriteTag().isEmpty();
}

QString RatingTagPolicy::effectiveWriteTag() const
{
    return writeTag == "DONOTWRITE"_L1 ? QString{} : writeTag;
}

std::span<const RatingScaleDescriptor> ratingScaleDescriptors()
{
    return RatingScaleTable;
}

std::span<const PopmMappingDescriptor> popmMappingDescriptors()
{
    return PopmMappingTable;
}

QString ratingScaleKey(RatingScale scale)
{
    switch(scale) {
        case RatingScale::Automatic:
            return QString::fromLatin1(RatingSettings::DefaultAutomatic);
        case RatingScale::Normalized01:
            return u"Normalized01"_s;
        case RatingScale::OneToFive:
            return u"OneToFive"_s;
        case RatingScale::OneToTen:
            return u"OneToTen"_s;
        case RatingScale::OneToHundred:
            return u"OneToHundred"_s;
    }

    return QString::fromLatin1(RatingSettings::DefaultAutomatic);
}

QString popmMappingKey(PopmMapping mapping)
{
    switch(mapping) {
        case PopmMapping::Default:
            return QString::fromLatin1(RatingSettings::DefaultPopmMapping);
        case PopmMapping::CommonFiveStar:
            return u"CommonFiveStar"_s;
        case PopmMapping::LinearByte:
            return u"LinearByte"_s;
    }

    return QString::fromLatin1(RatingSettings::DefaultPopmMapping);
}

RatingScale ratingScaleFromString(QString scale)
{
    scale = normalisedConfigKey(std::move(scale));
    for(const auto& descriptor : RatingScaleTable) {
        if(scale == normalisedConfigKey(ratingScaleKey(descriptor.scale))) {
            return descriptor.scale;
        }
    }

    if(scale == "0.0-1.0"_L1 || scale == "0-1"_L1) {
        return RatingScale::Normalized01;
    }
    if(scale == "1-5"_L1) {
        return RatingScale::OneToFive;
    }
    if(scale == "1-10"_L1) {
        return RatingScale::OneToTen;
    }
    if(scale == "1-100"_L1) {
        return RatingScale::OneToHundred;
    }
    return RatingScale::Automatic;
}

PopmMapping popmMappingFromString(QString mapping)
{
    mapping = normalisedConfigKey(std::move(mapping));
    for(const auto& descriptor : PopmMappingTable) {
        if(mapping == normalisedConfigKey(popmMappingKey(descriptor.mapping))) {
            return descriptor.mapping;
        }
    }

    if(mapping == "COMMONFIVESTAR"_L1) {
        return PopmMapping::CommonFiveStar;
    }
    if(mapping == "LINEARBYTE"_L1) {
        return PopmMapping::LinearByte;
    }
    return PopmMapping::Default;
}

RatingTagPolicy ratingTagPolicy()
{
    const FySettings settings;
    return {
        .readTag   = settings.value(RatingSettings::ReadTag, QLatin1StringView{RatingSettings::DefaultAutomatic})
                         .toString()
                         .trimmed()
                         .toUpper(),
        .readScale = ratingScaleFromString(
            settings.value(RatingSettings::ReadScale, QLatin1StringView{RatingSettings::DefaultAutomatic}).toString()),
        .writeTag   = settings.value(RatingSettings::WriteTag, QLatin1StringView{RatingSettings::DefaultFmpsRatingTag})
                          .toString()
                          .trimmed()
                          .toUpper(),
        .writeScale = ratingScaleFromString(
            settings.value(RatingSettings::WriteScale, QLatin1StringView{RatingSettings::DefaultNormalizedRatingScale})
                .toString()),
        .readId3Popm  = settings.value(RatingSettings::ReadId3Popm, RatingSettings::DefaultReadId3Popm).toBool(),
        .writeId3Popm = settings.value(RatingSettings::WriteId3Popm, RatingSettings::DefaultWriteId3Popm).toBool(),
        .popmOwner    = settings.value(RatingSettings::PopmOwner, QString{}).toString(),
        .popmMapping  = popmMappingFromString(
            settings.value(RatingSettings::PopmMapping, QLatin1StringView{RatingSettings::DefaultPopmMapping})
                .toString()),
        .readAsfSharedRating
        = settings.value(RatingSettings::ReadAsfSharedRating, RatingSettings::DefaultReadAsfSharedRating).toBool(),
        .writeAsfSharedRating
        = settings.value(RatingSettings::WriteAsfSharedRating, RatingSettings::DefaultWriteAsfSharedRating).toBool(),
    };
}

std::optional<float> normalisedTextRating(const QString& rawRating, RatingScale scale, bool ratingTagFallback)
{
    bool ok{false};
    const float rating = rawRating.toFloat(&ok);
    if(!ok) {
        return {};
    }

    switch(scale) {
        case RatingScale::Normalized01:
            return rating > 0.0F && rating <= 1.0F ? std::optional{rating} : std::nullopt;
        case RatingScale::OneToFive:
            return rating > 0.0F && rating <= 5.0F ? std::optional{rating / 5.0F} : std::nullopt;
        case RatingScale::OneToTen:
            return rating > 0.0F && rating <= 10.0F ? std::optional{rating / 10.0F} : std::nullopt;
        case RatingScale::OneToHundred:
            return rating > 0.0F && rating <= 100.0F ? std::optional{rating / 100.0F} : std::nullopt;
        case RatingScale::Automatic:
            break;
    }

    if(!ratingTagFallback) {
        return rating > 0.0F && rating <= 1.0F ? std::optional{rating} : std::nullopt;
    }

    if(rating > 0.0F && rating <= 1.0F) {
        return rating;
    }
    if(rating > 1.0F && rating <= 5.0F) {
        return rating / 5.0F;
    }
    if(rating > 5.0F && rating <= 10.0F) {
        return rating / 10.0F;
    }
    if(rating > 10.0F && rating <= 100.0F) {
        return rating / 100.0F;
    }

    return {};
}

QString formatTextRating(float rating, RatingScale scale)
{
    if(rating <= 0.0F) {
        return {};
    }

    switch(scale) {
        case RatingScale::OneToFive:
            return QString::number(std::lround(rating * 5.0F));
        case RatingScale::OneToTen:
            return QString::number(std::lround(rating * 10.0F));
        case RatingScale::OneToHundred:
            return QString::number(std::lround(rating * 100.0F));
        case RatingScale::Automatic:
        case RatingScale::Normalized01:
            return QString::number(rating);
    }

    return QString::number(rating);
}

float asfSharedUserRatingToRating(int rating)
{
    rating = std::clamp(rating, 0, 100);
    if(rating == 0) {
        return 0.0F;
    }
    if(rating < 25) {
        return 0.2F;
    }
    if(rating < 50) {
        return 0.4F;
    }
    if(rating < 75) {
        return 0.6F;
    }
    if(rating < 99) {
        return 0.8F;
    }
    return 1.0F;
}

int ratingToAsfSharedUserRating(float rating)
{
    rating = std::clamp(rating, 0.0F, 1.0F);
    if(rating <= 0.0F) {
        return 0;
    }

    const int stars = std::clamp(static_cast<int>(std::lround(rating * 5.0F)), 1, 5);
    switch(stars) {
        case 1:
            return 1;
        case 2:
            return 25;
        case 3:
            return 50;
        case 4:
            return 75;
        default:
            return 99;
    }
}

float popmToRating(int popm, PopmMapping mapping)
{
    popm = std::clamp(popm, 0, 255);

    if(mapping == PopmMapping::LinearByte) {
        return static_cast<float>(popm) / 255.0F;
    }

    if(mapping == PopmMapping::CommonFiveStar) {
        if(popm == 0) {
            return 0.0F;
        }
        if(popm < 32) {
            return 0.2F;
        }
        if(popm < 96) {
            return 0.4F;
        }
        if(popm < 160) {
            return 0.6F;
        }
        if(popm < 224) {
            return 0.8F;
        }
        return 1.0F;
    }

    // Reference: https://www.mediamonkey.com/forum/viewtopic.php?f=7&t=40532&start=30#p391067
    if(popm == 0) {
        return 0.0F;
    }
    if(popm == 1) {
        return 0.2F;
    }
    if(popm < 23) {
        return 0.1F;
    }
    if(popm < 32) {
        return 0.2F;
    }
    if(popm < 64) {
        return 0.3F;
    }
    if(popm < 96) {
        return 0.4F;
    }
    if(popm < 128) {
        return 0.5F;
    }
    if(popm < 160) {
        return 0.6F;
    }
    if(popm < 196) {
        return 0.7F;
    }
    if(popm < 224) {
        return 0.8F;
    }
    if(popm < 255) {
        return 0.9F;
    }
    return 1.0F;
}

int ratingToPopm(float rating, PopmMapping mapping)
{
    rating = std::clamp(rating, 0.0F, 1.0F);
    if(rating <= 0.0F) {
        return 0;
    }

    if(mapping == PopmMapping::CommonFiveStar) {
        const int stars = std::clamp(static_cast<int>(std::lround(rating * 5.0F)), 1, 5);
        switch(stars) {
            case 1:
                return 1;
            case 2:
                return 64;
            case 3:
                return 128;
            case 4:
                return 196;
            default:
                return 255;
        }
    }

    if(mapping == PopmMapping::LinearByte) {
        return std::clamp(static_cast<int>(std::lround(rating * 255.0F)), 1, 255);
    }

    if(rating < 0.1F) {
        return 0;
    }
    if(rating < 0.2F) {
        return 13;
    }
    if(rating < 0.3F) {
        return 1;
    }
    if(rating < 0.4F) {
        return 54;
    }
    if(rating < 0.5F) {
        return 64;
    }
    if(rating < 0.6F) {
        return 118;
    }
    if(rating < 0.7F) {
        return 128;
    }
    if(rating < 0.8F) {
        return 186;
    }
    if(rating < 0.9F) {
        return 196;
    }
    if(rating < 1.0F) {
        return 242;
    }

    return 255;
}
} // namespace Fooyin
