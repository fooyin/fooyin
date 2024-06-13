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

#include "m3uparser.h"

#include "tagging/tagreader.h"
#include "utils/utils.h"

#include <core/track.h>

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QRegularExpression>

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
    const QStringList list     = trackSection.split(QStringLiteral(" - "));

    if(list.size() <= 1) {
        metadata.title = trackSection;
        return true;
    }

    metadata.artist = list[0].trimmed();
    metadata.title  = list[1].trimmed();

    return true;
}
} // namespace

namespace Fooyin {
TrackList M3uParser::readPlaylist(const QString& file, bool skipNotFound)
{
    if(file.isEmpty()) {
        return {};
    }

    Type type{Type::Standard};
    Metadata metadata;

    QFile m3uFile{file};

    if(!m3uFile.open(QIODevice::ReadOnly)) {
        qWarning() << QStringLiteral("Could not open m3u file %1 for reading: %2")
                          .arg(m3uFile.fileName(), m3uFile.errorString());
        return {};
    }

    QString m3u = QString::fromUtf8(m3uFile.readAll());
    m3u.replace(QLatin1String{"\n\n"}, QLatin1String{"\n"});
    m3u.replace(u'\r', u'\n');

    QByteArray m3uBytes{m3u.toUtf8()};
    QBuffer buffer{&m3uBytes};
    if(!buffer.open(QIODevice::ReadOnly)) {
        return {};
    }

    QString line = QString::fromUtf8(buffer.readLine()).trimmed();
    if(line.startsWith(u"#EXTM3U")) {
        type = Type::Extended;
    }

    QDir dir{file};
    dir.cdUp();

    TrackList tracks;

    while(!buffer.atEnd()) {
        line = QString::fromUtf8(buffer.readLine()).trimmed();

        if(line.startsWith(u'#')) {
            if(type == Type::Extended && line.startsWith(u"#EXT")) {
                if(!processMetadata(line, metadata)) {
                    qWarning() << "Failed to process metadata:" << line;
                }
            }
        }
        else if(!line.isEmpty()) {
            QString path;

            if(dir.exists()) {
                if(QDir::isAbsolutePath(line)) {
                    path = line;
                }
                else {
                    path = dir.absoluteFilePath(line);
                }
            }

            Track track{path};
            if(Tagging::readMetaData(track) || !skipNotFound) {
                if(track.title().isEmpty() && !metadata.title.isEmpty()) {
                    track.setTitle(metadata.title);
                }
                if(track.artists().empty() && !metadata.artist.isEmpty()) {
                    track.setArtists({metadata.artist});
                }
                tracks.push_back(track);
                track = {};
            }
        }
    }

    buffer.close();

    return tracks;
}
} // namespace Fooyin
