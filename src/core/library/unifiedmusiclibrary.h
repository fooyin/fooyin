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
struct LibraryInfo;
class SettingsManager;
class UnifiedMusicLibraryPrivate;

class UnifiedMusicLibrary : public MusicLibrary
{
    Q_OBJECT

public:
    UnifiedMusicLibrary(LibraryManager* libraryManager, DbConnectionPoolPtr dbPool,
                        std::shared_ptr<PlaylistLoader> playlistLoader, std::shared_ptr<AudioLoader> audioLoader,
                        SettingsManager* settings, QObject* parent = nullptr);
    ~UnifiedMusicLibrary() override;

    [[nodiscard]] bool hasLibrary() const override;
    [[nodiscard]] std::optional<LibraryInfo> libraryInfo(int id) const override;
    [[nodiscard]] std::optional<LibraryInfo> libraryForPath(const QString& path) const override;

    void loadAllTracks() override;

    [[nodiscard]] bool isEmpty() const override;

    void refreshAll() override;
    void rescanAll() override;

    ScanRequest refresh(const LibraryInfo& library) override;
    ScanRequest rescan(const LibraryInfo& library) override;

    ScanRequest scanTracks(const TrackList& tracks) override;
    ScanRequest scanModifiedTracks(const TrackList& tracks) override;
    ScanRequest scanFiles(const QList<QUrl>& files) override;
    ScanRequest loadPlaylist(const QList<QUrl>& files) override;

    [[nodiscard]] TrackList tracks() const override;
    [[nodiscard]] Track trackForId(int id) const override;
    [[nodiscard]] TrackList tracksForIds(const TrackIds& ids) const override;

    void updateTrack(const Track& track) override;
    void updateTracks(const TrackList& tracks) override;

    void updateTrackMetadata(const TrackList& tracks) override;
    WriteRequest writeTrackMetadata(const TrackList& tracks) override;
    WriteRequest writeTrackCovers(const TrackCoverData& tracks) override;

    void updateTrackStats(const TrackList& tracks) override;
    void updateTrackStats(const Track& track) override;

    void trackWasPlayed(const Track& track);
    void cleanupTracks();
    WriteRequest removeUnavailbleTracks() override;

private:
    std::unique_ptr<UnifiedMusicLibraryPrivate> p;
};
} // namespace Fooyin
