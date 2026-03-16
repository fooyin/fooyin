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

#include <core/library/libraryinfo.h>
#include <core/track.h>

#include <functional>

class QFileInfo;

namespace Fooyin {
class AudioLoader;
class LibraryScanState;
class LibraryScanWriter;
class PlaylistLoader;
class TrackDatabase;

class LibraryTrackResolver
{
public:
    using FlushWritesHandler = std::function<void()>;

    LibraryTrackResolver(LibraryInfo currentLibrary, PlaylistLoader* playlistLoader, AudioLoader* audioLoader,
                         bool playlistSkipMissing, TrackDatabase* trackDatabase, LibraryScanState* state,
                         LibraryScanWriter* writer, FlushWritesHandler flushWrites);

    [[nodiscard]] TrackList readTracks(const QString& filepath);
    [[nodiscard]] TrackList readPlaylist(const QString& filepath);
    [[nodiscard]] TrackList readPlaylistTracks(const QString& path);
    [[nodiscard]] size_t countPlaylistTracks(const QString& path) const;
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
    TrackDatabase* m_trackDatabase;
    LibraryScanState* m_state;
    LibraryScanWriter* m_writer;

    FlushWritesHandler m_flushWrites;

    bool m_playlistSkipMissing;
};
} // namespace Fooyin
