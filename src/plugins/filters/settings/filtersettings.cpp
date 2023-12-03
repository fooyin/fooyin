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

#include "filterfwd.h"

#include <utils/settings/settingsmanager.h>

using namespace Qt::Literals::StringLiterals;

namespace Fooyin::Filters {
FiltersSettings::FiltersSettings(SettingsManager* settingsManager)
    : m_settings{settingsManager}
{
    m_settings->createSetting<Settings::Filters::FilterAltColours>(false, u"Filters"_s);
    m_settings->createSetting<Settings::Filters::FilterHeader>(true, u"Filters"_s);
    m_settings->createSetting<Settings::Filters::FilterScrollBar>(true, u"Filters"_s);
    m_settings->createSetting<Settings::Filters::FilterColumns>(QByteArray{}, u"Filters"_s);
    m_settings->createSetting<Settings::Filters::FilterAppearance>(QVariant::fromValue(FilterOptions{}), u"Filters"_s);
    m_settings->createSetting<Settings::Filters::FilterDoubleClick>(1, u"Filters"_s);
    m_settings->createSetting<Settings::Filters::FilterMiddleClick>(0, u"Filters"_s);
    m_settings->createSetting<Settings::Filters::FilterPlaylistEnabled>(true, u"Filters"_s);
    m_settings->createSetting<Settings::Filters::FilterAutoSwitch>(true, u"Filters"_s);
    m_settings->createSetting<Settings::Filters::FilterAutoPlaylist>("Filter Results", u"Filters"_s);

    m_settings->loadSettings();
}
} // namespace Fooyin::Filters

#include "moc_filtersettings.cpp"
