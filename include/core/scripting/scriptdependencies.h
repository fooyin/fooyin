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

#include <cstdint>
#include <vector>

namespace Fooyin {
using ScriptDataSourceId = uint32_t;

constexpr ScriptDataSourceId InvalidScriptDataSource = 0;

namespace ScriptSources {
constexpr ScriptDataSourceId TrackMetadata    = 1;
constexpr ScriptDataSourceId PlaybackTrack    = 2;
constexpr ScriptDataSourceId PlaybackState    = 3;
constexpr ScriptDataSourceId PlaybackTime     = 4;
constexpr ScriptDataSourceId PlaybackOutput   = 5;
constexpr ScriptDataSourceId PlaybackStopped  = 6;
constexpr ScriptDataSourceId Playlist         = 7;
constexpr ScriptDataSourceId PlaylistPlayback = 8;
constexpr ScriptDataSourceId Queue            = 9;
constexpr ScriptDataSourceId QueueCursor      = 10;
constexpr ScriptDataSourceId Libraries        = 11;
constexpr ScriptDataSourceId Clock            = 12;
} // namespace ScriptSources

struct ScriptDependencies
{
    std::vector<ScriptDataSourceId> sources{};
    bool unclassified{true};

    void merge(const ScriptDependencies& other)
    {
        for(const auto source : other.sources) {
            if(!contains(source)) {
                sources.push_back(source);
            }
        }
        unclassified |= other.unclassified;
    }

    [[nodiscard]] bool contains(ScriptDataSourceId source) const
    {
        return std::ranges::find(sources, source) != sources.end();
    }
};

struct ScriptSourceRevision
{
    ScriptDataSourceId source{InvalidScriptDataSource};
    uint64_t revision{0};
};
} // namespace Fooyin
