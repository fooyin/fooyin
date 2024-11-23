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

#include "vumetercolours.h"

#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QColor>
#include <QPalette>

using namespace Qt::StringLiterals;

namespace Fooyin::VuMeter {
VuMeterSettings::VuMeterSettings(SettingsManager* settings)
    : m_settings{settings}
{
    using namespace Settings::VuMeter;

    qRegisterMetaType<Fooyin::VuMeter::Colours>("Fooyin::VuMeter::Colours");

    m_settings->createSetting<PeakHoldTime>(1.5, u"VuMeter/PeakHoldTime"_s);
    m_settings->createSetting<FalloffTime>(13.0, u"VuMeter/FalloffTime"_s);
    m_settings->createSetting<ChannelSpacing>(1, u"VuMeter/ChannelSpacing"_s);
    m_settings->createSetting<BarSize>(0, u"VuMeter/BarSize"_s);
    m_settings->createSetting<BarSpacing>(1, u"VuMeter/BarSpacing"_s);
    m_settings->createSetting<BarSections>(1, u"VuMeter/BarSections"_s);
    m_settings->createSetting<SectionSpacing>(1, u"VuMeter/SectionSpacing"_s);
    m_settings->createSetting<MeterColours>(QVariant{}, u"VuMeter/Colours"_s);
}
} // namespace Fooyin::VuMeter
