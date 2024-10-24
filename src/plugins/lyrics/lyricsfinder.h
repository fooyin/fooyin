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

#pragma once

#include "sources/lyricsource.h"

#include <core/network/networkaccessmanager.h>
#include <core/scripting/scriptparser.h>
#include <core/track.h>

namespace Fooyin {
namespace Lyrics {
struct Lyrics;

class LyricsFinder : public QObject
{
    Q_OBJECT

public:
    explicit LyricsFinder(std::shared_ptr<NetworkAccessManager> networkManager, SettingsManager* settings,
                          QObject* parent = nullptr);

    void findLyrics(const Track& track);
    void findLocalLyrics(const Track& track);

    [[nodiscard]] std::vector<LyricSource*> sources() const;

    void saveState();
    void restoreState();
    void sort();
    void reset();

signals:
    void lyricsFound(const Fooyin::Lyrics::Lyrics& lyrics);

private:
    void loadDefaults();
    void startLyricsSearch(const Track& track);
    void finishOrStartNextSource(bool forceFinish = false);
    bool findNextAvailableSource();
    void onSearchResult(const std::vector<LyricData>& data);

    std::shared_ptr<NetworkAccessManager> m_networkManager;
    SettingsManager* m_settings;

    std::vector<LyricSource*> m_sources;

    ScriptParser m_parser;
    SearchParams m_params;
    bool m_localOnly;
    int m_currentSourceIndex;
    LyricSource* m_currentSource;
};
} // namespace Lyrics
} // namespace Fooyin
