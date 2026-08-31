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

#include "libraryscanutils.h"

#include <core/track.h>

#include <functional>
#include <memory>

namespace Fooyin {
class AudioLoader;
class TrackMetadataStore;

struct CueTrackReloadResult
{
    TrackList addedTracks;
    TrackList updatedTracks;
    bool cancelled{false};
};

class CueTrackReloader
{
public:
    using ContinueHandler = std::function<bool()>;
    using ProgressHandler = std::function<void(int count, const QString& source)>;

    CueTrackReloader(AudioLoader* audioLoader, std::shared_ptr<TrackMetadataStore> metadataStore);

    [[nodiscard]] CueTrackReloadResult reload(const TrackList& tracks, bool onlyModified,
                                              const TrackReloadOptions& options, const ContinueHandler& shouldContinue,
                                              const ProgressHandler& reportProgress) const;

private:
    AudioLoader* m_audioLoader;
    std::shared_ptr<TrackMetadataStore> m_metadataStore;
};
} // namespace Fooyin
