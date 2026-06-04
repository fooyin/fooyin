/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <core/engine/audioloader.h>
#include <core/track.h>

#include <QDir>
#include <QString>

class QUrl;

namespace Fooyin {
class FYCORE_EXPORT PlaylistParser
{
public:
    struct ReadPlaylistEntry
    {
        std::function<Track(const Track& filepath)> readTrack;
        std::function<bool(const Track& track)> canLoadTrack;
        bool cancel{false};
    };

    enum class PathType : uint8_t
    {
        Auto = 0,
        Absolute,
        Relative
    };

    virtual ~PlaylistParser() = default;

    [[nodiscard]] virtual QString name() const                    = 0;
    [[nodiscard]] virtual QStringList supportedExtensions() const = 0;
    [[nodiscard]] virtual bool saveIsSupported() const            = 0;
    [[nodiscard]] virtual bool canParse(const QByteArray& data, const QString& contentType, const QUrl& url) const;
    [[nodiscard]] virtual size_t countEntries(QIODevice* device, const QString& filepath, const QDir& dir) const;

    virtual TrackList readPlaylist(QIODevice* device, const QString& filepath, const QDir& dir,
                                   const ReadPlaylistEntry& readEntry, bool skipNotFound) = 0;
    virtual void savePlaylist(QIODevice* device, const QString& extension, const TrackList& tracks, const QDir& dir,
                              PathType type, bool writeMetdata);

    static QByteArray toUtf8(QIODevice* file);
    static QString determineTrackPath(const QUrl& url, const QDir& dir, PathType type);
};
} // namespace Fooyin
