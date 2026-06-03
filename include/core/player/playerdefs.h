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

#include "fycore_export.h"

#include <core/playlist/playlist.h>
#include <core/track.h>
#include <utils/id.h>

namespace Fooyin::Player {
Q_NAMESPACE_EXPORT(FYCORE_EXPORT)

enum class PlayState : uint8_t
{
    Playing = 0,
    Paused,
    Stopped,
};
Q_ENUM_NS(PlayState)

enum class AdvanceReason : uint8_t
{
    Unknown = 0,
    StartupRestore,
    ManualSelection,
    ManualNext,
    ManualPrevious,
    NaturalEnd,
    PreparedCrossfade,
    PreparedCommit,
    PreparedCrossfadeCommit,
};
Q_ENUM_NS(AdvanceReason)

struct FYCORE_EXPORT TrackChangeContext
{
    AdvanceReason reason{AdvanceReason::Unknown};
    bool userInitiated{false};
};

struct FYCORE_EXPORT TrackChangeRequest
{
    PlaylistTrack track;
    TrackChangeContext context;
    bool isQueueTrack{false};
    uint64_t itemId{0};
};

struct FYCORE_EXPORT UpcomingTrack
{
    PlaylistTrack track;
    bool isQueueTrack{false};
    uint64_t itemId{0};
};

struct FYCORE_EXPORT PlaybackSnapshot
{
    PlayState playState{PlayState::Stopped};
    Track track;
    UId playlistId;
    int indexInPlaylist{-1};
    uint64_t positionMs{0};
    uint64_t durationMs{0};
    int bitrate{0};
    bool isQueueTrack{false};
};
} // namespace Fooyin::Player

Q_DECLARE_METATYPE(Fooyin::Player::AdvanceReason)
Q_DECLARE_METATYPE(Fooyin::Player::TrackChangeContext)
Q_DECLARE_METATYPE(Fooyin::Player::TrackChangeRequest)
Q_DECLARE_METATYPE(Fooyin::Player::UpcomingTrack)
Q_DECLARE_METATYPE(Fooyin::Player::PlaybackSnapshot)
