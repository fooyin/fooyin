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
#include <core/track.h>

namespace Fooyin {
struct CueSheet
{
    QString cuePath;
    QString type;
    QString albumArtist;
    QString album;
    QString composer;
    QString genre;
    QString date;
    QString comment;
    int disc{-1};
    uint64_t lastModified{0};
};

class FYCORE_EXPORT CueParser : public PlaylistParser
{
public:
    CueParser();

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QStringList supportedExtensions() const override;
    [[nodiscard]] bool saveIsSupported() const override;

    TrackList readPlaylist(QIODevice* device, const QString& filepath, const QDir& dir, bool skipNotFound) override;

private:
    TrackList readCueTracks(QIODevice* device, const QString& filepath, const QDir& dir, bool skipNotFound);
    TrackList readEmbeddedCueTracks(QIODevice* device, const QString& filepath);
    void processCueLine(const QString& line, Track& track, QString& trackPath, const QDir& dir, bool skipNotFound,
                        bool skipFile, TrackList& tracks);
    void readRemLine(const QStringList& lineParts);
    void finaliseTrack(Track& track);
    void finaliseLastTrack(Track& track, const QString& trackPath, bool skipNotFound, TrackList& tracks);

    CueSheet m_sheet;
    bool m_hasValidIndex;
    Track m_currentFile;
};
} // namespace Fooyin
