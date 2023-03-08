/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "core/library/sorting/sortorder.h"
#include "core/models/trackfwd.h"

#include <QObject>

namespace Fy::Core::Library {
struct LibraryInfo;
class LibraryInteractor;

class MusicLibrary : public QObject
{
    Q_OBJECT

public:
    explicit MusicLibrary(QObject* parent = nullptr)
        : QObject{parent}
    { }

    virtual void loadLibrary() = 0;

    [[nodiscard]] virtual LibraryInfo* info() const = 0;

    virtual void reload() = 0;
    virtual void rescan() = 0;

    [[nodiscard]] virtual Track* track(int id) const     = 0;
    [[nodiscard]] virtual TrackPtrList tracks() const    = 0;
    [[nodiscard]] virtual TrackPtrList allTracks() const = 0;
    [[nodiscard]] virtual int trackCount() const         = 0;

    [[nodiscard]] virtual SortOrder sortOrder() const = 0;
    virtual void sortTracks(SortOrder order)          = 0;

    virtual void addInteractor(LibraryInteractor* interactor) = 0;

    virtual void removeLibrary(int id) = 0;

signals:
    void loadAllTracks(Core::Library::SortOrder order);
    void runLibraryScan(const Core::TrackPtrList& tracks);

    void tracksLoaded(const Core::TrackPtrList& tracks);
    void tracksAdded();
    void tracksUpdated();
    void tracksDeleted(const Core::TrackPtrList& tracks);

    void libraryRemoved();
    void libraryChanged();
    void tracksChanged();
};
} // namespace Fy::Core::Library
