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

bool pathIsWithinRoots(const QString& path, const QStringList& roots)
{
    if(roots.empty()) {
        return true;
    }

    return std::ranges::any_of(roots, [&path](const QString& root) { return pathIsWithinRoot(path, root); });
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

    for(const auto& cue : cues) {
        if(cue.completeBaseName() == file.completeBaseName() || cue.fileName().contains(file.fileName())) {
            return cue;
        }
    }

    return {};
}

void readFileProperties(Track& track)
{
    const QFileInfo fileInfo{track.filepath()};

    if(track.addedTime() == 0) {
        track.setAddedTime(QDateTime::currentMSecsSinceEpoch());
    }
    if(track.modifiedTime() == 0) {
        const QDateTime modifiedTime = fileInfo.lastModified();
        track.setModifiedTime(modifiedTime.isValid() ? modifiedTime.toMSecsSinceEpoch() : 0);
    }
    if(track.fileSize() == 0) {
        track.setFileSize(fileInfo.size());
    }
}
} // namespace Fooyin
