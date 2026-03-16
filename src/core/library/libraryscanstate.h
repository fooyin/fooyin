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

#include <core/library/musiclibrary.h>

#include <unordered_map>

namespace Fooyin {
class LibraryScanHost;

class LibraryScanState
{
public:
    explicit LibraryScanState(LibraryScanHost* host);

    void reset();

    [[nodiscard]] bool mayRun() const;
    [[nodiscard]] bool stopRequested() const;

    void setProgressPhase(ScanProgress::Phase phase, size_t total = 0);
    void setReportEnumeratingProgress(bool report);
    void reportProgress(const QString& file = {}) const;
    void fileDiscovered(const QString& file);
    void fileScanned(const QString& file);
    void progressScanned(const QString& file);

    [[nodiscard]] bool pathExists(const QString& path);
    void populateExistingTracks(const TrackList& tracks, const QStringList& roots, bool scopeMissing);
    [[nodiscard]] bool isScopedTrack(const Track& track) const;

    void addRelocationCandidate(const Track& track);
    void removeRelocationCandidate(const Track& track);
    void markTrackSeen(const Track& track);
    void markTracksSeen(const TrackList& tracks);

    [[nodiscard]] Track matchRelocatedTrack(const Track& track);
    [[nodiscard]] std::optional<Track> findExistingTrackByUniqueFilepath(const Track& track) const;

    [[nodiscard]] const std::unordered_map<QString, TrackList>& trackPaths() const;
    [[nodiscard]] const std::unordered_map<QString, TrackList>& existingArchives() const;
    [[nodiscard]] const std::unordered_map<QString, TrackList>& existingCueTracks() const;

    [[nodiscard]] std::set<QString>& cueFilesScanned();
    [[nodiscard]] const std::set<QString>& filesScanned() const;
    [[nodiscard]] const TrackList& scopedTracks() const;
    [[nodiscard]] bool trackWasSeen(const Track& track) const;
    [[nodiscard]] size_t filesScannedCount() const;
    [[nodiscard]] size_t progressCount() const;
    [[nodiscard]] size_t discoveredFiles() const;

    bool rememberDiscoveredPath(const QString& path);
    bool rememberScannedFile(const QString& path);

private:
    LibraryScanHost* m_host;

    std::unordered_map<QString, TrackList> m_trackPaths;
    std::unordered_map<QString, TrackList> m_existingArchives;
    std::unordered_map<QString, TrackList> m_existingCueTracks;

    std::unordered_map<QString, TrackList> m_candidatesByFilename;
    std::unordered_map<QString, TrackList> m_candidatesByHash;

    TrackList m_scopedTracks;
    QStringList m_scanRoots;

    std::unordered_map<QString, bool> m_pathExistsCache;

    std::set<QString> m_discoveredPaths;
    std::set<QString> m_filesScanned;
    std::set<QString> m_cueFilesScanned;
    std::set<QString> m_seenTracks;

    size_t m_discoveredFiles;
    size_t m_progressCount;
    size_t m_totalFiles;
    ScanProgress::Phase m_phase;
    bool m_reportEnumeratingProgress;
};
} // namespace Fooyin
