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

#include "fygui_export.h"

#include <gui/trackdisplay.h>

#include <QString>

class QJsonObject;

namespace Fooyin {
enum class BackgroundImageMode : uint8_t
{
    None = 0,
    AlbumCover,
    Custom
};

enum class BackgroundImagePosition : uint8_t
{
    TopLeft = 0,
    Top,
    TopRight,
    Left,
    Middle,
    Right,
    BottomLeft,
    Bottom,
    BottomRight
};

enum class BackgroundImageScaling : uint8_t
{
    ScaledAndCropped = 0,
    Scaled,
    ScaledKeepProportions,
    OriginalSize
};

struct FYGUI_EXPORT BackgroundConfig
{
    int imageMode{static_cast<int>(BackgroundImageMode::None)};
    QString customImage;
    int coverType{0};
    int scaling{static_cast<int>(BackgroundImageScaling::ScaledAndCropped)};
    int position{static_cast<int>(BackgroundImagePosition::Middle)};
    int maxSize{0};
    int blur{0};
    int opacity{40};
    int fadeDuration{0};
    TrackDisplayPreference trackPreference{TrackDisplayPreference::PlayingTrack};

    bool operator==(const BackgroundConfig& other) const = default;

    void normalise();
    void saveLayout(QJsonObject& layout) const;
    void loadLayout(const QJsonObject& layout);
};
} // namespace Fooyin
