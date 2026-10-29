/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#pragma once

#include "lyrics.h"

#include <core/scripting/scriptcontextservice.h>
#include <core/scripting/scriptparser.h>

namespace Fooyin {
struct CorePluginContext;
class SettingsManager;

namespace Lyrics {
class LyricsFinder;
class LyricsSaver;

class LyricsPlaybackService : public QObject
{
    Q_OBJECT

public:
    LyricsPlaybackService(const CorePluginContext& core, LyricsFinder* finder, LyricsSaver* saver,
                          QObject* parent = nullptr);

    [[nodiscard]] const Track& track() const;
    [[nodiscard]] const Lyrics& lyrics() const;
    [[nodiscard]] const std::vector<Lyrics>& candidates() const;
    [[nodiscard]] LyricsFinder* finder() const;
    [[nodiscard]] ScriptDataSourceId dataSource() const;

    void search();
    void selectLyrics(const Track& track, const Lyrics& lyrics);

Q_SIGNALS:
    void changed();

private:
    void updateTrack(const Track& track, bool metadataOnly);
    void updateDemand();
    void findLyrics(bool automatic);
    void updateCurrentLine();
    void applyLyrics(const Lyrics& lyrics);
    void capture(ScriptContextData& data) const;

    PlayerController* m_player;
    LyricsFinder* m_finder;
    LyricsSaver* m_saver;
    SettingsManager* m_settings;
    ScriptContextService* m_context;

    ScriptDataSourceId m_source;
    ScriptParser m_parser;
    Track m_track;
    Lyrics m_lyrics;
    std::vector<Lyrics> m_candidates;
    ScriptResult m_currentLine;
    std::pair<size_t, size_t> m_currentRange;
    bool m_lineDirty;
    bool m_searched;
    bool m_searching;
};
} // namespace Lyrics
} // namespace Fooyin
