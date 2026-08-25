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

#include <core/library/musiclibrary.h>

#include <QFile>
#include <QTemporaryDir>

#include <memory>
#include <optional>

namespace Fooyin {
class Playlist;
class SettingsManager;
} // namespace Fooyin

namespace Fooyin::Testing {
[[nodiscard]] QString testFilePath(const QString& relativePath);
void resetRatingSettings();

class TempResource : public QFile
{
public:
    explicit TempResource(const QString& filename, QObject* parent = nullptr);
    ~TempResource() override;

    [[nodiscard]] QString fileName();
    bool seek(qint64 position) override;
    void checkValid() const;

private:
    QTemporaryDir m_tempDir;
    QString m_file;
};

class PlaylistTestUtils
{
public:
    static std::unique_ptr<Playlist> createPlaylist(const QString& name, Fooyin::SettingsManager* settings);
    static void replaceTracks(Playlist& playlist, const TrackList& tracks);
    static void changeCurrentIndex(Playlist& playlist, int index);
};

class StubMusicLibrary : public MusicLibrary
{
public:
    using MusicLibrary::MusicLibrary;

    void setTracks(TrackList tracks);
    void setLibraryTracks(TrackList tracks);
    void setLibrary(LibraryInfo library);

    void emitTracksLoaded();
    void emitTracksUpdatedForTests(const TrackList& tracks);

    [[nodiscard]] bool hasLibrary() const override;
    [[nodiscard]] std::optional<LibraryInfo> libraryInfo(int id) const override;
    [[nodiscard]] std::optional<LibraryInfo> libraryForPath(const QString& path) const override;

    void loadAllTracks() override;
    [[nodiscard]] bool isEmpty() const override;
    void refreshAll() override;
    void rescanAll() override;
    ScanRequest refresh(const LibraryInfo& library) override;
    ScanRequest rescan(const LibraryInfo& library) override;
    void cancelScan(int id) override;
    ScanRequest scanTracks(const TrackList& tracks) override;
    ScanRequest scanModifiedTracks(const TrackList& tracks) override;
    ScanRequest scanFiles(const QList<QUrl>& files) override;
    ScanRequest loadPlaylist(const QList<QUrl>& files) override;

    [[nodiscard]] TrackList tracks() const override;
    [[nodiscard]] TrackList libraryTracks() const override;
    [[nodiscard]] Track trackForId(int id) const override;
    [[nodiscard]] TrackList tracksForIds(const TrackIds& ids) const override;
    [[nodiscard]] std::shared_ptr<TrackMetadataStore> metadataStore() const override;

    void updateTrack(const Track& track) override;
    void updateTracks(const TrackList& tracks) override;
    void updateTrackMetadata(const TrackList& tracks) override;
    WriteRequest writeTrackMetadata(const TrackList& tracks) override;
    WriteRequest writeTrackCovers(const TrackCoverData& coverData) override;
    [[nodiscard]] PendingTrackCoverProvider* pendingTrackCoverProvider() const override;
    void updateTrackStats(const TrackList& tracks) override;
    void updateTrackStats(const Track& track) override;
    WriteRequest removeUnavailbleTracks() override;
    WriteRequest deleteTracks(const TrackList& tracks) override;

private:
    TrackList m_tracks;
    std::optional<TrackList> m_libraryTracks;
    std::optional<LibraryInfo> m_library;
};
} // namespace Fooyin::Testing
