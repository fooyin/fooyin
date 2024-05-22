/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <utils/settings/settingsentry.h>

namespace Fooyin {
class SettingsManager;

namespace Settings::WaveBar {
Q_NAMESPACE

enum WaveBarSettings : uint32_t
{
    Downmix       = 1 | Type::Int,
    ShowCursor    = 2 | Type::Bool,
    CursorWidth   = 3 | Type::Int,
    ColourOptions = 4 | Type::Variant,
    Mode          = 5 | Type::Int,
    BarWidth      = 6 | Type::Int,
    BarGap        = 7 | Type::Int,
    MaxScale      = 8 | Type::Double,
    CentreGap     = 9 | Type::Int,
    ChannelScale  = 10 | Type::Double,
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

enum class DownmixOption
{
    Off = 0,
    Stereo,
    Mono,
};

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
