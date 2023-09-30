/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "filtersettings.h"

#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QFont>
#include <QPalette>

namespace Fy::Filters::Settings {
FiltersSettings::FiltersSettings(Utils::SettingsManager* settingsManager)
    : m_settings{settingsManager}
{
    m_settings->createSetting(Settings::FilterAltColours, false, "Filters");
    m_settings->createSetting(Settings::FilterHeader, true, "Filters");
    m_settings->createSetting(Settings::FilterScrollBar, true, "Filters");
    m_settings->createSetting(Settings::FilterFields, "", "Filters");
    m_settings->createSetting(Settings::FilterFont, QApplication::font().toString(), "Filters");

    QByteArray colour;
    QDataStream colourStream{&colour, QIODeviceBase::WriteOnly};
    colourStream << QApplication::palette().text().color();
    m_settings->createSetting(Settings::FilterColour, colour, "Filters");

    m_settings->createSetting(Settings::FilterRowHeight, 25, "Filters");
    m_settings->createSetting(Settings::FilterDoubleClick, 1, "Filters");
    m_settings->createSetting(Settings::FilterMiddleClick, 0, "Filters");
    m_settings->createSetting(Settings::FilterPlaylistEnabled, true, "Filters");
    m_settings->createSetting(Settings::FilterAutoSwitch, true, "Filters");
    m_settings->createSetting(Settings::FilterAutoPlaylist, "Filter Results", "Filters");

    m_settings->loadSettings();
}
} // namespace Fy::Filters::Settings
