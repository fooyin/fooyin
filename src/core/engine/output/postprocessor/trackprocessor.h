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

#include <core/engine/audioformat.h>
#include <core/track.h>

#include <cstddef>

namespace Fooyin {
/*!
 * Per-stream track-processing interface applied inside AudioMixer.
 *
 * TrackProcessors run before per-track DSP nodes for each stream. Instances are
 * stream-local and invoked on the audio thread.
 */
class FYCORE_EXPORT TrackProcessor
{
public:
    virtual ~TrackProcessor() = default;

    //! Initialize for a new track
    virtual void init(const Track& track, const AudioFormat& format) = 0;

    //! True if this processor needs to run for the current track.
    [[nodiscard]] virtual bool isActive() const = 0;

    //! Process interleaved F64 samples in-place.
    virtual void process(double* samples, size_t sampleCount) = 0;

    //! Called at end of track before switching to next track.
    virtual void endOfTrack() = 0;

    //! Reset internal state (e.g. after seek).
    virtual void reset() = 0;
};
} // namespace Fooyin
