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

#include "libraryscanwriter.h"

#include "database/trackdatabase.h"
#include "libraryscanner.h"

constexpr auto BatchSize = 250;

namespace Fooyin {
LibraryScanWriter::LibraryScanWriter(TrackDatabase* trackDatabase, FlushHandler flushHandler)
    : m_trackDatabase{trackDatabase}
    , m_flushHandler{std::move(flushHandler)}
{ }

void LibraryScanWriter::reset()
{
    m_tracksToStore.clear();
    m_tracksToUpdate.clear();
}

void LibraryScanWriter::storeTrack(Track track)
{
    m_tracksToStore.push_back(std::move(track));
}

void LibraryScanWriter::updateTrack(Track track)
{
    m_tracksToUpdate.push_back(std::move(track));
}

bool LibraryScanWriter::empty() const
{
    return m_tracksToStore.empty() && m_tracksToUpdate.empty();
}

bool LibraryScanWriter::shouldFlush() const
{
    return m_tracksToStore.size() >= BatchSize || m_tracksToUpdate.size() >= BatchSize;
}

void LibraryScanWriter::flush()
{
    if(empty()) {
        return;
    }

    if(!m_tracksToStore.empty()) {
        m_trackDatabase->storeTracks(m_tracksToStore);
    }
    if(!m_tracksToUpdate.empty()) {
        m_trackDatabase->updateTracks(m_tracksToUpdate);
    }

    ScanResult result;
    result.addedTracks   = m_tracksToStore;
    result.updatedTracks = m_tracksToUpdate;
    m_flushHandler(result);

    m_tracksToStore.clear();
    m_tracksToUpdate.clear();
}
} // namespace Fooyin
