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

#pragma once

#include "fycore_export.h"

#include <core/player/playerdefs.h>
#include <core/scripting/expression.h>
#include <core/scripting/scriptvalue.h>
#include <core/track.h>

#include <span>
#include <variant>

namespace Fooyin {
class Playlist;

/*!
 * Controls how list-scoped variables behave when evaluation begins from a single Track.
 */
enum class TrackListContextPolicy : uint8_t
{
    Unresolved = 0,
    Placeholder,
    Fallback,
};

/*!
 * Playlist-specific capability interface used by variables such as `%list_index%`.
 */
class FYCORE_EXPORT ScriptPlaylistEnvironment
{
public:
    virtual ~ScriptPlaylistEnvironment() = default;

    [[nodiscard]] virtual int currentPlaylistTrackIndex() const = 0;
    [[nodiscard]] virtual int currentPlayingTrackIndex() const
    {
        return -1;
    }
    [[nodiscard]] virtual int playlistTrackCount() const                   = 0;
    [[nodiscard]] virtual int trackDepth() const                           = 0;
    [[nodiscard]] virtual std::span<const int> currentQueueIndexes() const = 0;
};

/*!
 * Exposes the active TrackList to list-scoped variables.
 */
class FYCORE_EXPORT ScriptTrackListEnvironment
{
public:
    virtual ~ScriptTrackListEnvironment() = default;

    [[nodiscard]] virtual const TrackList* trackList() const = 0;
};

/*!
 * Playback capability interface used by base playback variables.
 */
class FYCORE_EXPORT ScriptPlaybackEnvironment
{
public:
    virtual ~ScriptPlaybackEnvironment() = default;

    [[nodiscard]] virtual uint64_t currentPosition() const      = 0;
    [[nodiscard]] virtual uint64_t currentTrackDuration() const = 0;
    [[nodiscard]] virtual int bitrate() const                   = 0;
    [[nodiscard]] virtual Player::PlayState playState() const   = 0;
};

/*!
 * Library capability interface used by base library variables.
 */
class FYCORE_EXPORT ScriptLibraryEnvironment
{
public:
    virtual ~ScriptLibraryEnvironment() = default;

    [[nodiscard]] virtual QString libraryName(const Track& track) const  = 0;
    [[nodiscard]] virtual QString libraryPath(const Track& track) const  = 0;
    [[nodiscard]] virtual QString relativePath(const Track& track) const = 0;
};

/*!
 * Evaluation policy hooks that affect fallback and output formatting behaviour.
 */
class FYCORE_EXPORT ScriptEvaluationEnvironment
{
public:
    virtual ~ScriptEvaluationEnvironment() = default;

    [[nodiscard]] virtual TrackListContextPolicy trackListContextPolicy() const = 0;
    [[nodiscard]] virtual QString trackListPlaceholder() const                  = 0;
    [[nodiscard]] virtual bool escapeRichText() const                           = 0;
    [[nodiscard]] virtual bool useVariousArtists() const
    {
        return false;
    }
    [[nodiscard]] virtual bool replacePathSeparators() const
    {
        return false;
    }
};

/*!
 * Aggregates the optional capabilities available to a script evaluation.
 */
class FYCORE_EXPORT ScriptEnvironment
{
public:
    virtual ~ScriptEnvironment() = default;

    [[nodiscard]] virtual const ScriptPlaylistEnvironment* playlistEnvironment() const
    {
        return nullptr;
    }

    [[nodiscard]] virtual const ScriptTrackListEnvironment* trackListEnvironment() const
    {
        return nullptr;
    }

    [[nodiscard]] virtual const ScriptPlaybackEnvironment* playbackEnvironment() const
    {
        return nullptr;
    }

    [[nodiscard]] virtual const ScriptLibraryEnvironment* libraryEnvironment() const
    {
        return nullptr;
    }

    [[nodiscard]] virtual const ScriptEvaluationEnvironment* evaluationEnvironment() const
    {
        return nullptr;
    }
};

/*!
 * Runtime evaluation state applied around a single script invocation.
 */
struct ScriptContext
{
    const ScriptEnvironment* environment{nullptr};
    const Track* track{nullptr};
    const TrackList* tracks{nullptr};
    const Playlist* playlist{nullptr};
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
