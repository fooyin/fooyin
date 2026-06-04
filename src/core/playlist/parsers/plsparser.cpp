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

#include "plsparser.h"

#include <core/track.h>

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QUrl>

#include <algorithm>
#include <map>
#include <ranges>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
struct PlsEntry
{
    QString file;
    QString title;
    int durationSeconds{-1};
};

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

std::map<int, PlsEntry> parseEntries(QIODevice* device)
{
    QByteArray data = PlaylistParser::toUtf8(device);
    QBuffer buffer{&data};
    if(!buffer.open(QIODevice::ReadOnly)) {
        return {};
    }

    std::map<int, PlsEntry> entries;

    while(!buffer.atEnd()) {
        const QString line = QString::fromUtf8(buffer.readLine()).trimmed();
        if(line.isEmpty() || line.startsWith(u'[')) {
            continue;
        }

        const qsizetype separator = line.indexOf(u'=');
        if(separator <= 0) {
            continue;
        }

        const QString key   = line.left(separator).trimmed();
        const QString value = line.mid(separator + 1).trimmed();
        if(value.isEmpty()) {
            continue;
        }

        qsizetype digitStart = key.size();
        while(digitStart > 0 && key.at(digitStart - 1).isDigit()) {
            --digitStart;
        }
        if(digitStart == key.size()) {
            continue;
        }

        bool ok{false};
        const int index = key.mid(digitStart).toInt(&ok);
        if(!ok || index <= 0) {
            continue;
        }

        PlsEntry& entry = entries[index];

        const QString field = key.left(digitStart).toLower();
        if(field == "file"_L1) {
            entry.file = value;
        }
        else if(field == "title"_L1) {
            entry.title = value;
        }
        else if(field == "length"_L1) {
            entry.durationSeconds = value.toInt();
        }
    }

    return entries;
}
} // namespace

QString PlsParser::name() const
{
    return u"PLS"_s;
}

QStringList PlsParser::supportedExtensions() const
{
    static const QStringList extensions{u"pls"_s};
    return extensions;
}

bool PlsParser::saveIsSupported() const
{
    return true;
}

bool PlsParser::canParse(const QByteArray& data, const QString& contentType, const QUrl& url) const
{
    const QString type = contentType.toLower();
    if(type.contains(u"scpls"_s)) {
        return true;
    }

    const QByteArray lower = data.left(32 * 1024).trimmed().toLower();
    if(lower.startsWith("[playlist]") || lower.startsWith("file1=") || lower.contains("\nfile1=")
       || lower.contains("\r\nfile1=")) {
        return true;
    }

    return PlaylistParser::canParse(data, contentType, url);
}

size_t PlsParser::countEntries(QIODevice* device, const QString& /*filepath*/, const QDir& /*dir*/) const
{
    const auto entries = parseEntries(device);
    return static_cast<size_t>(
        std::ranges::count_if(entries, [](const auto& entry) { return !entry.second.file.isEmpty(); }));
}

TrackList PlsParser::readPlaylist(QIODevice* device, const QString& filepath, const QDir& dir,
                                  const ReadPlaylistEntry& readEntry, bool skipNotFound)
{
    TrackList tracks;
    const auto entries = parseEntries(device);

    for(const auto& entry : entries | std::views::values) {
        if(readEntry.cancel) {
            break;
        }
        if(entry.file.isEmpty()) {
            continue;
        }

        QString path = resolvePlaylistEntryPath(filepath, entry.file, dir);
        Track track{path};

        if(!Track::isArchivePath(path) && !Track::isRemotePath(path) && !QFile::exists(path)) {
            track.setFilePath(path.replace(u'\\', u'/'));
        }

        track = readEntry.readTrack(track);

        if(track.isValid() || !skipNotFound) {
            if(track.title().isEmpty() && !entry.title.isEmpty()) {
                track.setTitle(entry.title);
            }
            if(track.duration() == 0 && entry.durationSeconds > 0) {
                track.setDuration(static_cast<uint64_t>(entry.durationSeconds) * 1000);
            }
            tracks.emplace_back(track);
        }
    }

    return tracks;
}

void PlsParser::savePlaylist(QIODevice* device, const QString& /*extension*/, const TrackList& tracks, const QDir& dir,
                             PathType type, bool writeMetdata)
{
    QTextStream stream{device};
    stream << "[playlist]\n";

    const auto count = tracks.size();

    for(size_t i{0}; i < count; ++i) {
        const Track& track      = tracks.at(i);
        const size_t entryIndex = i + 1;
        const QUrl trackUrl     = track.isRemote() ? QUrl{track.filepath()} : QUrl::fromLocalFile(track.filepath());

        stream << "File" << entryIndex << "=" << PlaylistParser::determineTrackPath(trackUrl, dir, type) << "\n";

        if(writeMetdata) {
            const QString title = track.title();
            if(!title.isEmpty()) {
                stream << "Title" << entryIndex << "=" << title << "\n";
            }
            if(track.duration() > 0) {
                stream << "Length" << entryIndex << "=" << (track.duration() / 1000) << "\n";
            }
        }
    }

    stream << "NumberOfEntries=" << count << "\n";
    stream << "Version=2\n";
}
} // namespace Fooyin
