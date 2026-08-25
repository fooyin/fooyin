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

#include "fygui_export.h"

#include "metadatatypes.h"

#include <core/track.h>

namespace Fooyin {
struct TrackMatch
{
    size_t localIndex{0};
    std::optional<size_t> remoteIndex;
    int confidence{0};
    bool ambiguous{false};
    bool manual{false};
};

FYGUI_EXPORT QString normaliseMatchText(const QString& text);
FYGUI_EXPORT std::vector<TrackMatch> matchTracks(const TrackList& localTracks, const Release& release);
FYGUI_EXPORT std::vector<TrackMatch> matchTracksByPosition(const TrackList& localTracks, const Release& release);
} // namespace Fooyin
