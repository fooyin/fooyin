/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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

#include "scriptvariableproviders.h"

#include <core/constants.h>
#include <core/player/playercontroller.h>
#include <utils/stringutils.h>

using namespace Qt::StringLiterals;

namespace {
QString frontCover()
{
    return QString::fromLatin1(Fooyin::Constants::FrontCover);
}

QString backCover()
{
    return QString::fromLatin1(Fooyin::Constants::BackCover);
}

QString artistPicture()
{
    return QString::fromLatin1(Fooyin::Constants::ArtistPicture);
}

const Fooyin::ScriptPlaylistEnvironment* playlistEnvironment(const Fooyin::ScriptContext& context)
{
    return context.environment ? context.environment->playlistEnvironment() : nullptr;
}

QString trackDepth(const Fooyin::ScriptContext& context)
{
    const auto* environment = playlistEnvironment(context);
    if(environment == nullptr) {
        return {};
    }

    return QString::number(environment->trackDepth());
}

QString trackIndex(const Fooyin::ScriptContext& context)
{
    const auto* environment = playlistEnvironment(context);
    if(environment == nullptr) {
        return {};
    }

    const int playlistTrackIndex = environment->currentPlaylistTrackIndex();
    if(playlistTrackIndex < 0) {
        return {};
    }

    const int playlistTrackCount = environment->playlistTrackCount();
    const int numDigits          = playlistTrackCount > 0 ? static_cast<int>(std::log10(playlistTrackCount)) + 1 : 2;

    return Fooyin::Utils::addLeadingZero(playlistTrackIndex + 1, numDigits);
}

QString queueIndex(const Fooyin::ScriptContext& context)
{
    const auto* environment = playlistEnvironment(context);
    if(environment == nullptr) {
        return {};
    }

    const auto indexes = environment->currentQueueIndexes();
    if(indexes.empty()) {
        return {};
    }

    return QString::number(indexes.front());
}

QString queueIndexes(const Fooyin::ScriptContext& context)
{
    const auto* environment = playlistEnvironment(context);
    if(environment == nullptr) {
        return {};
    }

    const auto indexes = environment->currentQueueIndexes();
    if(indexes.empty()) {
        return {};
    }

    QStringList textIndexes;
    textIndexes.reserve(static_cast<qsizetype>(indexes.size()));

    for(const int index : indexes) {
        textIndexes.append(QString::number(index + 1));
    }

    return textIndexes.join(u", "_s);
}

QString playingQueue(const Fooyin::ScriptContext& context)
{
    const QString indexes = queueIndexes(context);
    return indexes.isEmpty() ? QString{} : u"[%1]"_s.arg(indexes);
}
} // namespace

