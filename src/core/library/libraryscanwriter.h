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

#include "libraryscantypes.h"

#include <expected>
#include <functional>
#include <set>

namespace Fooyin {
class TrackDatabase;

class FYCORE_EXPORT LibraryScanWriter
{
public:
    using FlushHandler = std::function<void(const ScanResult&)>;

    LibraryScanWriter(TrackDatabase* trackDatabase, FlushHandler flushHandler, bool overwritePlaycount = false);

    void reset();
    void storeTrack(Track track);
    void updateTrack(Track track);

    [[nodiscard]] bool empty() const;
    [[nodiscard]] bool shouldFlush() const;
    std::expected<void, QString> flush();

private:
    TrackDatabase* m_trackDatabase;
    FlushHandler m_flushHandler;
    bool m_overwritePlaycount;
    TrackList m_tracksToStore;
    TrackList m_tracksToUpdate;
    std::set<std::pair<QString, QString>> m_statisticsMigrations;
};
} // namespace Fooyin
