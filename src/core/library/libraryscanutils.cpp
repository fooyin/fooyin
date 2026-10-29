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

#include "libraryscanutils.h"

#include <QDir>

#include <ranges>

using namespace Qt::StringLiterals;

namespace {
bool pathIsWithinRoot(const QString& path, const QString& root)
{
    return path == root || path.startsWith(root + u"/"_s);
}

int cueMatchRank(const QFileInfo& file, const QFileInfo& cue)
{
    const QString fileName = file.fileName();
    const QString fileBase = file.completeBaseName();
    const QString cueName  = cue.fileName();
    const QString cueBase  = cue.completeBaseName();

    if(cueName.compare(fileName + u".cue"_s, Qt::CaseInsensitive) == 0
       || cueBase.compare(fileName, Qt::CaseInsensitive) == 0) {
        return 0;
    }
    if(cueBase.compare(fileBase, Qt::CaseInsensitive) == 0) {
        return 1;
    }
    if(cueName.contains(fileName, Qt::CaseInsensitive)) {
        return 2;
    }
    if(cueBase.contains(fileBase, Qt::CaseInsensitive)) {
        return 3;
    }

    return -1;
}

bool pathIsWithinRoots(const QString& path, const QStringList& roots)
{
    if(roots.empty()) {
        return true;
    }

    return std::ranges::any_of(roots, [&path](const QString& root) { return pathIsWithinRoot(path, root); });
}

uint64_t minNonZero(const uint64_t lhs, const uint64_t rhs)
{
    if(lhs == 0) {
        return rhs;
    }
    if(rhs == 0) {
        return lhs;
    }
    return std::min(lhs, rhs);
}
} // namespace

namespace Fooyin {
QString normalisePath(const QString& path)
{
    if(path.isEmpty()) {
        return {};
    }

    if(Track::isArchivePath(path)) {
        return path;
    }

    const QFileInfo info{path};
    return QDir::cleanPath(info.absoluteFilePath());
}

QStringList normalisePaths(const QStringList& paths)
{
    QStringList normalised;
    normalised.reserve(paths.size());

    for(const auto& path : paths) {
        const QString normalisedPath = normalisePath(path);
        if(!normalisedPath.isEmpty()) {
            normalised.push_back(normalisedPath);
        }
    }

    return normalised;
}

QStringList normaliseExtensions(const QStringList& extensions)
{
    QStringList normalised;
    normalised.reserve(extensions.size());

    for(const auto& extension : extensions) {
        const QString value = extension.trimmed().toLower();
        if(!value.isEmpty()) {
            normalised.push_back(value);
        }
    }

    return normalised;
}

QString trackIdentity(const Track& track)
{
    if(track.id() >= 0) {
        return QString::number(track.id());
    }

    return track.uniqueFilepath() + u'|' + track.hash() + u'|' + QString::number(track.duration());
}

QString physicalTrackPath(const Track& track)
{
    return track.isInArchive() ? normalisePath(track.archivePath()) : normalisePath(track.filepath());
}

bool trackIsInRoots(const Track& track, const QStringList& roots)
{
    if(roots.empty()) {
        return true;
    }

    if(pathIsWithinRoots(physicalTrackPath(track), roots)) {
        return true;
    }

    if(track.hasCue() && !track.hasEmbeddedCue()) {
        return pathIsWithinRoots(normalisePath(track.cuePath()), roots);
    }

    return false;
}

std::optional<QFileInfo> findMatchingCue(const QFileInfo& file)
{
    static const QStringList cueExtensions{u"*.cue"_s};

    const QDir dir           = file.absoluteDir();
    const QFileInfoList cues = dir.entryInfoList(cueExtensions, QDir::Files, QDir::Name | QDir::IgnoreCase);

    return findMatchingCue(file, cues);
}

std::optional<QFileInfo> findMatchingCue(const QFileInfo& file, const QFileInfoList& cueFiles)
{
    std::optional<QFileInfo> bestCue;
    int bestRank{std::numeric_limits<int>::max()};

    for(const auto& cue : cueFiles) {
        const int rank = cueMatchRank(file, cue);
        if(rank < 0 || rank >= bestRank) {
            continue;
        }

        bestCue  = cue;
        bestRank = rank;
    }

    return bestCue;
}

void readFileProperties(Track& track)
{
    const QFileInfo fileInfo{physicalTrackPath(track)};

    if(track.addedTime() == 0) {
        track.setAddedTime(QDateTime::currentMSecsSinceEpoch());
    }
    if(track.createdTime() == 0) {
        const QDateTime createdTime = fileInfo.birthTime();
        track.setCreatedTime(createdTime.isValid() ? createdTime.toMSecsSinceEpoch() : 0);
    }
    if(track.modifiedTime() == 0) {
        const QDateTime modifiedTime = fileInfo.lastModified();
        track.setModifiedTime(modifiedTime.isValid() ? modifiedTime.toMSecsSinceEpoch() : 0);
    }
    if(track.fileSize() == 0) {
        track.setFileSize(fileInfo.size());
    }
}

void mergeReloadedTrackStats(Track& track, const Track& existingTrack, const TrackReloadOptions& options)
{
    track.setLoved(existingTrack.isLoved());

    const bool fileHasRating    = track.rating() > 0;
    const bool fileHasPlayStats = track.playCount() > 0 || track.firstPlayed() > 0 || track.lastPlayed() > 0;

    if(options.overwriteRatingOnReload) {
        if(!fileHasRating && existingTrack.rating() > 0) {
            track.setRating(existingTrack.rating());
        }
    }
    else if(existingTrack.rating() > 0 || !fileHasRating) {
        track.setRating(existingTrack.rating());
    }

    if(options.overwritePlaycountOnReload) {
        if(!fileHasPlayStats) {
            track.setPlayCount(existingTrack.playCount());
            track.setFirstPlayed(existingTrack.firstPlayed());
            track.setLastPlayed(existingTrack.lastPlayed());
            return;
        }

        if(track.playCount() <= 0) {
            track.setPlayCount(existingTrack.playCount());
        }
        if(track.firstPlayed() == 0) {
            track.setFirstPlayed(existingTrack.firstPlayed());
        }
        if(track.lastPlayed() == 0) {
            track.setLastPlayed(existingTrack.lastPlayed());
        }
        return;
    }

    track.setPlayCount(std::max(existingTrack.playCount(), track.playCount()));
    track.setFirstPlayed(minNonZero(existingTrack.firstPlayed(), track.firstPlayed()));
    track.setLastPlayed(std::max(existingTrack.lastPlayed(), track.lastPlayed()));
}
} // namespace Fooyin