namespace Fooyin {
PlaylistScriptEnvironment::PlaylistScriptEnvironment()
    : m_playlist{nullptr}
    , m_playlistQueue{nullptr}
    , m_tracks{nullptr}
    , m_playlistTrackIndex{-1}
    , m_trackDepth{0}
    , m_currentPosition{0}
    , m_currentTrackDuration{0}
    , m_bitrate{0}
    , m_playState{Player::PlayState::Stopped}
    , m_trackListContextPolicy{TrackListContextPolicy::Unresolved}
    , m_escapeRichText{false}
    , m_useVariousArtists{false}
{ }

void PlaylistScriptEnvironment::setPlaylistData(const Playlist* playlist, const PlaylistTrackIndexes* playlistQueue,
                                                const TrackList* tracks)
{
    m_playlist      = playlist;
    m_playlistQueue = playlistQueue;
    m_tracks        = tracks;
}

void PlaylistScriptEnvironment::setTrackState(int playlistTrackIndex, int trackDepth)
{
    m_playlistTrackIndex = playlistTrackIndex;
    m_trackDepth         = trackDepth;
}

void PlaylistScriptEnvironment::setPlaybackState(uint64_t currentPosition, uint64_t currentTrackDuration, int bitrate,
                                                 Player::PlayState playState)
{
    m_currentPosition      = currentPosition;
    m_currentTrackDuration = currentTrackDuration;
    m_bitrate              = bitrate;
    m_playState            = playState;
}

void PlaylistScriptEnvironment::setEvaluationPolicy(TrackListContextPolicy policy, QString placeholder,
                                                    bool escapeRichText, bool useVariousArtists)
{
    m_trackListContextPolicy = policy;
    m_trackListPlaceholder   = std::move(placeholder);
    m_escapeRichText         = escapeRichText;
    m_useVariousArtists      = useVariousArtists;
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

int PlaylistScriptEnvironment::playlistTrackCount() const
{
    return m_playlist ? m_playlist->trackCount() : 0;
}

int PlaylistScriptEnvironment::trackDepth() const
{
    return m_trackDepth;
}

std::span<const int> PlaylistScriptEnvironment::currentQueueIndexes() const
{
    static const std::vector<int> Empty;

    if(!m_playlistQueue) {
        return Empty;
    }

    if(const auto it = m_playlistQueue->find(m_playlistTrackIndex); it != m_playlistQueue->cend()) {
        return it->second;
    }

    return Empty;
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

PlaybackScriptContextData::PlaybackScriptContextData()
{
    context.environment = &environment;
}

PlaybackScriptContextData::PlaybackScriptContextData(PlaybackScriptContextData&& other) noexcept
    : playlistQueue{std::move(other.playlistQueue)}
    , tracks{std::move(other.tracks)}
    , environment{std::move(other.environment)}
    , context{other.context}
{
    environment.setPlaylistData(context.playlist, &playlistQueue, tracks.empty() ? nullptr : &tracks);
    context.environment = &environment;
}

PlaybackScriptContextData& PlaybackScriptContextData::operator=(PlaybackScriptContextData&& other) noexcept
{
    if(this != &other) {
        playlistQueue = std::move(other.playlistQueue);
        tracks        = std::move(other.tracks);
        environment   = std::move(other.environment);
        context       = other.context;
        environment.setPlaylistData(context.playlist, &playlistQueue, tracks.empty() ? nullptr : &tracks);
        context.environment = &environment;
    }
    return *this;
}

const ScriptVariableProvider& artworkMarkerVariableProvider()
{
    static const StaticScriptVariableProvider Provider{
        makeScriptVariableDescriptor<frontCover>(VariableKind::FrontCover, u"FRONTCOVER"_s),
        makeScriptVariableDescriptor<backCover>(VariableKind::BackCover, u"BACKCOVER"_s),
        makeScriptVariableDescriptor<artistPicture>(VariableKind::ArtistPicture, u"ARTISTPICTURE"_s)};
    return Provider;
}

const ScriptVariableProvider& playlistVariableProvider()
{
    static const StaticScriptVariableProvider Provider{
        makeScriptVariableDescriptor<trackDepth>(VariableKind::Depth, u"DEPTH"_s),
        makeScriptVariableDescriptor<trackIndex>(VariableKind::ListIndex, u"LIST_INDEX"_s),
        makeScriptVariableDescriptor<queueIndex>(VariableKind::QueueIndex, u"QUEUEINDEX"_s),
        makeScriptVariableDescriptor<queueIndexes>(VariableKind::QueueIndexes, u"QUEUEINDEXES"_s),
        makeScriptVariableDescriptor<playingQueue>(VariableKind::PlayingIcon, u"PLAYINGICON"_s)};
    return Provider;
}

PlaybackScriptContextData makePlaybackScriptContext(PlayerController* playerController, Playlist* playlist,
                                                    TrackListContextPolicy policy, QString placeholder,
                                                    bool escapeRichText, bool useVariousArtists)
{
    PlaybackScriptContextData data;
    data.context.playlist = playlist;

    if(playlist && playerController) {
        data.playlistQueue = playerController->playbackQueue().indexesForPlaylist(playlist->id());
        data.tracks        = playlist->tracks();
    }

    int playlistTrackIndex{-1};
    uint64_t currentPosition{0};
    uint64_t currentTrackDuration{0};
    int bitrate{0};
    auto playState{Player::PlayState::Stopped};

    if(playerController) {
        playlistTrackIndex   = playerController->currentPlaylistTrack().indexInPlaylist;
        currentPosition      = playerController->currentPosition();
        currentTrackDuration = playerController->currentTrack().duration();
        bitrate              = playerController->bitrate();
        playState            = playerController->playState();
    }

    data.environment.setPlaylistData(playlist, &data.playlistQueue, data.tracks.empty() ? nullptr : &data.tracks);
    data.environment.setTrackState(playlistTrackIndex, 0);
    data.environment.setPlaybackState(currentPosition, currentTrackDuration, bitrate, playState);
    data.environment.setEvaluationPolicy(policy, std::move(placeholder), escapeRichText, useVariousArtists);

    return data;
}
} // namespace Fooyin
