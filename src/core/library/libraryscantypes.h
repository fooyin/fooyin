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

#include <core/library/libraryinfo.h>
#include <core/track.h>

#include <stop_token>

namespace Fooyin {
struct ScanResult;

class FYCORE_EXPORT LibraryScanHost
{
public:
    virtual ~LibraryScanHost() = default;

    [[nodiscard]] virtual bool stopRequested() const                                                    = 0;
    virtual void reportProgress(int current, const QString& file, int total, int phase, int discovered) = 0;
    virtual void reportScanUpdate(const ScanResult& result)                                             = 0;
};

struct FYCORE_EXPORT LibraryScanConfig
{
    QStringList libraryRestrictExt;
    QStringList libraryExcludeExt;
    QStringList externalRestrictExt;
    QStringList externalExcludeExt;
    bool playlistSkipMissing{true};
    bool addFoldersIgnorePlaylists{false};
    bool overwriteRatingOnReload{false};
    bool overwritePlaycountOnReload{false};
    std::stop_token readCancellationToken{};
};

struct FYCORE_EXPORT LibraryScanFilesResult
{
    TrackList tracksScanned;
    TrackList playlistTracksScanned;
};

enum class EnumeratedFileType : uint8_t
{
    Track = 0,
    Playlist,
    Cue,
};
} // namespace Fooyin
