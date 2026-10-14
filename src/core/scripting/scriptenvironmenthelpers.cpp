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

#include <core/scripting/scriptenvironmenthelpers.h>

#include "library/librarymanager.h"

#include <core/player/playbackqueue.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlist.h>

#include <QDir>

namespace Fooyin {
LibraryScriptEnvironment::LibraryScriptEnvironment(const LibraryManager* libraryManager)
    : m_libraryManager{libraryManager}
    , m_trackListContextPolicy{TrackListContextPolicy::Unresolved}
    , m_escapeRichText{false}
    , m_useVariousArtists{false}
    , m_ratingSymbols{defaultRatingStarSymbols()}
{ }

void LibraryScriptEnvironment::setEvaluationPolicy(TrackListContextPolicy policy, QString placeholder,
                                                   bool escapeRichText, bool useVariousArtists)
{
    m_trackListContextPolicy = policy;
    m_trackListPlaceholder   = std::move(placeholder);
    m_escapeRichText         = escapeRichText;
    m_useVariousArtists      = useVariousArtists;
}

void LibraryScriptEnvironment::setRatingStarSymbols(const RatingStarSymbols& ratingSymbols)
{
    m_ratingSymbols = ratingSymbols;
}

const ScriptLibraryEnvironment* LibraryScriptEnvironment::libraryEnvironment() const
{
    return this;
}

const ScriptEvaluationEnvironment* LibraryScriptEnvironment::evaluationEnvironment() const
{
    return this;
}

QString LibraryScriptEnvironment::libraryName(const Track& track) const
{
    if(!m_libraryManager) {
        return {};
    }

    if(const auto library = m_libraryManager->libraryInfo(track.libraryId())) {
        return library->name;
    }

    return {};
}

QString LibraryScriptEnvironment::libraryPath(const Track& track) const
{
    if(!m_libraryManager) {
        return {};
    }

    if(const auto library = m_libraryManager->libraryInfo(track.libraryId())) {
        return library->path;
    }

    return {};
}

QString LibraryScriptEnvironment::relativePath(const Track& track) const
{
    const QString path = libraryPath(track);
    return path.isEmpty() ? QString{} : QDir{path}.relativeFilePath(track.prettyFilepath());
}

TrackListContextPolicy LibraryScriptEnvironment::trackListContextPolicy() const
{
    return m_trackListContextPolicy;
}

QString LibraryScriptEnvironment::trackListPlaceholder() const
{
    return m_trackListPlaceholder;
}

bool LibraryScriptEnvironment::escapeRichText() const
{
    return m_escapeRichText;
}

bool LibraryScriptEnvironment::useVariousArtists() const
{
    return m_useVariousArtists;
}

RatingStarSymbols LibraryScriptEnvironment::ratingStarSymbols() const
{
    RatingStarSymbols symbols{m_ratingSymbols};

    if(symbols.fullStarSymbol.isEmpty()) {
        symbols.fullStarSymbol = defaultRatingFullStarSymbol();
    }
    if(symbols.halfStarSymbol.isEmpty()) {
        symbols.halfStarSymbol = defaultRatingHalfStarSymbol();
    }

    return symbols;
}

PlaylistScriptEnvironment::PlaylistScriptEnvironment()
    : m_playlistTrackCount{0}
    , m_tracks{nullptr}
    , m_playlistTrackIndex{-1}
    , m_currentPlayingTrackIndex{-1}
    , m_currentPlayingTrackId{-1}
    , m_trackDepth{0}
    , m_queueTotal{0}
    , m_queueItemState{ScriptQueueItemState::None}
    , m_currentPosition{0}
    , m_currentTrackDuration{0}
    , m_bitrate{0}
    , m_playState{Player::PlayState::Stopped}
    , m_trackListContextPolicy{TrackListContextPolicy::Unresolved}
    , m_escapeRichText{false}
    , m_useVariousArtists{false}
    , m_ratingSymbols{defaultRatingStarSymbols()}
{ }

void PlaylistScriptEnvironment::setPlaylistData(QString name, int trackCount, const TrackList* tracks)
{
    m_playlistName       = std::move(name);
    m_playlistTrackCount = trackCount;
    m_tracks             = tracks;
    m_queueIndexes.clear();
    m_queueTotal     = 0;
    m_queueItemState = ScriptQueueItemState::None;
}

void PlaylistScriptEnvironment::setQueueState(std::span<const int> queueIndexes, const int queueTotal)
{
    m_queueIndexes.assign(queueIndexes.begin(), queueIndexes.end());
    m_queueTotal = queueTotal;
}

void PlaylistScriptEnvironment::setQueueItemState(ScriptQueueItemState state)
{
    m_queueItemState = state;
}

void PlaylistScriptEnvironment::setTrackState(int playlistTrackIndex, int currentPlayingTrackIndex,
                                              int currentPlayingTrackId, int trackDepth)
{
    m_playlistTrackIndex       = playlistTrackIndex;
    m_currentPlayingTrackIndex = currentPlayingTrackIndex;
    m_currentPlayingTrackId    = currentPlayingTrackId;
    m_trackDepth               = trackDepth;
}

void PlaylistScriptEnvironment::setPlaybackState(uint64_t currentPosition, uint64_t currentTrackDuration, int bitrate,
                                                 Player::PlayState playState, QString decoder,
                                                 Engine::PlaybackOutputInfo outputInfo)
{
    m_currentPosition      = currentPosition;
    m_currentTrackDuration = currentTrackDuration;
    m_bitrate              = bitrate;
    m_playState            = playState;
    m_decoder              = std::move(decoder);
    m_outputInfo           = std::move(outputInfo);
}

void PlaylistScriptEnvironment::setEvaluationPolicy(TrackListContextPolicy policy, QString placeholder,
                                                    bool escapeRichText, bool useVariousArtists)
{
    m_trackListContextPolicy = policy;
    m_trackListPlaceholder   = std::move(placeholder);
    m_escapeRichText         = escapeRichText;
    m_useVariousArtists      = useVariousArtists;
}

void PlaylistScriptEnvironment::setRatingStarSymbols(const RatingStarSymbols& ratingSymbols)
{
    m_ratingSymbols = ratingSymbols;
}

const ScriptPlaylistEnvironment* PlaylistScriptEnvironment::playlistEnvironment() const
{
    return this;
}

const ScriptTrackListEnvironment* PlaylistScriptEnvironment::trackListEnvironment() const
{
    return this;
}

const ScriptPlaybackEnvironment* PlaylistScriptEnvironment::playbackEnvironment() const
{
    return this;
}

const ScriptEvaluationEnvironment* PlaylistScriptEnvironment::evaluationEnvironment() const
{
    return this;
}

int PlaylistScriptEnvironment::currentPlaylistTrackIndex() const
{
    return m_playlistTrackIndex;
}

int PlaylistScriptEnvironment::currentPlayingTrackIndex() const
{
    return m_currentPlayingTrackIndex;
}

int PlaylistScriptEnvironment::currentPlayingTrackId() const
{
    return m_currentPlayingTrackId;
}

QString PlaylistScriptEnvironment::playlistName() const
{
    return m_playlistName;
}

int PlaylistScriptEnvironment::playlistTrackCount() const
{
    return m_playlistTrackCount;
}

int PlaylistScriptEnvironment::trackDepth() const
{
    return m_trackDepth;
}

std::span<const int> PlaylistScriptEnvironment::currentQueueIndexes() const
{
    return m_queueIndexes;
}

int PlaylistScriptEnvironment::currentQueueTotal() const
{
    return m_queueTotal;
}

ScriptQueueItemState PlaylistScriptEnvironment::queueItemState() const
{
    return m_queueItemState;
}

const TrackList* PlaylistScriptEnvironment::trackList() const
{
    return m_tracks;
}

uint64_t PlaylistScriptEnvironment::currentPosition() const
{
    return m_currentPosition;
}

uint64_t PlaylistScriptEnvironment::currentTrackDuration() const
{
    return m_currentTrackDuration;
}

int PlaylistScriptEnvironment::bitrate() const
{
    return m_bitrate;
}

QString PlaylistScriptEnvironment::decoder() const
{
    return m_decoder;
}

Engine::PlaybackOutputInfo PlaylistScriptEnvironment::outputInfo() const
{
    return m_outputInfo;
}

Player::PlayState PlaylistScriptEnvironment::playState() const
{
    return m_playState;
}

TrackListContextPolicy PlaylistScriptEnvironment::trackListContextPolicy() const
{
    return m_trackListContextPolicy;
}

QString PlaylistScriptEnvironment::trackListPlaceholder() const
{
    return m_trackListPlaceholder;
}

bool PlaylistScriptEnvironment::escapeRichText() const
{
    return m_escapeRichText;
}

bool PlaylistScriptEnvironment::useVariousArtists() const
{
    return m_useVariousArtists;
}

RatingStarSymbols PlaylistScriptEnvironment::ratingStarSymbols() const
{
    RatingStarSymbols symbols{m_ratingSymbols};

    if(symbols.fullStarSymbol.isEmpty()) {
        symbols.fullStarSymbol = defaultRatingFullStarSymbol();
    }
    if(symbols.halfStarSymbol.isEmpty()) {
        symbols.halfStarSymbol = defaultRatingHalfStarSymbol();
    }

    return symbols;
}

PlaybackScriptContext::PlaybackScriptContext()
{
    context.environment = &environment;
}

PlaybackScriptContext::PlaybackScriptContext(PlaybackScriptContext&& other) noexcept
    : environment{std::move(other.environment)}
    , context{other.context}
{
    context.environment = &environment;
}

PlaybackScriptContext& PlaybackScriptContext::operator=(PlaybackScriptContext&& other) noexcept
{
    if(this != &other) {
        environment         = std::move(other.environment);
        context             = other.context;
        context.environment = &environment;
    }
    return *this;
}

PlaybackScriptContext makePlaybackScriptContext(PlayerController* playerController, Playlist* playlist,
                                                TrackListContextPolicy policy, QString placeholder, bool escapeRichText,
                                                bool useVariousArtists, const RatingStarSymbols& ratingSymbols)
{
    PlaybackScriptContext data;
    data.context.playlist = playlist;

    int playlistTrackIndex{-1};
    uint64_t currentPosition{0};
    uint64_t currentTrackDuration{0};
    int bitrate{0};
    QString decoder;
    Engine::PlaybackOutputInfo outputInfo;
    auto playState{Player::PlayState::Stopped};

    if(playerController) {
        playlistTrackIndex   = playerController->currentPlaylistTrack().indexInPlaylist;
        currentPosition      = playerController->currentPosition();
        currentTrackDuration = playerController->currentTrack().duration();
        bitrate              = playerController->bitrate();
        decoder              = playerController->decoder();
        outputInfo           = playerController->playbackOutputInfo();
        playState            = playerController->playState();
    }

    data.environment.setPlaylistData(playlist ? playlist->name() : QString{}, playlist ? playlist->trackCount() : 0,
                                     playlist ? &playlist->tracks() : nullptr);
    const auto queueIndexes = playerController && playlist && playlistTrackIndex >= 0
                                ? playerController->playbackQueue().indexesForTrack(playlist->id(), playlistTrackIndex)
                                : std::vector<int>{};
    data.environment.setQueueState(queueIndexes, playerController ? playerController->queuedTracksCount() : 0);
    data.environment.setTrackState(playlistTrackIndex, playlistTrackIndex,
                                   playerController ? playerController->currentTrackId() : -1, 0);
    data.environment.setPlaybackState(currentPosition, currentTrackDuration, bitrate, playState, std::move(decoder),
                                      std::move(outputInfo));
    data.environment.setRatingStarSymbols(ratingSymbols);
    data.environment.setEvaluationPolicy(policy, std::move(placeholder), escapeRichText, useVariousArtists);

    return data;
}
} // namespace Fooyin
