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

#include "filtercontroller.h"

#include <utils/settings/settingsmanager.h>

#include <QApplication>
#include <QPalette>

Q_LOGGING_CATEGORY(FILTERS, "fy.filters")

using namespace Qt::StringLiterals;

namespace Fooyin::Filters {
FiltersSettings::FiltersSettings(SettingsManager* settingsManager)
    : m_settings{settingsManager}
{
    using namespace Settings::Filters;

    m_settings->createSetting<FilterDoubleClick>(1, u"Filters/DoubleClickBehaviour"_s);
    m_settings->createSetting<FilterMiddleClick>(0, u"Filters/MiddleClickBehaviour"_s);
    m_settings->createSetting<FilterPlaylistEnabled>(true, u"Filters/SelectionPlaylistEnabled"_s);
    m_settings->createSetting<FilterAutoSwitch>(true, u"Filters/AutoSwitchSelectionPlaylist"_s);
    m_settings->createSetting<FilterAutoPlaylist>(FilterController::defaultPlaylistName(),
                                                  u"Filters/SelectionPlaylistName"_s);
    m_settings->createSetting<FilterRowHeight>(0, u"Filters/RowHeight"_s);
    m_settings->createSetting<FilterSendPlayback>(true, u"Filters/StartPlaybackOnSend"_s);
    m_settings->createSetting<FilterKeepAlive>(false, u"Filters/KeepAlive"_s);
    m_settings->createSetting<FilterIconSize>(QSize{100, 100}, u"Filters/IconSize"_s);
}
} // namespace Fooyin::Filters

#include "moc_filtersettings.cpp"
