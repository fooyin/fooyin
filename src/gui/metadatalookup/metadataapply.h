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

#include "fygui_export.h"

#include "metadatamatcher.h"

#include <optional>

namespace Fooyin {
enum class ExistingMetadataPolicy : uint8_t
{
    FillMissing = 0,
    ReplaceLookupFields,
    WipeWritableTags,
};

struct MetadataApplyOptions
{
    ExistingMetadataPolicy policy{ExistingMetadataPolicy::ReplaceLookupFields};
    bool writeGenres{true};
    bool writeReleaseIds{true};
    bool useOriginalReleaseDate{false};
};

struct FieldChange
{
    size_t localIndex{0};
    QString field;
    QString before;
    QString after;
};

struct MetadataApplyResult
{
    TrackList tracks;
    //! Original track index corresponding to each entry in tracks
    std::vector<size_t> trackIndices;
    std::vector<FieldChange> changes;
};

FYGUI_EXPORT MetadataApplyResult applyReleaseMetadata(const TrackList& localTracks, const Release& release,
                                                      const std::vector<TrackMatch>& matches,
                                                      const MetadataApplyOptions& options);

[[nodiscard]] FYGUI_EXPORT std::optional<TrackList>
applyAutomaticDiscMetadata(const TrackList& tracks, const Release& release, const QString& discId);
} // namespace Fooyin
