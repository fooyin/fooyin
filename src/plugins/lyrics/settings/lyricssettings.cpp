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

    using namespace Settings::Lyrics;

    m_settings->createSetting<Paths>(defaultPaths(), QStringLiteral("Lyrics/Paths"));
    m_settings->createSetting<Settings::Lyrics::Colours>(QVariant{}, QStringLiteral("Lyrics/Colours"));
    m_settings->createSetting<LineFont>(QString{}, QStringLiteral("Lyrics/CurrentLineFont"));
    m_settings->createSetting<WordLineFont>(QString{}, QStringLiteral("Lyrics/CurrentWordLineFont"));
    m_settings->createSetting<WordFont>(QString{}, QStringLiteral("Lyrics/CurrentWordFont"));
    m_settings->createSetting<Alignment>(static_cast<int>(Qt::AlignCenter), QStringLiteral("Lyrics/Alignment"));
    m_settings->createSetting<ScrollDuration>(500, QStringLiteral("Lyrics/ScrollDuration"));
    m_settings->createSetting<SearchTags>(defaultTags(), QStringLiteral("Lyrics/SearchTags"));
    m_settings->createSetting<LineSpacing>(5, QStringLiteral("Lyrics/LineSpacing"));
    m_settings->createSetting<Margins>(QVariant::fromValue(QMargins{20, 20, 20, 20}), QStringLiteral("Lyrics/Margins"));
    m_settings->createSetting<NoLyricsScript>(LyricsWidget::defaultNoLyricsScript(),
                                              QStringLiteral("Lyrics/NoLyricsScript"));
    m_settings->createSetting<SeekOnClick>(true, QStringLiteral("Lyrics/SeekOnClick"));
    m_settings->createSetting<ShowScrollbar>(true, QStringLiteral("Lyrics/ShowScrollbar"));
    m_settings->createSetting<Settings::Lyrics::ScrollMode>(static_cast<int>(ScrollMode::Synced),
                                                            QStringLiteral("Lyrics/ScrollMode"));
    m_settings->createSetting<TitleField>(QStringLiteral("%title%"), QStringLiteral("Lyrics/TitleField"));
    m_settings->createSetting<AlbumField>(QStringLiteral("%album%"), QStringLiteral("Lyrics/AlbumField"));
    m_settings->createSetting<ArtistField>(QStringLiteral("%artist%"), QStringLiteral("Lyrics/ArtistField"));
    m_settings->createSetting<Settings::Lyrics::SaveScheme>(static_cast<int>(SaveScheme::Manual),
                                                            QStringLiteral("Lyrics/SaveScheme"));
    m_settings->createSetting<Settings::Lyrics::SaveMethod>(static_cast<int>(SaveMethod::Tag),
                                                            QStringLiteral("Lyrics/SaveMethod"));
    m_settings->createSetting<Settings::Lyrics::SavePrefer>(static_cast<int>(SavePrefer::None),
                                                            QStringLiteral("Lyrics/SavePrefer"));
    m_settings->createSetting<SaveSyncedTag>(QStringLiteral("LYRICS"), QStringLiteral("Lyrics/SaveSyncedTag"));
    m_settings->createSetting<SaveUnsyncedTag>(QStringLiteral("UNSYNCED LYRICS"),
                                               QStringLiteral("Lyrics/SaveUnsyncedTag"));
    m_settings->createSetting<SaveDir>(QStringLiteral("%path%"), QStringLiteral("Lyrics/SaveDir"));
    m_settings->createSetting<SaveFilename>(QStringLiteral("[%artist% - ]%title%"),
                                            QStringLiteral("Lyrics/SaveFilename"));
    m_settings->createSetting<SkipRemaining>(true, QStringLiteral("Lyrics/SkipRemaining"));
    m_settings->createSetting<SkipExternal>(true, QStringLiteral("Lyrics/SkipExternal"));
    m_settings->createSetting<AutoSearch>(true, QStringLiteral("Lyrics/AutoSearch"));
    m_settings->createSetting<CollapseDuplicates>(true, QStringLiteral("Lyrics/CollapseDuplicateLines"));
}
} // namespace Fooyin::Lyrics
