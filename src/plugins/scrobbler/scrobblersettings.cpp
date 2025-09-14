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

#include "scrobblersettings.h"

#include <utils/settings/settingsmanager.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Scrobbler {
ScrobblerSettings::ScrobblerSettings(SettingsManager* settings)
{
    using namespace Settings::Scrobbler;

    settings->createSetting<ScrobblingEnabled>(false, u"Scrobbling/Enabled"_s);
    settings->createSetting<ScrobblingDelay>(0, u"Scrobbling/Delay"_s);
    settings->createSetting<PreferAlbumArtist>(false, u"Scrobbling/PreferAlbumArtist"_s);
    settings->createSetting<EnableScrobbleFilter>(false, u"Scrobbling/EnableScrobbleFilter"_s);
    settings->createSetting<ScrobbleFilter>(u""_s, u"Scrobbling/Filter"_s);
    settings->createSetting<ServicesData>(QByteArray{}, u"Scrobbling/ServicesData"_s);
}
} // namespace Fooyin::Scrobbler
