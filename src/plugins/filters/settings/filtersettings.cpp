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

#include "filterfwd.h"

#include <utils/settings/settingsmanager.h>

using namespace Qt::Literals::StringLiterals;

namespace Fooyin::Filters {
FiltersSettings::FiltersSettings(SettingsManager* settingsManager)
    : m_settings{settingsManager}
{
    using namespace Settings::Filters;

    qRegisterMetaType<Fooyin::Filters::FilterOptions>("Fooyin::Filters::FilterOptions");

    m_settings->createSetting<FilterAltColours>(false, u"Filters/AlternatingColours"_s);
    m_settings->createSetting<FilterHeader>(true, u"Filters/Header"_s);
    m_settings->createSetting<FilterScrollBar>(true, u"Filters/Scrollbar"_s);
    m_settings->createSetting<FilterAppearance>(QVariant::fromValue(FilterOptions{}), u"Filters/Appearance"_s);
    m_settings->createSetting<FilterDoubleClick>(1, u"Filters/DoubleClickBehaviour"_s);
    m_settings->createSetting<FilterMiddleClick>(0, u"Filters/MiddleClickBehaviour"_s);
    m_settings->createSetting<FilterPlaylistEnabled>(true, u"Filters/SelectionPlaylistEnabled"_s);
    m_settings->createSetting<FilterAutoSwitch>(true, u"Filters/AutoSwitchSelectionPlaylist"_s);
    m_settings->createSetting<FilterAutoPlaylist>("Filter Results", u"Filters/SelectionPlaylistName"_s);
}
} // namespace Fooyin::Filters

#include "moc_filtersettings.cpp"
