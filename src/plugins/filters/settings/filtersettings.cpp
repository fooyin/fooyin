/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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
#include <QPalette>

Q_LOGGING_CATEGORY(FILTERS, "fy.filters")

namespace Fooyin::Filters {
FiltersSettings::FiltersSettings(SettingsManager* settingsManager)
    : m_settings{settingsManager}
{
    using namespace Settings::Filters;

    m_settings->createSetting<FilterAltColours>(false, QStringLiteral("Filters/AlternatingColours"));
    m_settings->createSetting<FilterHeader>(true, QStringLiteral("Filters/Header"));
    m_settings->createSetting<FilterScrollBar>(true, QStringLiteral("Filters/Scrollbar"));
    m_settings->createSetting<FilterDoubleClick>(1, QStringLiteral("Filters/DoubleClickBehaviour"));
    m_settings->createSetting<FilterMiddleClick>(0, QStringLiteral("Filters/MiddleClickBehaviour"));
    m_settings->createSetting<FilterPlaylistEnabled>(true, QStringLiteral("Filters/SelectionPlaylistEnabled"));
    m_settings->createSetting<FilterAutoSwitch>(true, QStringLiteral("Filters/AutoSwitchSelectionPlaylist"));
    m_settings->createSetting<FilterAutoPlaylist>(QStringLiteral("Filter Results"),
                                                  QStringLiteral("Filters/SelectionPlaylistName"));
    m_settings->createSetting<FilterFont>(QString{}, QStringLiteral("Filters/Font"));
    m_settings->createSetting<FilterColour>(QApplication::palette().text().color().name(),
                                            QStringLiteral("Filters/Colour"));
    m_settings->createSetting<FilterRowHeight>(0, QStringLiteral("Filters/RowHeight"));
    m_settings->createSetting<FilterSendPlayback>(true, QStringLiteral("Filters/StartPlaybackOnSend"));
    m_settings->createSetting<FilterKeepAlive>(false, QStringLiteral("Filters/KeepAlive"));
    m_settings->createSetting<FilterIconSize>(QSize{100, 100}, QStringLiteral("Filters/IconSize"));
}
} // namespace Fooyin::Filters

#include "moc_filtersettings.cpp"
