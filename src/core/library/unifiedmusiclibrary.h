/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "libraryscanner.h"

#include <core/library/musiclibrary.h>

namespace Fooyin {
class SettingsManager;
struct LibraryInfo;
class TagLoader;

class UnifiedMusicLibrary : public MusicLibrary
{
    Q_OBJECT

public:
    UnifiedMusicLibrary(LibraryManager* libraryManager, DbConnectionPoolPtr dbPool,
                        std::shared_ptr<PlaylistLoader> playlistLoader, std::shared_ptr<TagLoader> tagLoader,
                        SettingsManager* settings, QObject* parent = nullptr);
    ~UnifiedMusicLibrary() override;

    void loadAllTracks() override;

    void refreshAll() override;
    void rescanAll() override;
    ScanRequest refresh(const LibraryInfo& library) override;
    ScanRequest rescan(const LibraryInfo& library) override;
    ScanRequest scanTracks(const TrackList& tracks) override;
    ScanRequest scanFiles(const QList<QUrl>& files) override;

    [[nodiscard]] bool hasLibrary() const override;
    [[nodiscard]] bool isEmpty() const override;

    [[nodiscard]] TrackList tracks() const override;
    [[nodiscard]] TrackList tracksForIds(const TrackIds& ids) const override;

    void updateTrackMetadata(const TrackList& tracks) override;
    void updateTrackStats(const Track& track) override;

    void trackWasPlayed(const Track& track);
    void cleanupTracks();

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
