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

#include "fycore_export.h"

#include <core/ratingsymbols.h>
#include <core/scripting/scripttypes.h>

#include <vector>

namespace Fooyin {
class LibraryManager;
class PlaybackQueue;
class PlayerController;

/*!
 * Reusable environment adapter for library-backed scripting surfaces.
 *
 * This helper exposes library lookups plus a small evaluation-policy surface. It is
 * intended for callers that need basic library variables without implementing their
 * own `ScriptEnvironment`.
 */
class FYCORE_EXPORT LibraryScriptEnvironment : public ScriptEnvironment,
                                               public ScriptLibraryEnvironment,
                                               public ScriptEvaluationEnvironment
{
public:
    explicit LibraryScriptEnvironment(const LibraryManager* libraryManager);

    void setEvaluationPolicy(TrackListContextPolicy policy, QString placeholder = {}, bool escapeRichText = false,
                             bool useVariousArtists = false);
    void setRatingStarSymbols(const RatingStarSymbols& ratingSymbols);

    [[nodiscard]] const ScriptLibraryEnvironment* libraryEnvironment() const override;
    [[nodiscard]] const ScriptEvaluationEnvironment* evaluationEnvironment() const override;

    [[nodiscard]] QString libraryName(const Track& track) const override;
    [[nodiscard]] QString libraryPath(const Track& track) const override;
    [[nodiscard]] QString relativePath(const Track& track) const override;
    [[nodiscard]] TrackListContextPolicy trackListContextPolicy() const override;
    [[nodiscard]] QString trackListPlaceholder() const override;
    [[nodiscard]] bool escapeRichText() const override;
    [[nodiscard]] bool useVariousArtists() const override;
    [[nodiscard]] RatingStarSymbols ratingStarSymbols() const override;

private:
    const LibraryManager* m_libraryManager;
    TrackListContextPolicy m_trackListContextPolicy;
    QString m_trackListPlaceholder;
    bool m_escapeRichText;
    bool m_useVariousArtists;
    RatingStarSymbols m_ratingSymbols;
};

/*!
 * Reusable environment adapter for playlist/playback-backed scripting surfaces.
 *
 * This helper exposes playlist, track-list, playback, queue, rating-symbol, and
 * evaluation-policy state for widgets and plugins that evaluate scripts around
 * the current playback context.
 */
class FYCORE_EXPORT PlaylistScriptEnvironment : public ScriptEnvironment,
                                                public ScriptPlaylistEnvironment,
                                                public ScriptTrackListEnvironment,
                                                public ScriptPlaybackEnvironment,
                                                public ScriptEvaluationEnvironment
{
public:
    PlaylistScriptEnvironment();

    void setPlaylistData(const Playlist* playlist, const PlaybackQueue* playbackQueue,
                         const TrackList* tracks = nullptr, int queueTotal = 0);
    void setQueueState(std::span<const int> queueIndexes, int queueTotal);
    void setQueueIndexesVisible(bool visible);
    void setTrackState(int playlistTrackIndex, int currentPlayingTrackIndex, int currentPlayingTrackId, int trackDepth);
    void setPlaybackState(uint64_t currentPosition, uint64_t currentTrackDuration, int bitrate,
                          Player::PlayState playState);
    void setEvaluationPolicy(TrackListContextPolicy policy, QString placeholder, bool escapeRichText,
                             bool useVariousArtists = false);
    void setRatingStarSymbols(const RatingStarSymbols& ratingSymbols);

    [[nodiscard]] const ScriptPlaylistEnvironment* playlistEnvironment() const override;
    [[nodiscard]] const ScriptTrackListEnvironment* trackListEnvironment() const override;
    [[nodiscard]] const ScriptPlaybackEnvironment* playbackEnvironment() const override;
    [[nodiscard]] const ScriptEvaluationEnvironment* evaluationEnvironment() const override;

    [[nodiscard]] int currentPlaylistTrackIndex() const override;
    [[nodiscard]] int currentPlayingTrackIndex() const override;
    [[nodiscard]] int currentPlayingTrackId() const override;
    [[nodiscard]] int playlistTrackCount() const override;
    [[nodiscard]] int trackDepth() const override;
    [[nodiscard]] std::span<const int> currentQueueIndexes() const override;
    [[nodiscard]] int currentQueueTotal() const override;
    [[nodiscard]] const TrackList* trackList() const override;
    [[nodiscard]] uint64_t currentPosition() const override;
    [[nodiscard]] uint64_t currentTrackDuration() const override;
    [[nodiscard]] int bitrate() const override;
    [[nodiscard]] Player::PlayState playState() const override;
    [[nodiscard]] TrackListContextPolicy trackListContextPolicy() const override;
    [[nodiscard]] QString trackListPlaceholder() const override;
    [[nodiscard]] bool escapeRichText() const override;
    [[nodiscard]] bool useVariousArtists() const override;
    [[nodiscard]] RatingStarSymbols ratingStarSymbols() const override;
    [[nodiscard]] bool hasDirectQueueState() const;

private:
    const Playlist* m_playlist;
    const PlaybackQueue* m_playbackQueue;
    const TrackList* m_tracks;
    std::vector<int> m_directQueueIndexes;
    mutable std::vector<int> m_currentQueueIndexes;
    int m_playlistTrackIndex;
    int m_currentPlayingTrackIndex;
    int m_currentPlayingTrackId;
    int m_trackDepth;
    int m_queueTotal;
    uint64_t m_currentPosition;
    uint64_t m_currentTrackDuration;
    int m_bitrate;
    Player::PlayState m_playState;
    TrackListContextPolicy m_trackListContextPolicy;
    QString m_trackListPlaceholder;
    bool m_escapeRichText;
    bool m_useVariousArtists;
    RatingStarSymbols m_ratingSymbols;
    bool m_hasDirectQueueState;
    bool m_queueIndexesVisible;
};

struct FYCORE_EXPORT PlaybackScriptContext
{
    PlaylistScriptEnvironment environment;
    ScriptContext context;

    PlaybackScriptContext();
    PlaybackScriptContext(const PlaybackScriptContext&)            = delete;
    PlaybackScriptContext& operator=(const PlaybackScriptContext&) = delete;
    PlaybackScriptContext(PlaybackScriptContext&& other) noexcept;
    PlaybackScriptContext& operator=(PlaybackScriptContext&& other) noexcept;
};

[[nodiscard]] FYCORE_EXPORT PlaybackScriptContext makePlaybackScriptContext(
    PlayerController* playerController, Playlist* playlist, TrackListContextPolicy policy, QString placeholder = {},
    bool escapeRichText = false, bool useVariousArtists = false, const RatingStarSymbols& ratingSymbols = {});
} // namespace Fooyin
