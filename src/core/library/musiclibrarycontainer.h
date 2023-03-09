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

#include "musiclibrary.h"
#include "musiclibraryinternal.h"

namespace Fy::Core::Library {
class MusicLibraryContainer : public MusicLibrary
{
    Q_OBJECT

public:
    explicit MusicLibraryContainer(MusicLibraryInternal* library = nullptr, QObject* parent = nullptr);

    void loadLibrary() override;

    [[nodiscard]] LibraryInfo* info() const override;

    void reload() override;
    void rescan() override;

    void changeLibrary(MusicLibraryInternal* library);

    [[nodiscard]] Track* track(int id) const override;
    [[nodiscard]] TrackPtrList tracks() const override;
    [[nodiscard]] TrackPtrList allTracks() const override;

    [[nodiscard]] SortOrder sortOrder() const override;
    void sortTracks(SortOrder order) override;

    void addInteractor(LibraryInteractor* interactor) override;

    void removeLibrary(int id) override;

private:
    using LibraryInteractors = std::vector<LibraryInteractor*>;

    MusicLibraryInternal* m_currentLibrary;
    LibraryInteractors m_interactors;
};
} // namespace Fy::Core::Library
