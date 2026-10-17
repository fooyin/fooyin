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

#include <core/library/libraryinfo.h>
#include <core/player/playbackqueue.h>
#include <core/player/playerdefs.h>
#include <core/ratingsymbols.h>
#include <core/scripting/expression.h>
#include <core/scripting/scriptvalue.h>
#include <core/track.h>
#include <utils/id.h>

#include <span>
#include <variant>

namespace Fooyin {
class Playlist;
class PlayerController;

/*!
 * Controls how list-scoped variables behave when evaluation begins from a single Track.
 */
enum class TrackListContextPolicy : uint8_t
{
    Unresolved = 0,
    Placeholder,
    Fallback,
};

enum class ScriptQueueItemState : uint8_t
{
    None = 0,
    Current,
    Upcoming,
    Played,
};

enum class ScriptPlayingIcon : uint8_t
{
    None = 0,
    Play,
    Pause,
    Stop,
    Disabled,
};

enum class ScriptWhitespaceMode : uint8_t
{
    IgnoreLayout = 0,
    Preserve,
};

struct ScriptEvaluationOptions
{
    ScriptWhitespaceMode whitespaceMode{ScriptWhitespaceMode::IgnoreLayout};
    QString missingVariableText{};
    TrackListContextPolicy trackListContextPolicy{TrackListContextPolicy::Unresolved};
    QString trackListPlaceholder{};
    bool escapeRichText{false};
    bool useVariousArtists{false};
    RatingStarSymbols ratingSymbols{defaultRatingStarSymbols()};
    bool replacePathSeparators{false};
};

struct ScriptPlaylistData
{
    UId playlistId{};
    UId entryId{};
    QString name{};
    int trackCount{0};
    int trackIndex{-1};
    int playingTrackIndex{-1};
    int playingTrackId{-1};
    int depth{0};
    std::vector<int> queueIndexes{};
    int queueTotal{0};
    ScriptQueueItemState queueItemState{ScriptQueueItemState::None};
    ScriptPlayingIcon playingIcon{ScriptPlayingIcon::None};
};

struct ScriptPlaybackData
{
    uint64_t position{0};
    uint64_t duration{0};
    int bitrate{0};
    QString decoder{};
    Engine::PlaybackOutputInfo outputInfo{};
    Player::PlayState playState{Player::PlayState::Stopped};
    PlaybackQueueMode queueMode{PlaybackQueueMode::PlaylistWithOverrides};
};

struct ScriptVariableValue
{
    QString name{};
    ScriptResult result;
};

/*!
 * Evaluation data. All referenced data must outlive the invocation.
 * Named values supply contextual variables and built-ins without registered callbacks.
 * Playback and playlist data are snapshots, safe to pass to scripting workers.
 */
struct ScriptContext
{
    const Track* track{nullptr};
    const TrackList* tracks{nullptr};
    const Playlist* playlist{nullptr};
    const ScriptPlaylistData* playlistData{nullptr};
    const ScriptPlaybackData* playback{nullptr};
    const LibraryInfoMap* libraries{nullptr};
    std::span<const ScriptVariableValue> values{};
};

/*! Owns snapshots */
struct ScriptContextData
{
    ScriptPlaylistData playlistData;
    ScriptPlaybackData playback;
    const TrackList* tracks{nullptr};
    const Playlist* playlist{nullptr};

    [[nodiscard]] ScriptContext context() const
    {
        return {.tracks = tracks, .playlist = playlist, .playlistData = &playlistData, .playback = &playback};
    }
};

/*! Call on the owning thread; the returned playback and row data are snapshots. */
[[nodiscard]] FYCORE_EXPORT ScriptContextData makePlaybackScriptData(PlayerController* playerController,
                                                                     Playlist* playlist = nullptr);

struct ScriptEvaluationContext
{
    ScriptContextData data;
    ScriptEvaluationOptions options;
};

/*!
 * The explicit subject being evaluated by the script engine.
 */
struct ScriptSubject
{
    const Track* track{nullptr};
    const TrackList* tracks{nullptr};
    const Playlist* playlist{nullptr};

    [[nodiscard]] bool isEmpty() const
    {
        return track == nullptr && tracks == nullptr && playlist == nullptr;
    }
};

/*!
 * Creates a subject for single-track evaluation.
 */
[[nodiscard]] inline ScriptSubject makeScriptSubject(const Track& track)
{
    return {.track = &track};
}

/*!
 * Creates a subject for track-list evaluation.
 */
[[nodiscard]] inline ScriptSubject makeScriptSubject(const TrackList& tracks)
{
    return {.tracks = &tracks};
}

/*!
 * Creates a subject for playlist evaluation.
 */
[[nodiscard]] inline ScriptSubject makeScriptSubject(const Playlist& playlist)
{
    return {.playlist = &playlist};
}

using ScriptFunctionId = uint32_t;
constexpr ScriptFunctionId InvalidScriptFunctionId{0};

using ScriptFieldValue = std::variant<int, uint64_t, float, QString, QStringList>;
} // namespace Fooyin
