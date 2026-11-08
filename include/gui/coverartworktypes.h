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

#include <QSize>

#include <cstdint>

namespace Fooyin {
enum class ArtworkScaling : uint8_t
{
    Fit = 0,
    Fill,
    Stretch,
};

constexpr ArtworkScaling artworkScalingFromInt(int value)
{
    switch(value) {
        case static_cast<int>(ArtworkScaling::Fill):
            return ArtworkScaling::Fill;
        case static_cast<int>(ArtworkScaling::Stretch):
            return ArtworkScaling::Stretch;
        default:
            return ArtworkScaling::Fit;
    }
}

constexpr Qt::AspectRatioMode artworkAspectRatioMode(ArtworkScaling scaling)
{
    switch(scaling) {
        case ArtworkScaling::Fill:
            return Qt::KeepAspectRatioByExpanding;
        case ArtworkScaling::Stretch:
            return Qt::IgnoreAspectRatio;
        default:
            return Qt::KeepAspectRatio;
    }
}

enum class ArtworkSourcePreference : uint8_t
{
    PreferDirectory = 0,
    PreferEmbedded
};

enum class ThumbnailSize : uint16_t
{
    None        = 0,
    Tiny        = 32,
    Small       = 64,
    MediumSmall = 96,
    Medium      = 128,
    Large       = 192,
    VeryLarge   = 256,
    ExtraLarge  = 384,
    XxLarge     = 512,
    Huge        = 768,
    Full        = 1024
};

constexpr int coverThumbnailPixelSize(ThumbnailSize size)
{
    return static_cast<int>(size);
}

constexpr ThumbnailSize coverThumbnailSizeFor(const QSize& size)
{
    const int maxSize = size.width() > size.height() ? size.width() : size.height();

    if(maxSize <= 32) {
        return ThumbnailSize::Small;
    }
    if(maxSize <= 64) {
        return ThumbnailSize::MediumSmall;
    }
    if(maxSize <= 96) {
        return ThumbnailSize::Medium;
    }
    if(maxSize <= 128) {
        return ThumbnailSize::Large;
    }
    if(maxSize <= 192) {
        return ThumbnailSize::VeryLarge;
    }
    if(maxSize <= 256) {
        return ThumbnailSize::ExtraLarge;
    }
    if(maxSize <= 384) {
        return ThumbnailSize::XxLarge;
    }
    if(maxSize <= 512) {
        return ThumbnailSize::Huge;
    }

    return ThumbnailSize::Full;
}
} // namespace Fooyin
