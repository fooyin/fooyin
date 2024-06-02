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

#include "wavebarsettings.h"

#include "wavebarcolours.h"

#include <utils/settings/settingsmanager.h>

namespace Fooyin::WaveBar {
WaveBarSettings::WaveBarSettings(SettingsManager* settingsManager)
    : m_settings{settingsManager}
{
    using namespace Settings::WaveBar;

    qRegisterMetaType<Fooyin::WaveBar::Colours>("Fooyin::WaveBar::Colours");

    m_settings->createSetting<Downmix>(0, QStringLiteral("WaveBar/Downmix"));
    m_settings->createSetting<ShowCursor>(true, QStringLiteral("WaveBar/ShowCursor"));
    m_settings->createSetting<CursorWidth>(3, QStringLiteral("WaveBar/CursorWidth"));
    m_settings->createSetting<ColourOptions>(QVariant::fromValue(Colours{}), QStringLiteral("WaveBar/Colours"));
    m_settings->createSetting<Mode>(static_cast<int>(Default), QStringLiteral("WaveBar/Mode"));
    m_settings->createSetting<BarWidth>(1, QStringLiteral("WaveBar/BarWidth"));
    m_settings->createSetting<BarGap>(0, QStringLiteral("WaveBar/BarGap"));
    m_settings->createSetting<MaxScale>(1.0, QStringLiteral("WaveBar/MaxScale"));
    m_settings->createSetting<CentreGap>(0, QStringLiteral("WaveBar/CentreGap"));
    m_settings->createSetting<ChannelScale>(0.9, QStringLiteral("WaveBar/ChannelScale"));
    m_settings->createSetting<NumSamples>(2048, QStringLiteral("WaveBar/NumSamples"));
}
} // namespace Fooyin::WaveBar
