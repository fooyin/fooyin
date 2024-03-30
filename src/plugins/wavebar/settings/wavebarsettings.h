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

enum class ValueOptions
{
    All = 0,
    MinMax,
    RMS,
};

enum class DownmixOption
{
    Off = 0,
    Stereo,
    Mono,
};

namespace Settings::WaveBar {
Q_NAMESPACE

enum WaveBarSettings : uint32_t
{
    Downmix            = 1 | Type::Int,
    ChannelHeightScale = 2 | Type::Double,
    CursorWidth        = 3 | Type::Double,
    ColourOptions      = 4 | Type::Variant,
    DrawValues         = 5 | Type::Int,
    BarWidth           = 6 | Type::Int,
    BarGap             = 7 | Type::Int,
};
Q_ENUM_NS(WaveBarSettings)
} // namespace Settings::WaveBar

namespace WaveBar {
class WaveBarSettings
{
public:
    explicit WaveBarSettings(SettingsManager* settingsManager);

private:
    SettingsManager* m_settings;
};
} // namespace WaveBar
} // namespace Fooyin
