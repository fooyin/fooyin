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

#include "lyricsfinder.h"

#include "lyrics.h"
#include "lyricsparser.h"
#include "settings/lyricssettings.h"
#include "sources/darklyrics.h"
#include "sources/kugoulyrics.h"
#include "sources/locallyrics.h"
#include "sources/lrcliblyrics.h"
#include "sources/lyricsource.h"
#include "sources/neteaselyrics.h"
#include "sources/qqlyrics.h"
#include "sources/taglyrics.h"

#include <core/track.h>
#include <utils/helpers.h>
#include <utils/settings/settingsmanager.h>
#include <utils/stringutils.h>

using namespace Qt::StringLiterals;

constexpr auto SourceState = "Lyrics/SourceState";

namespace {
void sortSources(std::vector<Fooyin::Lyrics::LyricSource*>& sources)
{
    std::ranges::sort(sources, {}, &Fooyin::Lyrics::LyricSource::index);
    std::ranges::for_each(sources, [i = 0](auto& source) mutable { source->setIndex(i++); });
}
} // namespace

namespace Fooyin::Lyrics {
LyricsFinder::LyricsFinder(std::shared_ptr<NetworkAccessManager> networkManager, SettingsManager* settings,
                           QObject* parent)
    : QObject{parent}
    , m_networkManager{std::move(networkManager)}
    , m_settings{settings}
    , m_foundAnyResults{false}
    , m_externalPhaseStarted{false}
    , m_searchFinished{true}
    , m_nextResultIndex{0}
    , m_searchGeneration{0}
{
    loadDefaults();
    restoreState();
}

void LyricsFinder::findLyrics(const LyricsSearchRequest& request)
{
    m_request = request;
    startLyricsSearch(m_request.track);
}

void LyricsFinder::findLyrics(const Track& track)
{
    findLyrics(
        {.track     = track,
         .title     = m_parser.evaluate(m_settings->fileValue(Settings::TitleField, u"%title%"_s).toString(), track),
         .album     = m_parser.evaluate(m_settings->fileValue(Settings::AlbumField, u"%album%"_s).toString(), track),
         .artist    = m_parser.evaluate(m_settings->fileValue(Settings::ArtistField, u"%artist%"_s).toString(), track),
         .localOnly = false,
         .tagOnly   = false,
         .stopOnFirstResult             = m_settings->fileValue(Settings::SkipRemaining, true).toBool(),
         .skipExternalAfterLocalResults = m_settings->fileValue(Settings::SkipExternal, true).toBool()});
}

void LyricsFinder::findLocalLyrics(const Track& track)
{
    findLyrics(
        {.track     = track,
         .title     = m_parser.evaluate(m_settings->fileValue(Settings::TitleField, u"%title%"_s).toString(), track),
         .album     = m_parser.evaluate(m_settings->fileValue(Settings::AlbumField, u"%album%"_s).toString(), track),
         .artist    = m_parser.evaluate(m_settings->fileValue(Settings::ArtistField, u"%artist%"_s).toString(), track),
         .localOnly = true});
}

void LyricsFinder::findTagLyrics(const Track& track)
{
    findLyrics(
        {.track   = track,
         .title   = m_parser.evaluate(m_settings->fileValue(Settings::TitleField, u"%title%"_s).toString(), track),
         .album   = m_parser.evaluate(m_settings->fileValue(Settings::AlbumField, u"%album%"_s).toString(), track),
         .artist  = m_parser.evaluate(m_settings->fileValue(Settings::ArtistField, u"%artist%"_s).toString(), track),
         .tagOnly = true});
}

std::shared_ptr<NetworkAccessManager> LyricsFinder::networkManager() const
{
    return m_networkManager;
}

std::vector<LyricSource*> LyricsFinder::sources() const
{
    return m_sources;
}

void LyricsFinder::saveState()
{
    FySettings settings;

    QByteArray data;
    QDataStream stream{&data, QIODevice::WriteOnly};

    stream << static_cast<qsizetype>(m_sources.size());
    for(const auto& source : m_sources) {
        stream << source->name() << source->index() << source->enabled();
    }

    settings.setValue(SourceState, data);
}

void LyricsFinder::restoreState()
{
    const FySettings settings;

    QByteArray data = settings.value(SourceState, {}).toByteArray();
    QDataStream stream{&data, QIODevice::ReadOnly};

    qsizetype size{0};
    stream >> size;

    while(size > 0) {
        --size;
        QString name;
        int index{0};
        bool enabled{true};

        stream >> name >> index >> enabled;

        auto sourceIt = std::ranges::find_if(m_sources, [&name](const auto& source) { return source->name() == name; });
        if(sourceIt != m_sources.cend()) {
            (*sourceIt)->setIndex(index);
            (*sourceIt)->setEnabled(enabled);
        }
    }

    sortSources(m_sources);
}

void LyricsFinder::sort()
{
    sortSources(m_sources);
}

void LyricsFinder::reset()
{
    FySettings settings;
    settings.remove(QLatin1String{SourceState});

    loadDefaults();
}

void LyricsFinder::loadDefaults()
{
    m_sources = {new LocalLyrics(m_networkManager.get(), m_settings, 0, true, this),
                 new TagLyrics(m_networkManager.get(), m_settings, 1, true, this),
                 new LrcLibLyrics(m_networkManager.get(), m_settings, 2, true, this),
                 new NeteaseLyrics(m_networkManager.get(), m_settings, 3, true, this),
                 new QQLyrics(m_networkManager.get(), m_settings, 4, true, this),
                 new KugouLyrics(m_networkManager.get(), m_settings, 5, true, this),
                 new DarkLyrics(m_networkManager.get(), m_settings, 6, false, this)};
}

void LyricsFinder::startLyricsSearch(const Track& track)
{
    ++m_searchGeneration;

    for(const auto& source : m_sources) {
        QObject::disconnect(source, nullptr, this, nullptr);
        source->cancel();
    }

    m_params = {.track = track, .title = m_request.title, .album = m_request.album, .artist = m_request.artist};
    m_searches.clear();

    for(auto* source : m_sources) {
        const bool matchesLocalFilter = !m_request.localOnly || source->isLocal();
        const bool matchesTagFilter   = !m_request.tagOnly || source->name() == "Metadata Tags"_L1;
        if(source->enabled() && matchesLocalFilter && matchesTagFilter) {
            m_searches.emplace_back(source, SearchStatus::NotStarted, std::vector<Lyrics>{});
        }
    }

    m_foundAnyResults      = false;
    m_externalPhaseStarted = false;
    m_searchFinished       = false;
    m_nextResultIndex      = 0;

    startSources(true);
    advanceSearch();
}

void LyricsFinder::startSources(bool local)
{
    std::vector<LyricSource*> sources;
    for(auto& search : m_searches) {
        if(search.status == SearchStatus::NotStarted && search.source->isLocal() == local) {
            search.status = SearchStatus::Pending;
            sources.push_back(search.source);
        }
    }

    const uint64_t generation = m_searchGeneration;

    for(auto* source : sources) {
        QObject::connect(
            source, &LyricSource::searchResult, this,
            [this, source, generation](const std::vector<LyricData>& data) {
                if(generation == m_searchGeneration) {
                    onSearchResult(source, data);
                }
            },
            Qt::SingleShotConnection);

        source->search(m_params);

        if(generation != m_searchGeneration) {
            return;
        }
    }
}

void LyricsFinder::advanceSearch()
{
    if(m_searchFinished || !localSearchComplete()) {
        return;
    }

    if(!m_externalPhaseStarted) {
        m_externalPhaseStarted = true;

        if(m_request.skipExternalAfterLocalResults && foundLocalResults()) {
            for(auto& search : m_searches) {
                if(!search.source->isLocal() && search.status == SearchStatus::NotStarted) {
                    search.status = SearchStatus::Skipped;
                }
            }
        }
        else {
            drainResults();
            if(m_searchFinished) {
                return;
            }
            startSources(false);
        }
    }

    drainResults();
}

void LyricsFinder::drainResults()
{
    if(m_searchFinished) {
        return;
    }

    const uint64_t generation = m_searchGeneration;

    while(m_nextResultIndex < m_searches.size()) {
        auto& search = m_searches.at(m_nextResultIndex);
        if(search.status == SearchStatus::NotStarted || search.status == SearchStatus::Pending) {
            return;
        }

        ++m_nextResultIndex;

        if(search.status == SearchStatus::Skipped) {
            continue;
        }

        const auto results = search.results;
        for(const auto& lyrics : results) {
            m_foundAnyResults = true;
            Q_EMIT lyricsFound(m_params.track, lyrics);
            if(generation != m_searchGeneration) {
                return;
            }
        }

        if(m_request.stopOnFirstResult && !results.empty()) {
            cancelSearches();
            finishSearch();
            return;
        }
    }

    finishSearch();
}

void LyricsFinder::finishSearch()
{
    if(m_searchFinished) {
        return;
    }

    m_searchFinished = true;
    Q_EMIT lyricsSearchFinished(m_params.track, m_foundAnyResults);
}

void LyricsFinder::cancelSearches()
{
    for(auto& search : m_searches) {
        if(search.status == SearchStatus::Pending) {
            QObject::disconnect(search.source, nullptr, this, nullptr);
            search.source->cancel();
            search.status = SearchStatus::Skipped;
        }
        else if(search.status == SearchStatus::NotStarted) {
            search.status = SearchStatus::Skipped;
        }
    }
}

void LyricsFinder::onSearchResult(LyricSource* source, const std::vector<LyricData>& data)
{
    const auto searchIt
        = std::ranges::find(m_searches, source, [](const SourceSearch& search) { return search.source; });
    if(searchIt == m_searches.end() || searchIt->status != SearchStatus::Pending) {
        return;
    }

    searchIt->results = validatedResults(source, data);
    searchIt->status  = SearchStatus::Complete;
    advanceSearch();
}

std::vector<Lyrics> LyricsFinder::validatedResults(LyricSource* source, const std::vector<LyricData>& data) const
{
    if(data.empty()) {
        return {};
    }

    const int matchThreshold = m_settings->fileValue(Settings::MatchThreshold, 75).toInt();

    const auto isSimilar = [matchThreshold](const QString& param, const QString& lyricParam) {
        if(param.isEmpty() || lyricParam.isEmpty()) {
            return true;
        }
        return Utils::similarityRatio(param, lyricParam, Qt::CaseInsensitive) >= matchThreshold;
    };

    std::vector<Lyrics> results;

    for(const auto& lyricData : data) {
        if(!isSimilar(m_params.title, lyricData.title)) {
            continue;
        }
        if(!isSimilar(m_params.artist, lyricData.artist)) {
            continue;
        }
        if(!isSimilar(m_params.album, lyricData.album)) {
            continue;
        }

        Lyrics lyrics = parse(lyricData.data);
        if(!lyrics.isValid()) {
            continue;
        }

        lyrics.filepath = lyricData.path;
        lyrics.tag      = lyricData.tag;
        lyrics.data     = lyricData.data;
        lyrics.source   = source->name();
        lyrics.isLocal  = source->isLocal();

        if(lyrics.metadata.title.isEmpty()) {
            lyrics.metadata.title = lyricData.title;
        }
        if(lyrics.metadata.album.isEmpty()) {
            lyrics.metadata.album = lyricData.album;
        }
        if(lyrics.metadata.artist.isEmpty()) {
            lyrics.metadata.artist = lyricData.artist;
        }

        const bool isDuplicate = std::ranges::any_of(results, [&lyrics](const Lyrics& existingLyrics) {
            return existingLyrics.source == lyrics.source && existingLyrics.metadata.title == lyrics.metadata.title
                && existingLyrics.metadata.album == lyrics.metadata.album
                && existingLyrics.metadata.artist == lyrics.metadata.artist && existingLyrics == lyrics;
        });
        if(isDuplicate) {
            continue;
        }

        results.emplace_back(std::move(lyrics));
    }

    return results;
}

bool LyricsFinder::localSearchComplete() const
{
    return std::ranges::all_of(m_searches, [](const SourceSearch& search) {
        return !search.source->isLocal() || search.status == SearchStatus::Complete
            || search.status == SearchStatus::Skipped;
    });
}

bool LyricsFinder::foundLocalResults() const
{
    return std::ranges::any_of(
        m_searches, [](const SourceSearch& search) { return search.source->isLocal() && !search.results.empty(); });
}
} // namespace Fooyin::Lyrics
