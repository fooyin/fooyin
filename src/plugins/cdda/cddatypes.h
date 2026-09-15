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
 */

#pragma once

#include <QString>

#include <unordered_map>
#include <vector>

namespace Fooyin::Cdda {
constexpr auto SectorsPerSecond = 75;
constexpr auto FramesPerSector  = 588;
constexpr auto BytesPerSector   = 2352;
constexpr auto LeadInSectors    = 150;
constexpr auto MaximumTracks    = 99;

// Sector positions use logical block addresses; track 1 normally starts at zero
struct CdTocTrack
{
    int number{0};
    int firstSector{0};
    int endSectorExclusive{0};
    bool isAudio{false};

    bool operator==(const CdTocTrack&) const = default;
};

struct CdToc
{
    int firstTrackNumber{0};
    int lastTrackNumber{0};
    int leadoutSector{0};
    std::vector<CdTocTrack> tracks;

    bool operator==(const CdToc&) const = default;
};

struct CdTextFields
{
    QString title;
    QString performer;
    QString genre;
    QString composer;
    QString message;
    QString isrc;

    [[nodiscard]] bool empty() const
    {
        return title.isEmpty() && performer.isEmpty() && genre.isEmpty() && composer.isEmpty() && message.isEmpty()
            && isrc.isEmpty();
    }

    bool operator==(const CdTextFields&) const = default;
};

struct CdText
{
    CdTextFields disc;
    std::unordered_map<int, CdTextFields> tracks;

    [[nodiscard]] bool empty() const
    {
        return disc.empty() && tracks.empty();
    }

    bool operator==(const CdText&) const = default;
};
} // namespace Fooyin::Cdda
