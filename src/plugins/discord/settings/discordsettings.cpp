/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <LukeT1@proton.me>
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

#include "discordsettings.h"

#include <utils/settings/settingsmanager.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Discord {
DiscordSettings::DiscordSettings(SettingsManager* settingsManager)
    : m_settings{settingsManager}
{
    using namespace Settings::Discord;

    m_settings->createSetting<ClientId>(u"1252972125623685140"_s, u"Discord/ClientId"_s);
    m_settings->createSetting<ArtistField>(u"%artist%"_s, u"Discord/ArtistField"_s);
    m_settings->createSetting<AlbumField>(u"%album%"_s, u"Discord/AlbumField"_s);
    m_settings->createSetting<TitleField>(u"%title%"_s, u"Discord/TitleField"_s);
    m_settings->createSetting<ShowStateIcon>(true, u"Discord/ShowPlayStateIcon"_s);
    m_settings->createSetting<DiscordEnabled>(false, u"Discord/Enable"_s);
}
} // namespace Fooyin::Discord
