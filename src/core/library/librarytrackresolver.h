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

#include "libraryscanutils.h"

#include <core/library/libraryinfo.h>
#include <core/track.h>

#include <functional>
#include <memory>

class QFileInfo;
class QObject;
class QUrl;

namespace Fooyin {
class AudioLoader;
class LibraryScanState;
class LibraryScanWriter;
class PlaylistLoader;
class RemoteDownloadHandle;
class RemoteIoService;
class TrackDatabase;
class TrackMetadataStore;

class FYCORE_EXPORT LibraryTrackResolver
{
public:
    using FlushWritesHandler = std::function<void()>;

    LibraryTrackResolver(LibraryInfo currentLibrary, PlaylistLoader* playlistLoader, AudioLoader* audioLoader,
                         bool playlistSkipMissing, std::shared_ptr<RemoteIoService> remoteIo,
                         std::shared_ptr<TrackMetadataStore> metadataStore, TrackDatabase* trackDatabase,
                         LibraryScanState* state, LibraryScanWriter* writer, TrackReloadOptions reloadOptions,
                         FlushWritesHandler flushWrites);
    LibraryTrackResolver(LibraryInfo currentLibrary, PlaylistLoader* playlistLoader, AudioLoader* audioLoader,
                         bool playlistSkipMissing, std::shared_ptr<TrackMetadataStore> metadataStore,
                         TrackDatabase* trackDatabase, LibraryScanState* state, LibraryScanWriter* writer,
                         TrackReloadOptions reloadOptions, FlushWritesHandler flushWrites);

    [[nodiscard]] TrackList readTracks(const QString& filepath);
    [[nodiscard]] TrackList readPlaylist(const QString& filepath);
    [[nodiscard]] TrackList readPlaylist(const QUrl& url);
    [[nodiscard]] TrackList readPlaylistTracks(const QString& path);
    [[nodiscard]] TrackList readPlaylistTracks(const QUrl& url);
    [[nodiscard]] std::shared_ptr<RemoteDownloadHandle>
    readPlaylistAsync(const QUrl& url, QObject* context,
                      std::function<void(bool completed, TrackList tracks)> callback);
    [[nodiscard]] size_t countPlaylistTracks(const QString& path) const;
    [[nodiscard]] size_t countPlaylistTracks(const QUrl& url) const;
    [[nodiscard]] TrackList readEmbeddedPlaylistTracks(const Track& track);

    void readCue(const QString& cue, bool onlyModified);
    void readCue(const QFileInfo& info, bool onlyModified);
    void readFile(const QString& file, bool onlyModified);
    void readFile(const QFileInfo& info, bool onlyModified);

private:
    [[nodiscard]] TrackList readArchiveTracks(const QString& filepath);

    void setTrackProps(Track& track);
    void setTrackProps(Track& track, const QString& file);

    void updateExistingTrack(Track& track, const QString& file);
    void readNewTrack(const QString& file);

    LibraryInfo m_currentLibrary;
    PlaylistLoader* m_playlistLoader;
    AudioLoader* m_audioLoader;
    std::shared_ptr<RemoteIoService> m_remoteIo;
    std::shared_ptr<TrackMetadataStore> m_metadataStore;
    TrackDatabase* m_trackDatabase;
    LibraryScanState* m_state;
    LibraryScanWriter* m_writer;
    TrackReloadOptions m_reloadOptions;

    FlushWritesHandler m_flushWrites;

    bool m_playlistSkipMissing;
};
} // namespace Fooyin
