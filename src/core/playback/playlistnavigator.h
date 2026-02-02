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

#include <core/engine/audioloader.h>
#include <core/playlist/playlist.h>

#include <memory>

namespace Fooyin {
class FYCORE_EXPORT PlaylistNavigator
{
public:
    explicit PlaylistNavigator(std::shared_ptr<AudioLoader> audioLoader);

    PlaylistTrack currentTrack(Playlist* playlist) const;
    PlaylistTrack previewRelativeTrack(Playlist* playlist, Playlist::PlayModes mode, int delta) const;
    PlaylistTrack advanceRelativeTrack(Playlist* playlist, Playlist::PlayModes mode, int delta) const;

private:
    [[nodiscard]] PlaylistTrack populateTrackMetadata(Playlist* playlist, Track track, int index) const;

    std::shared_ptr<AudioLoader> m_audioLoader;
};
} // namespace Fooyin
