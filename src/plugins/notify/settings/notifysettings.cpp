/*
 * Fooyin
 * Copyright 2025, ripdog <https://github.com/ripdog>
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

#include "notifysettings.h"

#include <utils/settings/settingsmanager.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Notify {
NotifySettings::NotifySettings(SettingsManager* settingsManager)
    : m_settings{settingsManager}
{
    using namespace Settings::Notify;

    m_settings->createSetting<Enabled>(true, u"Notify/Enabled"_s);
    m_settings->createSetting<TitleField>(u"%title%"_s, u"Notify/TitleField"_s);
    m_settings->createSetting<BodyField>(u"%artist%[ - %album%]"_s, u"Notify/BodyField"_s);
    m_settings->createSetting<ShowAlbumArt>(true, u"Notify/ShowAlbumArt"_s);
    m_settings->createSetting<Timeout>(-1, u"Notify/Timeout"_s);
}
} // namespace Fooyin::Notify
