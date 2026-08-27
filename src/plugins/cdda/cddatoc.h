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

#include "cddatypes.h"

#include <QString>

#include <expected>
#include <optional>
#include <vector>

namespace Fooyin::Cdda {
enum class CdTocError : uint8_t
{
    InvalidTrackNumberRange = 0,
    MissingTracks,
    InvalidLeadout,
    LeadoutTooLarge,
    InvalidTrackNumbers,
    InvalidTrackSectorRange,
    NoncontiguousTrackSectorRanges,
    TrackBeyondLeadout,
    FinalTrackBeforeLeadout,
    NoAudioTracks,
};

[[nodiscard]] QString invalidTocUserMessage();
[[nodiscard]] std::expected<void, CdTocError> validateToc(const CdToc& toc);
[[nodiscard]] std::vector<CdTocTrack> audioTracks(const CdToc& toc);
[[nodiscard]] std::optional<CdTocTrack> audioTrackForSubsong(const CdToc& toc, int subsong);

[[nodiscard]] std::expected<uint64_t, QString> framesForSectors(uint64_t sectors);
[[nodiscard]] std::expected<uint64_t, QString> bytesForSectors(uint64_t sectors);
[[nodiscard]] std::expected<uint64_t, QString> durationForSectors(uint64_t sectors);

//! Calculates the case-sensitive MusicBrainz Disc ID from a TOC.
[[nodiscard]] std::expected<QString, CdTocError> musicBrainzDiscId(const CdToc& toc);
//! Returns the MusicBrainz TOC query value using absolute frame offsets.
[[nodiscard]] std::expected<QString, CdTocError> musicBrainzToc(const CdToc& toc);
} // namespace Fooyin::Cdda
