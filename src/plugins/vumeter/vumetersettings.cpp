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

#include "vumetersettings.h"

#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QColor>
#include <QPalette>

namespace Fooyin::VuMeter {
VuMeterSettings::VuMeterSettings(SettingsManager* settings)
    : m_settings{settings}
{
    using namespace Settings::VuMeter;

    m_settings->createSetting<PeakHoldTime>(1.5, QStringLiteral("VuMeter/PeakHoldTime"));
    m_settings->createSetting<FalloffTime>(13.0, QStringLiteral("VuMeter/FalloffTime"));
    m_settings->createSetting<ChannelSpacing>(1, QStringLiteral("VuMeter/ChannelSpacing"));
    m_settings->createSetting<BarSize>(0, QStringLiteral("VuMeter/BarSize"));
    m_settings->createSetting<BarSpacing>(1, QStringLiteral("VuMeter/BarSpacing"));
    m_settings->createSetting<BarSections>(1, QStringLiteral("VuMeter/BarSections"));
    m_settings->createSetting<SectionSpacing>(1, QStringLiteral("VuMeter/SectionSpacing"));
    m_settings->createSetting<MeterColours>(QVariant{}, QStringLiteral("VuMeter/Colours"));
}
} // namespace Fooyin::VuMeter
