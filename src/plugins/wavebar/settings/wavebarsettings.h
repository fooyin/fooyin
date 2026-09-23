/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <core/player/playerdefs.h>
#include <utils/settings/settingsentry.h>

namespace Fooyin {
class SettingsManager;

namespace Settings::WaveBar {
Q_NAMESPACE

enum WaveBarSettings : uint32_t
{
    NumSamples = 11 | Type::Int,
};
Q_ENUM_NS(WaveBarSettings)
} // namespace Settings::WaveBar

namespace WaveBar {
enum WaveMode : uint32_t
{
    None    = 0,
    MinMax  = 1 << 0,
    Rms     = 1 << 1,
    Silence = 1 << 2,
    Default = MinMax | Rms,
};
Q_DECLARE_FLAGS(WaveModes, WaveMode)

enum class DownmixOption : uint8_t
{
    Off = 0,
    Stereo,
    Mono,
};

enum class PeakDisplayMode : uint8_t
{
    Maximum = 0,
    Average,
    SmoothedAverage,
};

enum class TrackPreference : uint8_t
{
    PlayingTrack = 0,
    SelectedTrack,
    PlayingTrackSelectedWhenStopped,
    PlayingTrackBlankAtStartup,
    PlayingTrackBlankWhenStopped,
};

enum class PreferredTrackSource : uint8_t
{
    Playing = 0,
    Selected,
    None,
};

constexpr PreferredTrackSource preferredTrackSource(TrackPreference preference, Player::PlayState playState,
                                                    bool playbackStarted)
{
    switch(preference) {
        case TrackPreference::SelectedTrack:
            return PreferredTrackSource::Selected;
        case TrackPreference::PlayingTrackSelectedWhenStopped:
            return playState == Player::PlayState::Stopped ? PreferredTrackSource::Selected
                                                           : PreferredTrackSource::Playing;
        case TrackPreference::PlayingTrackBlankAtStartup:
            return playbackStarted ? PreferredTrackSource::Playing : PreferredTrackSource::None;
        case TrackPreference::PlayingTrackBlankWhenStopped:
            return playState == Player::PlayState::Stopped ? PreferredTrackSource::None : PreferredTrackSource::Playing;
        case TrackPreference::PlayingTrack:
        default:
            return PreferredTrackSource::Playing;
    }
}

class WaveBarSettings
{
public:
    explicit WaveBarSettings(SettingsManager* settingsManager);

private:
    SettingsManager* m_settings;
};
} // namespace WaveBar
} // namespace Fooyin

Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::WaveBar::WaveModes)
