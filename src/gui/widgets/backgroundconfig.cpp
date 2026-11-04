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

#include <gui/widgets/backgroundconfig.h>

#include "pixmapfadecontroller.h"

#include <core/track.h>

#include <QJsonObject>

using namespace Qt::StringLiterals;

namespace Fooyin {
void BackgroundConfig::normalise()
{
    const BackgroundConfig defaults;

    maxSize      = std::clamp(maxSize, 0, 4096);
    blur         = std::clamp(blur, 0, 100);
    opacity      = std::clamp(opacity, 0, 100);
    fadeDuration = std::clamp(fadeDuration, 0, PixmapFadeController::MaxDurationMs);

    if(imageMode < static_cast<int>(BackgroundImageMode::None)
       || imageMode > static_cast<int>(BackgroundImageMode::Global)) {
        imageMode = defaults.imageMode;
    }
    if(coverType < static_cast<int>(Track::Cover::Front) || coverType > static_cast<int>(Track::Cover::Other)) {
        coverType = defaults.coverType;
    }
    if(scaling < static_cast<int>(BackgroundImageScaling::ScaledAndCropped)
       || scaling > static_cast<int>(BackgroundImageScaling::OriginalSize)) {
        scaling = defaults.scaling;
    }
    if(position < static_cast<int>(BackgroundImagePosition::TopLeft)
       || position > static_cast<int>(BackgroundImagePosition::BottomRight)) {
        position = defaults.position;
    }
    if(trackPreference < TrackDisplayPreference::PlayingTrack
       || trackPreference > TrackDisplayPreference::PlayingTrackBlankWhenStopped) {
        trackPreference = defaults.trackPreference;
    }
}

void BackgroundConfig::saveLayout(QJsonObject& layout) const
{
    layout["BackgroundImageMode"_L1]       = imageMode;
    layout["BackgroundCustomImage"_L1]     = customImage;
    layout["BackgroundCoverType"_L1]       = coverType;
    layout["BackgroundScaling"_L1]         = scaling;
    layout["BackgroundPosition"_L1]        = position;
    layout["BackgroundMaxSize"_L1]         = maxSize;
    layout["BackgroundBlur"_L1]            = blur;
    layout["BackgroundOpacity"_L1]         = opacity;
    layout["BackgroundFadeDuration"_L1]    = fadeDuration;
    layout["BackgroundTrackPreference"_L1] = static_cast<int>(trackPreference);
}

void BackgroundConfig::loadLayout(const QJsonObject& layout)
{
    imageMode    = layout.value("BackgroundImageMode"_L1).toInt(imageMode);
    coverType    = layout.value("BackgroundCoverType"_L1).toInt(coverType);
    scaling      = layout.value("BackgroundScaling"_L1).toInt(scaling);
    position     = layout.value("BackgroundPosition"_L1).toInt(position);
    maxSize      = layout.value("BackgroundMaxSize"_L1).toInt(maxSize);
    blur         = layout.value("BackgroundBlur"_L1).toInt(blur);
    opacity      = layout.value("BackgroundOpacity"_L1).toInt(opacity);
    fadeDuration = layout.value("BackgroundFadeDuration"_L1).toInt(fadeDuration);

    customImage     = layout.value("BackgroundCustomImage"_L1).toString(customImage);
    trackPreference = static_cast<TrackDisplayPreference>(
        layout.value("BackgroundTrackPreference"_L1).toInt(static_cast<int>(trackPreference)));

    normalise();
}
} // namespace Fooyin
