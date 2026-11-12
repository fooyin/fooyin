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

#include <QString>

#include <cstdint>
#include <optional>
#include <vector>

namespace Fooyin {
struct FYCORE_EXPORT LibraryGroupStatistics
{
    QString name;
    uint64_t trackCount{0};
    uint64_t totalDurationMs{0};
    uint64_t totalFileSize{0};
    uint64_t totalPlayCount{0};

    bool operator==(const LibraryGroupStatistics&) const = default;
};

struct FYCORE_EXPORT LibraryStatistics
{
    uint64_t trackCount{0};
    uint64_t totalDurationMs{0};
    uint64_t totalFileSize{0};
    uint64_t totalPlayCount{0};
    std::optional<double> averageRating;
    uint64_t unratedTrackCount{0};
    std::vector<LibraryGroupStatistics> formats;
    std::vector<LibraryGroupStatistics> bitDepths;
    std::vector<LibraryGroupStatistics> sampleRates;
    std::vector<LibraryGroupStatistics> folders;
    std::vector<LibraryGroupStatistics> ratings;

    bool operator==(const LibraryStatistics&) const = default;
};
} // namespace Fooyin
