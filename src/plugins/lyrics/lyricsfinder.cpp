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

#include "lyricsfinder.h"

#include "lyrics.h"
#include "lyricsparser.h"
#include "settings/lyricssettings.h"
#include "sources/locallyrics.h"
#include "sources/lrcliblyrics.h"
#include "sources/lyricsource.h"
#include "sources/neteaselyrics.h"
#include "sources/qqlyrics.h"
#include "sources/taglyrics.h"

#include <utils/helpers.h>
#include <utils/settings/settingsmanager.h>

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
    , m_localOnly{false}
    , m_currentSourceIndex{-1}
    , m_currentSource{nullptr}
{
    loadDefaults();
}

void LyricsFinder::findLyrics(const Track& track)
{
    m_localOnly = false;
    startLyricsSearch(track);
}

void LyricsFinder::findLocalLyrics(const Track& track)
{
    m_localOnly = true;
    startLyricsSearch(track);
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

    settings.setValue(QLatin1String{SourceState}, data);
}

void LyricsFinder::restoreState()
{
    const FySettings settings;

    QByteArray data = settings.value(QLatin1String{SourceState}, {}).toByteArray();
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
                 // new NeteaseLyrics(m_networkManager.get(), m_settings, 3, true, this),
                 new QQLyrics(m_networkManager.get(), m_settings, 4, true, this)};
}

void LyricsFinder::startLyricsSearch(const Track& track)
{
    for(const auto& source : m_sources) {
        QObject::disconnect(source, nullptr, this, nullptr);
    }

    m_params = {.track  = track,
                .title  = m_parser.evaluate(m_settings->value<Settings::Lyrics::TitleField>(), track),
                .album  = m_parser.evaluate(m_settings->value<Settings::Lyrics::AlbumField>(), track),
                .artist = m_parser.evaluate(m_settings->value<Settings::Lyrics::ArtistField>(), track)};

    m_currentSourceIndex = -1;
    m_currentSource      = nullptr;
    finishOrStartNextSource();
}

void LyricsFinder::finishOrStartNextSource(bool forceFinish)
{
    if(m_currentSource) {
        QObject::disconnect(m_currentSource, nullptr, this, nullptr);
    }

    if(!forceFinish && findNextAvailableSource()) {
        QObject::connect(m_currentSource, &LyricSource::searchResult, this, &LyricsFinder::onSearchResult,
                         Qt::SingleShotConnection);
        m_currentSource->search(m_params);
    }
}

bool LyricsFinder::findNextAvailableSource()
{
    ++m_currentSourceIndex;

    while(std::cmp_less(m_currentSourceIndex, m_sources.size())) {
        auto* source = m_sources.at(m_currentSourceIndex);
        if(source->enabled() && (!m_localOnly || source->isLocal())) {
            m_currentSource = source;
            return true;
        }
        ++m_currentSourceIndex;
    }

    m_currentSource = nullptr;
    return false;
}

void LyricsFinder::onSearchResult(const std::vector<LyricData>& data)
{
    if(data.empty()) {
        finishOrStartNextSource();
        return;
    }

    for(const auto& lyricData : data) {
        Lyrics lyrics = parse(lyricData.data);
        if(!lyrics.isValid()) {
            continue;
        }

        lyrics.source  = m_currentSource->name();
        lyrics.isLocal = m_currentSource->isLocal();

        if(lyrics.metadata.title.isEmpty()) {
            lyrics.metadata.title = lyricData.title;
        }
        if(lyrics.metadata.album.isEmpty()) {
            lyrics.metadata.album = lyricData.album;
        }
        if(lyrics.metadata.artist.isEmpty()) {
            lyrics.metadata.artist = lyricData.artists.join(u", ");
        }

        emit lyricsFound(lyrics);
    }

    const bool foundLocal    = m_currentSource->isLocal();
    const bool skipExternal  = m_settings->value<Settings::Lyrics::SkipExternal>();
    const bool skipRemaining = m_settings->value<Settings::Lyrics::SkipRemaining>();

    finishOrStartNextSource(skipRemaining || (skipExternal && foundLocal));
}
} // namespace Fooyin::Lyrics
