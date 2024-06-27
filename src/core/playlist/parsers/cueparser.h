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

#include "fycore_export.h"

#include <core/playlist/playlistparser.h>
#include <core/tagging/tagparser.h>
#include <core/track.h>

struct CueSheet;

namespace Fooyin {
class FYCORE_EXPORT CueParser : public PlaylistParser
{
public:
    using PlaylistParser::PlaylistParser;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QStringList supportedExtensions() const override;
    [[nodiscard]] bool saveIsSupported() const override;

    TrackList readPlaylist(QIODevice* device, const QString& filepath, const QDir& dir, bool skipNotFound) override;

private:
    TrackList readCueTracks(QIODevice* device, const QString& filepath, const QDir& dir, bool skipNotFound);
    TrackList readEmbeddedCueTracks(QIODevice* device, const QString& filepath);
    void processCueLine(CueSheet& sheet, const QString& line, Fooyin::Track& track, QString& trackPath, const QDir& dir,
                        bool skipNotFound, bool skipFile, Fooyin::TrackList& tracks);
};
} // namespace Fooyin
