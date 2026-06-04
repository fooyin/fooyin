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

#include "m3uparser.h"

#include "cueparser.h"

#include <core/track.h>

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QTextStream>

Q_LOGGING_CATEGORY(M3U, "fy.m3u")

using namespace Qt::StringLiterals;

constexpr auto HlsProbeBytes = 32 * 1024;

namespace Fooyin {
namespace {
enum class Type
{
    Standard,
    Extended,
    Dir // TODO
};

struct Metadata
{
    QString title;
    QString artist;
    uint64_t duration{0};
};

bool processMetadata(const QString& line, Metadata& metadata)
{
    const QString info = line.section(u':', 1);
    const QString dur  = info.section(u',', 0, 0);

    bool isNum{false};
    const int duration = dur.toInt(&isNum);
    if(!isNum) {
        return false;
    }

    metadata.duration = static_cast<uint64_t>(duration) * 1000;

    const QString trackSection = info.section(u',', 1);
    const QStringList list     = trackSection.split(" - "_L1);

    if(list.size() <= 1) {
        metadata.title = trackSection;
        return true;
    }

    metadata.artist = list[0].trimmed();
    metadata.title  = list[1].trimmed();

    return true;
}

bool looksLikeHlsPlaylist(const QByteArray& data)
{
    const QByteArray probe = data.left(HlsProbeBytes).toUpper();
    return probe.startsWith("#EXTM3U") && probe.contains("#EXT-X-");
}

int endingSubsong(QString* filepath)
{
    static const QRegularExpression regex{uR"(#(\d+)$)"_s};
    const QRegularExpressionMatch match = regex.match(*filepath);
    if(match.hasMatch()) {
        const int subsong = match.captured(1).toInt();
        filepath->remove(match.capturedStart(), match.capturedLength());
        return subsong;
    }
    return -1;
}

bool isCuePath(const QString& path)
{
    return QFileInfo{path}.suffix().compare(u"cue"_s, Qt::CaseInsensitive) == 0;
}

QString resolvePlaylistEntryPath(const QString& playlistPath, const QString& entry, const QDir& dir)
{
    if(Track::isArchivePath(entry) || Track::isRemotePath(entry)) {
        return entry;
    }

    const QUrl playlistUrl{playlistPath};

    if(Track::isRemotePath(playlistUrl.toString())) {
        const QUrl resolved = playlistUrl.resolved(QUrl{entry});
        if(resolved.isValid() && !resolved.scheme().isEmpty()) {
            return resolved.toString();
        }
    }

    if(dir.exists() && !QDir::isAbsolutePath(entry)) {
        return QDir::cleanPath(dir.absoluteFilePath(entry));
    }

    return entry;
}

TrackList readCuePlaylist(const QString& path, const PlaylistParser::ReadPlaylistEntry& readEntry, bool skipNotFound)
{
    QFile cueFile{path};
    if(!cueFile.open(QIODevice::ReadOnly)) {
        return {};
    }

    QDir cueDir{path};
    cueDir.cdUp();

    CueParser parser;
    return parser.readPlaylist(&cueFile, path, cueDir, readEntry, skipNotFound);
}

TrackList readEmbeddedCueTracks(const Track& track, const PlaylistParser::ReadPlaylistEntry& readEntry)
{
    const auto cueSheet = track.extraTag(u"CUESHEET"_s);
    if(cueSheet.empty()) {
        return {};
    }

    QByteArray bytes{cueSheet.front().toUtf8()};
    QBuffer buffer{&bytes};
    if(!buffer.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    CueParser parser;
    return parser.readPlaylist(&buffer, track.filepath(), {}, readEntry, false);
}

QString cueExportPath(const Track& track)
{
    return track.hasEmbeddedCue() ? track.filepath() : track.cuePath();
}
} // namespace

QString M3uParser::name() const
{
    return u"M3U"_s;
}

QStringList M3uParser::supportedExtensions() const
{
    static const QStringList extensions{u"m3u"_s, u"m3u8"_s};
    return extensions;
}

bool M3uParser::saveIsSupported() const
{
    return true;
}

bool M3uParser::canParse(const QByteArray& data, const QString& contentType, const QUrl& url) const
{
    if(looksLikeHlsPlaylist(data)) {
        return false;
    }

    const QString type = contentType.toLower();
    if(type.contains(u"mpegurl"_s) || type.contains(u"application/vnd.apple.mpegurl"_s)) {
        return true;
    }

    const QByteArray trimmed = data.left(HlsProbeBytes).trimmed();
    if(trimmed.startsWith("#EXTM3U")) {
        return true;
    }

    return PlaylistParser::canParse(data, contentType, url);
}

size_t M3uParser::countEntries(QIODevice* device, const QString& /*filepath*/, const QDir& /*dir*/) const
{
    QByteArray m3u = toUtf8(device);
    if(looksLikeHlsPlaylist(m3u)) {
        return 0;
    }

    QBuffer buffer{&m3u};
    if(!buffer.open(QIODevice::ReadOnly)) {
        return 0;
    }

    size_t entries{0};

    while(!buffer.atEnd()) {
        const QString line = QString::fromUtf8(buffer.readLine()).trimmed();
        if(!line.isEmpty() && !line.startsWith(u'#')) {
            ++entries;
        }
    }

    return entries;
}

TrackList M3uParser::readPlaylist(QIODevice* device, const QString& filepath, const QDir& dir,
                                  const ReadPlaylistEntry& readEntry, bool skipNotFound)
{
    Type type{Type::Standard};
    Metadata metadata;

    QByteArray m3u = toUtf8(device);
    if(looksLikeHlsPlaylist(m3u)) {
        return {};
    }

    QBuffer buffer{&m3u};
    if(!buffer.open(QIODevice::ReadOnly)) {
        return {};
    }

    TrackList tracks;

    while(!buffer.atEnd() && !readEntry.cancel) {
        const QString line = QString::fromUtf8(buffer.readLine()).trimmed();

        if(line.startsWith("#EXTM3U"_L1)) {
            type = Type::Extended;
            continue;
        }

        if(line.startsWith(u'#')) {
            if(type == Type::Extended && line.startsWith("#EXT"_L1)) {
                if(!processMetadata(line, metadata)) {
                    qCWarning(M3U) << "Failed to process metadata:" << line;
                }
            }
        }
        else if(!line.isEmpty()) {
            QString path{resolvePlaylistEntryPath(filepath, line, dir)};

            const int subsong = endingSubsong(&path);
            if(isCuePath(path)) {
                const auto cueTracks = readCuePlaylist(path, readEntry, skipNotFound);
                tracks.insert(tracks.end(), cueTracks.cbegin(), cueTracks.cend());
                metadata = {};
                continue;
            }

            Track track{path};

            if(subsong > 0) {
                track.setSubsong(subsong);
            }

            if(!Track::isArchivePath(path) && !Track::isRemotePath(path) && !QFile::exists(path)) {
                // Handle potential windows filepath
                track.setFilePath(path.replace(u'\\', u'/'));
            }

            track = readEntry.readTrack(track);
            if(track.hasExtraTag(u"CUESHEET"_s)) {
                if(const auto cueTracks = readEmbeddedCueTracks(track, readEntry); !cueTracks.empty()) {
                    tracks.insert(tracks.end(), cueTracks.cbegin(), cueTracks.cend());
                    metadata = {};
                    continue;
                }
            }

            if(track.isValid() || !skipNotFound) {
                if(track.title().isEmpty() && !metadata.title.isEmpty()) {
                    track.setTitle(metadata.title);
                }
                if(!track.hasArtists() && !metadata.artist.isEmpty()) {
                    track.setArtists({metadata.artist});
                }
                tracks.push_back(track);
            }

            metadata = {};
        }
    }

    buffer.close();

    return tracks;
}

void M3uParser::savePlaylist(QIODevice* device, const QString& extension, const TrackList& tracks, const QDir& dir,
                             PathType type, bool writeMetdata)
{
    QTextStream stream{device};
    if(extension == "m3u"_L1) {
        stream.setEncoding(QStringConverter::System);
    }

    if(writeMetdata) {
        stream << "#EXTM3U\n";
    }

    QString previousCueExportPath;

    for(const Track& track : tracks) {
        if(track.hasCue()) {
            const QString exportPath = cueExportPath(track);
            if(exportPath == previousCueExportPath) {
                continue;
            }

            previousCueExportPath = exportPath;
            stream << PlaylistParser::determineTrackPath(QUrl::fromLocalFile(exportPath), dir, type) << "\n";
            continue;
        }

        previousCueExportPath.clear();

        if(writeMetdata) {
            stream << u"#EXTINF:%1,%2 - %3\n"_s.arg(track.duration() / 1000).arg(track.artist(), track.title());
        }
        QString path = track.filepath();
        if(track.subsong() > 0) {
            path += u"#%1"_s.arg(track.subsong());
        }
        stream << PlaylistParser::determineTrackPath(QUrl::fromLocalFile(path), dir, type) << "\n";
    }
}
} // namespace Fooyin
