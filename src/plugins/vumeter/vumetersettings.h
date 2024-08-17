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

namespace Settings::VuMeter {
Q_NAMESPACE

enum VuMeterSettings : uint32_t
{
    PeakHoldTime   = 0 | Type::Double,
    FalloffTime    = 1 | Type::Double,
    ChannelSpacing = 2 | Type::Int,
    BarSize        = 3 | Type::Int,
    BarSpacing     = 4 | Type::Int,
    BarSections    = 5 | Type::Int,
    SectionSpacing = 6 | Type::Int,
    MeterColours   = 7 | Type::Variant,
};
Q_ENUM_NS(VuMeterSettings)
} // namespace Settings::VuMeter

namespace VuMeter {
constexpr auto VuMeterPage = "Fooyin.Page.VuMeter";

class VuMeterSettings
{
public:
    explicit VuMeterSettings(SettingsManager* settings);

private:
    SettingsManager* m_settings;
};
} // namespace VuMeter
} // namespace Fooyin
