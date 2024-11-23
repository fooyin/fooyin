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

#include <utils/stringutils.h>

#include <QStringDecoder>
#include <QUrl>

using namespace Qt::StringLiterals;

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

        if(!relative.startsWith("../"_L1) || type == PathType::Relative) {
            return relative;
        }
    }

    return filepath;
}

QByteArray PlaylistParser::toUtf8(QIODevice* file)
{
    const QByteArray data = file->readAll();
    if(data.isEmpty()) {
        return {};
    }

    QStringDecoder toUtf16;

    auto encoding = QStringConverter::encodingForData(data);
    if(encoding) {
        toUtf16 = QStringDecoder{encoding.value()};
    }
    else {
        const auto encodingName = Utils::detectEncoding(data);
        if(encodingName.isEmpty()) {
            return {};
        }

        encoding = QStringConverter::encodingForName(encodingName.constData());
        if(encoding) {
            toUtf16 = QStringDecoder{encoding.value()};
        }
        else {
            toUtf16 = QStringDecoder{encodingName.constData()};
        }
    }

    if(!toUtf16.isValid()) {
        toUtf16 = QStringDecoder{QStringConverter::Utf8};
    }

    QString string = toUtf16(data);
    string.replace("\n\n"_L1, "\n"_L1);
    string.replace('\r'_L1, '\n'_L1);

    return string.toUtf8();
}
} // namespace Fooyin
