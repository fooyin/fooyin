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

#include "lyricssettings.h"

#include "lyricscolours.h"
#include "lyricssaver.h"
#include "lyricswidget.h"

#include <utils/settings/settingsmanager.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Lyrics::Defaults {
QStringList paths()
{
    static const QStringList values{u"%path%/%filename%.lrc"_s};
    return values;
}

QStringList searchTags()
{
    static const QStringList values{u"LYRICS"_s, u"SYNCEDLYRICS"_s, u"UNSYNCEDLYRICS"_s, u"UNSYNCED LYRICS"_s};
    return values;
}

QMargins margins()
{
    return {20, 20, 20, 20};
}
} // namespace Fooyin::Lyrics::Defaults

namespace Fooyin::Lyrics {
LyricsSettings::LyricsSettings(SettingsManager* settingsManager)
    : m_settings{settingsManager}
{
    qRegisterMetaType<QMargins>("QMargins");
    qRegisterMetaType<Fooyin::Lyrics::Colours>("Fooyin::Lyrics::Colours");
}
} // namespace Fooyin::Lyrics
