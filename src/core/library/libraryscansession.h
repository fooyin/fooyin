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

#include "libraryscantypes.h"

#include "libraryfileenumerator.h"
#include "libraryscanstate.h"
#include "libraryscanwriter.h"
#include "librarytrackresolver.h"

namespace Fooyin {
class AudioLoader;
class PlaylistLoader;
class TrackDatabase;

class LibraryScanSession
{
public:
    enum class EnumerationMode : uint8_t
    {
        Library = 0,
        External,
    };

    LibraryScanSession(TrackDatabase* trackDatabase, PlaylistLoader* playlistLoader, AudioLoader* audioLoader,
                       LibraryScanConfig config, LibraryScanHost* host);
    ~LibraryScanSession();

    bool scanLibrary(const LibraryInfo& library, const TrackList& tracks, bool onlyModified);
    bool scanDirectories(const LibraryInfo& library, const QStringList& dirs, const TrackList& tracks);
    bool scanFiles(const TrackList& libraryTracks, const QList<QUrl>& urls, LibraryScanFilesResult& result);
    bool scanPlaylist(const TrackList& libraryTracks, const QList<QUrl>& urls, TrackList& result);

    void finish();

    [[nodiscard]] size_t filesScanned() const;
    [[nodiscard]] size_t progressCount() const;
    [[nodiscard]] size_t discoveredFiles() const;

private:
    void reset(const LibraryInfo& library);
    void flushWriter(bool finalFlush = false);
    void maybeFlushWriter();
    LibraryTrackResolver makeResolver();
    bool handleEnumeratedFile(const QFileInfo& info, EnumeratedFileType type);
    void handleScanWriterFlush(const ScanResult& result);
    void flushTrackResolverWrites();
    void finaliseMissingTracks();

    LibraryScanHost* m_host;
    LibraryScanConfig m_config;
    TrackDatabase* m_trackDatabase;
    PlaylistLoader* m_playlistLoader;
    AudioLoader* m_audioLoader;

    LibraryScanState m_state;
    LibraryScanWriter m_writer;
    LibraryInfo m_currentLibrary;
    LibraryTrackResolver* m_resolver;
    LibraryScanFilesResult* m_fileScanResult;

    bool m_onlyModified;
    EnumerationMode m_enumerationMode;
    ScanProgress::Phase m_phase;
};
} // namespace Fooyin
