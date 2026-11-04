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

#include <core/track.h>

#include <QFileInfo>

#include <optional>

namespace Fooyin {
struct FYCORE_EXPORT TrackReloadOptions
{
    bool overwriteRatingOnReload{false};
    bool overwritePlaycountOnReload{false};
};

FYCORE_EXPORT QString normalisePath(const QString& path);
FYCORE_EXPORT QStringList normalisePaths(const QStringList& paths);
FYCORE_EXPORT QStringList normaliseExtensions(const QStringList& extensions);
FYCORE_EXPORT QString trackIdentity(const Track& track);
FYCORE_EXPORT QString physicalTrackPath(const Track& track);
FYCORE_EXPORT bool trackIsInRoots(const Track& track, const QStringList& roots);
FYCORE_EXPORT std::optional<QFileInfo> findMatchingCue(const QFileInfo& file, const QFileInfoList& cueFiles);
FYCORE_EXPORT std::optional<QFileInfo> findMatchingCue(const QFileInfo& file);
FYCORE_EXPORT void readFileProperties(Track& track);
FYCORE_EXPORT void mergeReloadedTrack(Track& track, const Track& existingTrack, const TrackReloadOptions& options);
FYCORE_EXPORT void mergeReloadedTrackStats(Track& track, const Track& existingTrack, const TrackReloadOptions& options);
} // namespace Fooyin
