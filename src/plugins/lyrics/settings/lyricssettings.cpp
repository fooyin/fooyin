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
#include "lyricswidget.h"

#include <utils/settings/settingsmanager.h>

namespace {
QStringList defaultPaths()
{
    static const QStringList paths{QStringLiteral("%path%/%filename%.lrc")};
    return paths;
}

QStringList defaultTags()
{
    static const QStringList paths{QStringLiteral("LYRICS"), QStringLiteral("SYNCEDLYRICS"),
                                   QStringLiteral("UNSYNCEDLYRICS"), QStringLiteral("UNSYNCED LYRICS")};
    return paths;
}
} // namespace

namespace Fooyin::Lyrics {
LyricsSettings::LyricsSettings(SettingsManager* settingsManager)
    : m_settings{settingsManager}
{
    qRegisterMetaType<QMargins>("QMargins");
    qRegisterMetaType<Fooyin::Lyrics::Colours>("Fooyin::Lyrics::Colours");

    m_settings->createSetting<Settings::Lyrics::Paths>(defaultPaths(), QStringLiteral("Lyrics/Paths"));
    m_settings->createSetting<Settings::Lyrics::Colours>(QVariant{}, QStringLiteral("Lyrics/Colours"));
    m_settings->createSetting<Settings::Lyrics::LineFont>(QString{}, QStringLiteral("Lyrics/CurrentLineFont"));
    m_settings->createSetting<Settings::Lyrics::WordLineFont>(QString{}, QStringLiteral("Lyrics/CurrentWordLineFont"));
    m_settings->createSetting<Settings::Lyrics::WordFont>(QString{}, QStringLiteral("Lyrics/CurrentWordFont"));
    m_settings->createSetting<Settings::Lyrics::Alignment>(static_cast<int>(Qt::AlignCenter),
                                                           QStringLiteral("Lyrics/Alignment"));
    m_settings->createSetting<Settings::Lyrics::ScrollDuration>(500, QStringLiteral("Lyrics/ScrollDuration"));
    m_settings->createSetting<Settings::Lyrics::SearchTags>(defaultTags(), QStringLiteral("Lyrics/SearchTags"));
    m_settings->createSetting<Settings::Lyrics::LineSpacing>(5, QStringLiteral("Lyrics/LineSpacing"));
    m_settings->createSetting<Settings::Lyrics::Margins>(QVariant::fromValue(QMargins{20, 20, 20, 20}),
                                                         QStringLiteral("Lyrics/Margins"));
    m_settings->createSetting<Settings::Lyrics::NoLyricsScript>(LyricsWidget::defaultNoLyricsScript(),
                                                                QStringLiteral("Lyrics/NoLyricsScript"));
    m_settings->createSetting<Settings::Lyrics::SeekOnClick>(true, QStringLiteral("Lyrics/SeekOnClick"));
    m_settings->createSetting<Settings::Lyrics::ShowScrollbar>(true, QStringLiteral("Lyrics/ShowScrollbar"));
    m_settings->createSetting<Settings::Lyrics::ScrollMode>(static_cast<int>(ScrollMode::Synced),
                                                            QStringLiteral("Lyrics/ScrollMode"));
}
} // namespace Fooyin::Lyrics
