/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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
#include "sources/lyricsource.h"

#include <core/network/networkaccessmanager.h>
#include <core/scripting/scriptparser.h>

namespace Fooyin {
class Track;

namespace Lyrics {
struct Lyrics;

struct LyricsSearchRequest
{
    Track track;
    QString title;
    QString album;
    QString artist;
    bool localOnly{false};
    bool tagOnly{false};
    bool stopOnFirstResult{false};
    bool skipExternalAfterLocalResults{false};
};

class LyricsFinder : public QObject
{
    Q_OBJECT

public:
    explicit LyricsFinder(std::shared_ptr<NetworkAccessManager> networkManager, SettingsManager* settings,
                          QObject* parent = nullptr);

    void findLyrics(const LyricsSearchRequest& request);
    void findLyrics(const Track& track);
    void findLocalLyrics(const Track& track);
    void findTagLyrics(const Track& track);

    [[nodiscard]] std::shared_ptr<NetworkAccessManager> networkManager() const;
    [[nodiscard]] std::vector<LyricSource*> sources() const;

    void saveState();
    void restoreState();
    void sort();
    void reset();

Q_SIGNALS:
    void lyricsFound(const Fooyin::Track& track, const Fooyin::Lyrics::Lyrics& lyrics);
    void lyricsSearchFinished(const Fooyin::Track& track, bool foundAny);

private:
    enum class SearchStatus : uint8_t
    {
        NotStarted = 0,
        Pending,
        Complete,
        Skipped
    };

    struct SourceSearch
    {
        LyricSource* source;
        SearchStatus status{SearchStatus::NotStarted};
        std::vector<Lyrics> results;
    };

    void loadDefaults();
    void startLyricsSearch(const Track& track);
    void startSources(bool local);
    void advanceSearch();
    void drainResults();
    void finishSearch();
    void cancelSearches();
    void onSearchResult(LyricSource* source, const std::vector<LyricData>& data);
    [[nodiscard]] std::vector<Lyrics> validatedResults(LyricSource* source, const std::vector<LyricData>& data) const;
    [[nodiscard]] bool localSearchComplete() const;
    [[nodiscard]] bool foundLocalResults() const;

    std::shared_ptr<NetworkAccessManager> m_networkManager;
    SettingsManager* m_settings;

    std::vector<LyricSource*> m_sources;

    ScriptParser m_parser;
    SearchParams m_params;
    LyricsSearchRequest m_request;
    std::vector<SourceSearch> m_searches;
    bool m_foundAnyResults;
    bool m_externalPhaseStarted;
    bool m_searchFinished;
    size_t m_nextResultIndex;
    uint64_t m_searchGeneration;
};
} // namespace Lyrics
} // namespace Fooyin
