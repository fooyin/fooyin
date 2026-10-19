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

#include "lyricsplaybackservice.h"

#include "lyricsfinder.h"
#include "lyricssaver.h"
#include "settings/lyricssettings.h"

#include <core/player/playercontroller.h>
#include <core/plugins/coreplugincontext.h>
#include <core/scripting/scriptregistry.h>
#include <utils/settings/settingsmanager.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Lyrics {
LyricsPlaybackService::LyricsPlaybackService(const CorePluginContext& core, LyricsFinder* finder, LyricsSaver* saver,
                                             QObject* parent)
    : QObject{parent}
    , m_player{core.playerController}
    , m_finder{finder}
    , m_saver{saver}
    , m_settings{core.settingsManager}
    , m_context{core.scriptContext}
    , m_source{ScriptRegistry::instance().registerDataSource(u"lyrics.currentLine"_s)}
    , m_track{m_player->currentTrack()}
    , m_lineDirty{true}
    , m_searched{false}
    , m_searching{false}
{
    ScriptRegistry::instance().registerVariable(
        {.name         = u"LYRICS_LINE"_s,
         .category     = tr("Lyrics"),
         .description  = tr("Current synchronised lyrics line for the playing track"),
         .dependencies = {.sources = {m_source}, .unclassified = false},
         .contextual   = true});

    m_context->registerProvider(m_source, this, [this](ScriptContextData& data) { capture(data); });

    QObject::connect(m_context, &ScriptContextService::sourceDemandChanged, this, [this](ScriptDataSourceId source) {
        if(source == m_source) {
            updateDemand();
        }
    });
    QObject::connect(m_player, &PlayerController::currentTrackChanged, this,
                     [this](const Track& track) { updateTrack(track, false); });
    QObject::connect(m_player, &PlayerController::currentTrackUpdated, this,
                     [this](const Track& track) { updateTrack(track, true); });
    QObject::connect(m_player, &PlayerController::positionChanged, this, &LyricsPlaybackService::updateCurrentLine);
    QObject::connect(m_player, &PlayerController::positionMoved, this, &LyricsPlaybackService::updateCurrentLine);
    QObject::connect(m_player, &PlayerController::playStateChanged, this, &LyricsPlaybackService::updateCurrentLine);
    QObject::connect(finder, &LyricsFinder::lyricsSearchStarted, this, [this](const Track& track) {
        if(track.sameIdentityAs(m_track)) {
            m_candidates.clear();
            Q_EMIT changed();
        }
    });
    QObject::connect(finder, &LyricsFinder::lyricsFound, this, [this](const Track& track, const Lyrics& lyrics) {
        if(!track.sameIdentityAs(m_track)) {
            return;
        }
        const bool first = m_candidates.empty();
        m_candidates.push_back(lyrics);
        if(first) {
            applyLyrics(lyrics);
        }
    });
    QObject::connect(finder, &LyricsFinder::lyricsSearchFinished, this, [this](const Track& track, bool found) {
        if(!track.sameIdentityAs(m_track)) {
            return;
        }
        m_searching = false;
        if(!found) {
            applyLyrics({});
        }
    });
    QObject::connect(saver, &LyricsSaver::lyricsSaved, this, [this](const Track& track, const Lyrics& lyrics) {
        if(!track.sameIdentityAs(m_track)) {
            return;
        }
        m_searched = true;
        m_track    = track;
        std::erase_if(m_candidates, [](const Lyrics& item) { return item.isLocal; });
        if(lyrics.isValid()) {
            m_candidates.insert(m_candidates.begin(), lyrics);
        }
        applyLyrics(lyrics);
    });
}

const Track& LyricsPlaybackService::track() const
{
    return m_track;
}

const Lyrics& LyricsPlaybackService::lyrics() const
{
    return m_lyrics;
}

const std::vector<Lyrics>& LyricsPlaybackService::candidates() const
{
    return m_candidates;
}

LyricsFinder* LyricsPlaybackService::finder() const
{
    return m_finder;
}

ScriptDataSourceId LyricsPlaybackService::dataSource() const
{
    return m_source;
}

