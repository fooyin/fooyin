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

#include "libraryscanstate.h"

#include "libraryscanutils.h"

#include <ranges>

namespace Fooyin {
LibraryScanState::LibraryScanState(LibraryScanHost* host)
    : m_host{host}
    , m_discoveredFiles{0}
    , m_progressCount{0}
    , m_totalFiles{0}
    , m_phase{ScanProgress::Phase::ReadingMetadata}
    , m_reportEnumeratingProgress{true}
{ }

void LibraryScanState::reset()
{
    m_trackPaths.clear();
    m_existingArchives.clear();
    m_existingCueTracks.clear();
    m_candidatesByFilename.clear();
    m_candidatesByHash.clear();
    m_scopedTracks.clear();
    m_scanRoots.clear();
    m_pathExistsCache.clear();
    m_discoveredPaths.clear();
    m_filesScanned.clear();
    m_cueFilesScanned.clear();
    m_seenTracks.clear();

    m_discoveredFiles = 0;
    m_progressCount   = 0;
    m_totalFiles      = 0;
    m_phase           = ScanProgress::Phase::ReadingMetadata;
}

bool LibraryScanState::mayRun() const
{
    return !stopRequested();
}

bool LibraryScanState::stopRequested() const
{
    return m_host->stopRequested();
}

void LibraryScanState::setProgressPhase(const ScanProgress::Phase phase, const size_t total)
{
    m_phase      = phase;
    m_totalFiles = total;
}

void LibraryScanState::setReportEnumeratingProgress(const bool report)
{
    m_reportEnumeratingProgress = report;
}

void LibraryScanState::reportProgress(const QString& file) const
{
    const size_t total = m_totalFiles > 0 ? m_totalFiles : m_discoveredFiles;
    m_host->reportProgress(static_cast<int>(m_progressCount), file, static_cast<int>(total), static_cast<int>(m_phase),
                           static_cast<int>(m_discoveredFiles));
}

void LibraryScanState::fileDiscovered(const QString& file)
{
    ++m_discoveredFiles;

    if(!m_reportEnumeratingProgress) {
        return;
    }

    setProgressPhase(ScanProgress::Phase::Enumerating, 0);
    reportProgress(file);
}

void LibraryScanState::fileScanned(const QString& file)
{
    if(rememberScannedFile(file)) {
        ++m_progressCount;
        reportProgress(file);
    }
}

void LibraryScanState::progressScanned(const QString& file)
{
    ++m_progressCount;
    reportProgress(file);
}

bool LibraryScanState::pathExists(const QString& path)
{
    const QString normalisedPath = normalisePath(path);
    if(normalisedPath.isEmpty()) {
        return false;
    }

    if(m_pathExistsCache.contains(normalisedPath)) {
        return m_pathExistsCache.at(normalisedPath);
    }

    const bool exists = QFileInfo::exists(normalisedPath);
    m_pathExistsCache.emplace(normalisedPath, exists);
    return exists;
}

void LibraryScanState::populateExistingTracks(const TrackList& tracks, const QStringList& roots,
                                              const bool scopeMissing)
{
    m_scanRoots = normalisePaths(roots);

    for(const auto& track : tracks) {
        m_trackPaths[normalisePath(track.filepath())].push_back(track);

        if(track.isInArchive()) {
            m_existingArchives[normalisePath(track.archivePath())].push_back(track);
        }

        if(track.hasCue()) {
            const QString cuePath
                = track.hasEmbeddedCue() ? normalisePath(track.filepath()) : normalisePath(track.cuePath());
            m_existingCueTracks[cuePath].push_back(track);
        }

        if(scopeMissing && isScopedTrack(track)) {
            m_scopedTracks.push_back(track);
            addRelocationCandidate(track);
        }
    }
}

bool LibraryScanState::isScopedTrack(const Track& track) const
{
    return trackIsInRoots(track, m_scanRoots);
}

void LibraryScanState::addRelocationCandidate(const Track& track)
{
    if(!isScopedTrack(track)) {
        return;
    }

    m_candidatesByFilename[track.filename()].push_back(track);
    m_candidatesByHash[track.hash()].push_back(track);
}

void LibraryScanState::removeRelocationCandidate(const Track& track)
{
    auto eraseTrack = [&track](TrackList& tracks) {
        std::erase_if(tracks, [&track](const Track& existingTrack) {
            return trackIdentity(existingTrack) == trackIdentity(track);
        });
    };

    if(m_candidatesByFilename.contains(track.filename())) {
        auto& tracks = m_candidatesByFilename.at(track.filename());
        eraseTrack(tracks);
        if(tracks.empty()) {
            m_candidatesByFilename.erase(track.filename());
        }
    }

    if(m_candidatesByHash.contains(track.hash())) {
        auto& tracks = m_candidatesByHash.at(track.hash());
        eraseTrack(tracks);
        if(tracks.empty()) {
            m_candidatesByHash.erase(track.hash());
        }
    }
}

void LibraryScanState::markTrackSeen(const Track& track)
{
    const QString key = trackIdentity(track);
    if(m_seenTracks.emplace(key).second) {
        removeRelocationCandidate(track);
    }
}

void LibraryScanState::markTracksSeen(const TrackList& tracks)
{
    for(const auto& track : tracks) {
        markTrackSeen(track);
    }
}

Track LibraryScanState::matchRelocatedTrack(const Track& track)
{
    auto findCandidate = [this, &track](const TrackList& candidates) -> Track {
        const QString newPath = physicalTrackPath(track);

        for(const auto& candidate : candidates) {
            if(candidate.duration() != track.duration() || candidate.hash() != track.hash()) {
                continue;
            }

            if(m_seenTracks.contains(trackIdentity(candidate))) {
                continue;
            }

            const QString candidatePath = physicalTrackPath(candidate);
            if(candidatePath == newPath || pathExists(candidatePath)) {
                continue;
            }

            return candidate;
        }

        return {};
    };

    if(m_candidatesByFilename.contains(track.filename())) {
        if(const Track candidate = findCandidate(m_candidatesByFilename.at(track.filename()));
           candidate.isInDatabase() || candidate.isInLibrary()) {
            return candidate;
        }
    }

    if(m_candidatesByHash.contains(track.hash())) {
        return findCandidate(m_candidatesByHash.at(track.hash()));
    }

    return {};
}

std::optional<Track> LibraryScanState::findExistingTrackByUniqueFilepath(const Track& track) const
{
    const QString fileKey = normalisePath(track.filepath());
    if(!m_trackPaths.contains(fileKey)) {
        return {};
    }

    const auto& existingTracks = m_trackPaths.at(fileKey);
    const auto existingTrack   = std::ranges::find_if(existingTracks, [&track](const Track& existing) {
        return existing.uniqueFilepath() == track.uniqueFilepath();
    });

    if(existingTrack != existingTracks.cend()) {
        return *existingTrack;
    }

    return {};
}

const std::unordered_map<QString, TrackList>& LibraryScanState::trackPaths() const
{
    return m_trackPaths;
}

const std::unordered_map<QString, TrackList>& LibraryScanState::existingArchives() const
{
    return m_existingArchives;
}

const std::unordered_map<QString, TrackList>& LibraryScanState::existingCueTracks() const
{
    return m_existingCueTracks;
}

std::set<QString>& LibraryScanState::cueFilesScanned()
{
    return m_cueFilesScanned;
}

const std::set<QString>& LibraryScanState::filesScanned() const
{
    return m_filesScanned;
}

const TrackList& LibraryScanState::scopedTracks() const
{
    return m_scopedTracks;
}

bool LibraryScanState::trackWasSeen(const Track& track) const
{
    return m_seenTracks.contains(trackIdentity(track));
}

size_t LibraryScanState::filesScannedCount() const
{
    return m_filesScanned.size();
}

size_t LibraryScanState::progressCount() const
{
    return m_progressCount;
}

size_t LibraryScanState::discoveredFiles() const
{
    return m_discoveredFiles;
}

bool LibraryScanState::rememberDiscoveredPath(const QString& path)
{
    return m_discoveredPaths.emplace(path).second;
}

bool LibraryScanState::rememberScannedFile(const QString& path)
{
    return m_filesScanned.emplace(path).second;
}
} // namespace Fooyin
