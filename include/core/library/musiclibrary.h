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

#include "fycore_export.h"

#include <core/track.h>

#include <QObject>

namespace Fooyin {
struct LibraryInfo;

struct ScanRequest
{
    enum Type : uint8_t
    {
        Tracks = 0,
        Library,
    };

    Type type;
    int id{-1};
    std::function<void()> cancel;
};

class FYCORE_EXPORT MusicLibrary : public QObject
{
    Q_OBJECT

public:
    explicit MusicLibrary(QObject* parent = nullptr)
        : QObject{parent}
    { }

    [[nodiscard]] virtual bool hasLibrary() const = 0;

    virtual void loadLibrary() = 0;

    [[nodiscard]] virtual bool isEmpty() const = 0;

    virtual void reloadAll()                        = 0;
    virtual void reload(const LibraryInfo& library) = 0;
    virtual void rescan()                           = 0;

    virtual ScanRequest* scanTracks(const TrackList& tracks) = 0;

    [[nodiscard]] virtual TrackList tracks() const                          = 0;
    [[nodiscard]] virtual TrackList tracksForIds(const TrackIds& ids) const = 0;

    virtual void updateTrackMetadata(const TrackList& tracks) = 0;

    virtual void removeLibrary(int id) = 0;

signals:
    void scanProgress(int id, int percent);

    void tracksLoaded(const TrackList& tracks);
    void tracksAdded(const TrackList& tracks);
    void tracksScanned(const TrackList& tracks);
    void tracksUpdated(const TrackList& tracks);
    void tracksDeleted(const TrackList& tracks);
    void tracksSorted(const TrackList& tracks);

    void libraryAdded();
    void libraryRemoved(int id);
    void libraryChanged();
};
} // namespace Fooyin