void LyricsPlaybackService::updateTrack(const Track& track, bool metadataOnly)
{
    const auto tags     = m_settings->fileValue(Settings::SearchTags, Defaults::searchTags()).toStringList();
    const bool preserve = track.sameIdentityAs(m_track) && m_lyrics.isValid()
                       && std::ranges::all_of(tags, [this, &track](const QString& tag) {
                              return m_track.extraTag(tag) == track.extraTag(tag);
                          });

    const Track previousTrack = std::exchange(m_track, track);

    if(!preserve) {
        m_finder->cancel();
        m_searching = false;
        m_searched  = false;
        m_lyrics    = {};
        m_lineDirty = true;
        m_candidates.clear();
    }

    updateCurrentLine();
    if(previousTrack.isValid() && !previousTrack.sameIdentityAs(track)) {
        m_context->publish(m_source, previousTrack);
    }

    if(!metadataOnly) {
        m_context->publish(m_source, m_track);
    }

    Q_EMIT changed();

    if(!preserve && m_context->isObserved(m_source)) {
        findLyrics(!metadataOnly);
    }
}

void LyricsPlaybackService::updateDemand()
{
    if(m_context->isObserved(m_source)) {
        if(!m_searched) {
            findLyrics(true);
        }
    }
    else if(m_searching) {
        m_finder->cancel();
        m_searching = false;
        m_searched  = false;
    }
}

void LyricsPlaybackService::findLyrics(bool automatic)
{
    if(!m_track.isValid()) {
        return;
    }

    m_searched  = true;
    m_searching = true;

    const QString skip = m_settings->fileValue(Settings::AutoSearchSkip, Defaults::autoSearchSkipScript()).toString();
    if(automatic && m_settings->fileValue(Settings::AutoSearch, false).toBool()
       && m_parser.evaluate(skip, m_track).isEmpty()) {
        m_finder->findLyrics(m_track);
    }
    else {
        m_finder->findLocalLyrics(m_track);
    }
}

void LyricsPlaybackService::search()
{
    if(m_track.isValid()) {
        m_searched  = true;
        m_searching = true;
        m_finder->findLyrics(m_track);
    }
}

void LyricsPlaybackService::selectLyrics(const Track& track, const Lyrics& lyrics)
{
    if(!track.sameIdentityAs(m_track)) {
        return;
    }

    m_finder->cancel();
    m_searching = false;
    m_searched  = true;

    if(lyrics.isValid() && std::ranges::find(m_candidates, lyrics) == m_candidates.end()) {
        if(lyrics.isLocal) {
            std::erase_if(m_candidates, [](const Lyrics& item) { return item.isLocal; });
        }
        m_candidates.insert(m_candidates.begin(), lyrics);
    }

    applyLyrics(lyrics);
}

void LyricsPlaybackService::applyLyrics(const Lyrics& lyrics)
{
    m_lyrics    = lyrics;
    m_lineDirty = true;
    updateCurrentLine();
    Q_EMIT changed();
    if(lyrics.isValid() && !lyrics.isLocal) {
        m_saver->autoSaveLyrics(lyrics, m_track);
    }
}

void LyricsPlaybackService::updateCurrentLine()
{
    const bool active = m_player->playState() != Player::PlayState::Stopped;
    const auto range  = active ? m_lyrics.lineRange(m_player->currentPosition()) : std::pair<size_t, size_t>{};
    if(!m_lineDirty && range == m_currentRange) {
        return;
    }

    m_lineDirty    = false;
    m_currentRange = range;

    QString text;
    if(active) {
        text = m_lyrics.lineText(m_player->currentPosition());
    }

    ScriptResult line{.value = text, .cond = !text.isEmpty()};
    if(line.value != m_currentLine.value || line.cond != m_currentLine.cond) {
        m_currentLine = std::move(line);
        m_context->publish(m_source, m_track);
    }
}

void LyricsPlaybackService::capture(ScriptContextData& data) const
{
    QString text;
    if(data.playback.playState != Player::PlayState::Stopped && data.playback.track.sameIdentityAs(m_track)) {
        text = m_lyrics.lineText(data.playback.position);
    }
    data.values.push_back(
        {.name = u"LYRICS_LINE"_s, .result = {.value = text, .cond = !text.isEmpty()}, .track = data.playback.track});
}
} // namespace Fooyin::Lyrics
