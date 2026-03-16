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

#include <core/track.h>

#include <QFileInfo>

#include <optional>

namespace Fooyin {
QString normalisePath(const QString& path);
QStringList normalisePaths(const QStringList& paths);
QStringList normaliseExtensions(const QStringList& extensions);
QString trackIdentity(const Track& track);
QString physicalTrackPath(const Track& track);
bool trackIsInRoots(const Track& track, const QStringList& roots);
std::optional<QFileInfo> findMatchingCue(const QFileInfo& file);
void readFileProperties(Track& track);
} // namespace Fooyin
