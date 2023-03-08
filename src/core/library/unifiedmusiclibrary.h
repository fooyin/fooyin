/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "musiclibraryinternal.h"
#include "unifiedtrackstore.h"

namespace Fy::Core::Library {
class LibraryManager;

class UnifiedMusicLibrary : public MusicLibraryInternal
{
    Q_OBJECT

public:
    explicit UnifiedMusicLibrary(LibraryInfo* info, LibraryManager* libraryManager, QObject* parent = nullptr);

    void reload() override;
    void rescan() override;

    [[nodiscard]] LibraryInfo* info() const override;

    [[nodiscard]] Track* track(int id) const override;
    [[nodiscard]] TrackPtrList tracks() const override;
    [[nodiscard]] int trackCount() const override;

    [[nodiscard]] UnifiedTrackStore* trackStore() const override;

    [[nodiscard]] SortOrder sortOrder() const override;
    void sortTracks(SortOrder order) override;

private:
    LibraryInfo* m_info;
    LibraryManager* m_libraryManager;
    std::unique_ptr<UnifiedTrackStore> m_trackStore;

    SortOrder m_order{Library::SortOrder::YearDesc};
};
} // namespace Fy::Core::Library
