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

#include "core/models/trackfwd.h"

#include <QObject>

namespace Fy::Core::Library {
struct LibraryInfo;

class MusicLibrary : public QObject
{
    Q_OBJECT

public:
    explicit MusicLibrary(QObject* parent = nullptr)
        : QObject{parent}
    { }

    [[nodiscard]] virtual bool hasLibrary() const = 0;

    virtual void loadLibrary() = 0;

    virtual bool isEmpty() const = 0;

    virtual void reloadAll()                        = 0;
    virtual void reload(const LibraryInfo& library) = 0;
    virtual void rescan()                           = 0;

    [[nodiscard]] virtual TrackList tracks() const = 0;

    virtual void removeLibrary(int id) = 0;

signals:
    void loadAllTracks();
    void runLibraryScan(const Library::LibraryInfo& library, const Core::TrackList& tracks);
    void scanProgress(int percent);

    void tracksLoaded(const Core::TrackList& tracks);
    void tracksAdded(const Core::TrackList& tracks);
    void tracksUpdated(const Core::TrackList& tracks);
    void tracksDeleted(const Core::TrackList& tracks);
    void tracksSorted(const Core::TrackList& tracks);

    void libraryAdded();
    void libraryRemoved(int id);
    void libraryChanged();
};
} // namespace Fy::Core::Library
