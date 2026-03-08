/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <set>

class QString;
class QWidget;

namespace Fooyin {
class AudioLoader;
struct ArtworkResult;

struct ArtworkExportSummary
{
    int written{0};
    int failed{0};
};

class ArtworkExporter
{
public:
    [[nodiscard]] static ArtworkExportSummary extractTracks(AudioLoader* loader, const TrackList& tracks,
                                                            const std::set<Track::Cover>& types);
    [[nodiscard]] static ArtworkExportSummary extractTracks(const TrackList& tracks, Track::Cover type,
                                                            const ArtworkResult& artwork);
    [[nodiscard]] static QString extractTrackAs(AudioLoader* loader, const Track& track, Track::Cover type,
                                                QWidget* parent = nullptr);
    [[nodiscard]] static QString extractArtworkAs(const Track& track, Track::Cover type, const ArtworkResult& artwork,
                                                  QWidget* parent = nullptr);
    [[nodiscard]] static QString statusMessage(const ArtworkExportSummary& summary);
};
} // namespace Fooyin
