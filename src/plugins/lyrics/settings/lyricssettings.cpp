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

namespace {
QStringList defaultPaths()
{
    static const QStringList paths{u"%path%/%filename%.lrc"_s};
    return paths;
}

QStringList defaultTags()
{
    static const QStringList paths{u"LYRICS"_s, u"SYNCEDLYRICS"_s, u"UNSYNCEDLYRICS"_s, u"UNSYNCED LYRICS"_s};
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

    m_settings->createSetting<Paths>(defaultPaths(), u"Lyrics/Paths"_s);
    m_settings->createSetting<Settings::Lyrics::Colours>(QVariant{}, u"Lyrics/Colours"_s);
    m_settings->createSetting<LineFont>(QString{}, u"Lyrics/CurrentLineFont"_s);
    m_settings->createSetting<WordLineFont>(QString{}, u"Lyrics/CurrentWordLineFont"_s);
    m_settings->createSetting<WordFont>(QString{}, u"Lyrics/CurrentWordFont"_s);
    m_settings->createSetting<Alignment>(static_cast<int>(Qt::AlignCenter), u"Lyrics/Alignment"_s);
    m_settings->createSetting<ScrollDuration>(500, u"Lyrics/ScrollDuration"_s);
    m_settings->createSetting<SearchTags>(defaultTags(), u"Lyrics/SearchTags"_s);
    m_settings->createSetting<LineSpacing>(5, u"Lyrics/LineSpacing"_s);
    m_settings->createSetting<Margins>(QVariant::fromValue(QMargins{20, 20, 20, 20}), u"Lyrics/Margins"_s);
    m_settings->createSetting<NoLyricsScript>(LyricsWidget::defaultNoLyricsScript(), u"Lyrics/NoLyricsScript"_s);
    m_settings->createSetting<SeekOnClick>(true, u"Lyrics/SeekOnClick"_s);
    m_settings->createSetting<ShowScrollbar>(true, u"Lyrics/ShowScrollbar"_s);
    m_settings->createSetting<Settings::Lyrics::ScrollMode>(static_cast<int>(ScrollMode::Synced),
                                                            u"Lyrics/ScrollMode"_s);
    m_settings->createSetting<TitleField>(u"%title%"_s, u"Lyrics/TitleField"_s);
    m_settings->createSetting<AlbumField>(u"%album%"_s, u"Lyrics/AlbumField"_s);
    m_settings->createSetting<ArtistField>(u"%artist%"_s, u"Lyrics/ArtistField"_s);
    m_settings->createSetting<Settings::Lyrics::SaveScheme>(static_cast<int>(SaveScheme::Manual),
                                                            u"Lyrics/SaveScheme"_s);
    m_settings->createSetting<Settings::Lyrics::SaveMethod>(static_cast<int>(SaveMethod::Tag), u"Lyrics/SaveMethod"_s);
    m_settings->createSetting<Settings::Lyrics::SavePrefer>(static_cast<int>(SavePrefer::None), u"Lyrics/SavePrefer"_s);
    m_settings->createSetting<SaveSyncedTag>(u"LYRICS"_s, u"Lyrics/SaveSyncedTag"_s);
    m_settings->createSetting<SaveUnsyncedTag>(u"UNSYNCED LYRICS"_s, u"Lyrics/SaveUnsyncedTag"_s);
    m_settings->createSetting<SaveDir>(u"%path%"_s, u"Lyrics/SaveDir"_s);
    m_settings->createSetting<SaveFilename>(u"%filename%"_s, u"Lyrics/SaveFilename"_s);
    m_settings->createSetting<SkipRemaining>(true, u"Lyrics/SkipRemaining"_s);
    m_settings->createSetting<SkipExternal>(true, u"Lyrics/SkipExternal"_s);
    m_settings->createSetting<AutoSearch>(true, u"Lyrics/AutoSearch"_s);
    m_settings->createSetting<SaveOptions>(static_cast<int>(LyricsSaver::SaveOption::None), u"Lyrics/SaveOptions"_s);
    m_settings->createSetting<MatchThreshold>(75, u"Lyrics/MatchThreshold"_s);
}
} // namespace Fooyin::Lyrics
