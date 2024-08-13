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

#include <core/playlist/playlistparser.h>

#include <utils/utils.h>

#include <QUrl>

namespace Fooyin {
PlaylistParser::PlaylistParser(std::shared_ptr<AudioLoader> audioLoader)
    : m_audioLoader{std::move(audioLoader)}
{ }

void Fooyin::PlaylistParser::savePlaylist(QIODevice* /*device*/, const QString& /*extension*/,
                                          const TrackList& /*tracks*/, const QDir& /*dir*/, PathType /*type*/,
                                          bool /*writeMetdata*/)
{ }

QString PlaylistParser::determineTrackPath(const QUrl& url, const QDir& dir, PathType type)
{
    if(!url.isLocalFile()) {
        return url.toString();
    }

    QString filepath = url.toLocalFile();

    if(type != PathType::Absolute && QDir::isAbsolutePath(filepath)) {
        const QString relative = dir.relativeFilePath(filepath);

        if(!relative.startsWith(u"../") || type == PathType::Relative) {
            return relative;
        }
    }

    return filepath;
}

bool PlaylistParser::detectEncoding(QTextStream& in, QIODevice* file)
{
    const QByteArray data = file->peek(1024);
    auto encoding         = QStringConverter::encodingForData(data);
    if(encoding) {
        in.setEncoding(encoding.value());
    }
    else {
        const auto encodingName = Utils::detectEncoding(data);
        if(encodingName.isEmpty()) {
            return false;
        }

        encoding = QStringConverter::encodingForName(encodingName.constData());
        if(encoding) {
            in.setEncoding(encoding.value());
        }
    }
    return true;
}

Track PlaylistParser::readMetadata(const Track& track)
{
    Track readTrack{track};
    const QFileInfo fileInfo{track.filepath()};
    readTrack.setFileSize(fileInfo.size());

    if(!m_audioLoader->readTrackMetadata(readTrack)) {
        return {};
    }

    if(readTrack.addedTime() == 0) {
        readTrack.setAddedTime(QDateTime::currentMSecsSinceEpoch());
    }
    if(readTrack.modifiedTime() == 0) {
        const QDateTime modifiedTime = fileInfo.lastModified();
        readTrack.setModifiedTime(modifiedTime.isValid() ? modifiedTime.toMSecsSinceEpoch() : 0);
    }

    readTrack.generateHash();

    return readTrack;
}

} // namespace Fooyin
