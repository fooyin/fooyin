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

#include <core/engine/visualisationservice.h>
#include <gui/trackdisplay.h>

#include <QColor>

#include <array>
#include <vector>

namespace Fooyin::Spectrogram {
constexpr auto MinTimeColumns = 256;
constexpr auto MaxTimeColumns = 8192;
constexpr std::array FftSizes{256, 512, 1024, 2048, 4096, 8192, 16384};

enum class ChannelMode : uint8_t
{
    Unchanged = 0,
    Mono,
    FrontOnly,
    BackOnly,
};

enum class PresentationMode : uint8_t
{
    Scrolling = 0,
    Stationary,
    FullTrack,
};

struct SpectrogramConfig
{
    ChannelMode channelMode{ChannelMode::Unchanged};
    PresentationMode presentationMode{PresentationMode::Scrolling};
    int trackPreference{static_cast<int>(TrackDisplayPreference::PlayingTrack)};
    bool logFrequency{true};
    bool clearOnTrackChange{false};
    bool showTopLabels{false};
    bool showBottomLabels{true};
    bool showLeftLabels{true};
    bool showRightLabels{false};
    bool showCursor{true};
    int cursorWidth{3};
    QColor cursorColour;
    QColor seekingCursorColour;
    QColor playedOverlayColour;
    QColor unplayedOverlayColour;
    int channelSpacing{0};
    int fftSize{8192};
    int timeColumns{1024};
    int minDb{-70};
    int maxDb{0};
    int windowFunction{static_cast<int>(VisualisationSession::SpectrumWindowFunction::BlackmanHarris)};
    std::vector<QColor> colours;
};

} // namespace Fooyin::Spectrogram
