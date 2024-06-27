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

#include "cueparser.h"

#include "tagging/tagreader.h"
#include "utils/utils.h"

#include <core/track.h>

#include <QBuffer>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QRegularExpression>

constexpr auto CueLineRegex    = R"lit((\S+)\s+(?:"([^"]+)"|(\S+))\s*(?:"([^"]+)"|(\S+))?)lit";
constexpr auto TrackIndexRegex = R"lit((\d{1,3}):(\d{2}):(\d{2}))lit";

namespace {
QStringList splitCueLine(const QString& line)
{
    static const QRegularExpression lineRegex{QLatin1String{CueLineRegex}};
    const QRegularExpressionMatch match = lineRegex.match(line);
    if(!match.hasMatch()) {
        return {};
    }

    const QStringList capturedTexts = match.capturedTexts().mid(1);

    QStringList result;
    result.reserve(capturedTexts.size() - 1);

    for(const QString& text : capturedTexts) {
        if(!text.isEmpty() && text != line) {
            result.append(text);
        }
    }

    return result;
};

std::optional<uint64_t> msfToMs(const QString& index)
{
    static const QRegularExpression indexRegex{QLatin1String{TrackIndexRegex}};
    const QRegularExpressionMatch match = indexRegex.match(index);
    if(!match.hasMatch()) {
        return {};
    }

    const QStringList parts = match.capturedTexts().mid(1);

    const int minutes = parts.at(0).toInt();
    const int seconds = parts.at(1).toInt();
    const int frames  = parts.at(2).toInt();

    return ((minutes * 60 + seconds) * 1000) + frames * 1000 / 75;
}
} // namespace

namespace Fooyin {
CueParser::CueParser()
    : m_hasValidIndex{false}
{ }

QString CueParser::name() const
{
    return QStringLiteral("CUE");
}

QStringList CueParser::supportedExtensions() const
{
    static const QStringList extensions{QStringLiteral("cue")};
    return extensions;
}

bool CueParser::saveIsSupported() const
{
    // TODO: Implement saving cue files
    return false;
}

TrackList CueParser::readPlaylist(QIODevice* device, const QString& filepath, const QDir& dir, bool skipNotFound)
{
    m_sheet         = {};
    m_hasValidIndex = false;

    if(dir.path() == u".") {
        return readEmbeddedCueTracks(device, filepath);
    }

    return readCueTracks(device, filepath, dir, skipNotFound);
}

TrackList CueParser::readCueTracks(QIODevice* device, const QString& filepath, const QDir& dir, bool skipNotFound)
{
    TrackList tracks;

    m_sheet.cuePath = filepath;

    const QFileInfo cueInfo{filepath};
    const QDateTime lastModified{cueInfo.lastModified()};
    if(lastModified.isValid()) {
        m_sheet.lastModified = static_cast<uint64_t>(lastModified.toMSecsSinceEpoch());
    }

    Track track;
    QString trackPath;

    QTextStream in{device};
    Fooyin::PlaylistParser::detectEncoding(in, device);

    while(!in.atEnd()) {
        processCueLine(in.readLine().trimmed(), track, trackPath, dir, skipNotFound, false, tracks);

        if(m_sheet.type == u"BINARY") {
            qInfo() << "[CUE] Unsupported file type:" << m_sheet.type;
            return {};
        }
    }

    finaliseLastTrack(track, trackPath, skipNotFound, tracks);

    return tracks;
}

TrackList CueParser::readEmbeddedCueTracks(QIODevice* device, const QString& filepath)
{
    TrackList tracks;

    m_sheet.cuePath = QStringLiteral("Embedded");

    Track track;
    QString trackPath{filepath};

    QTextStream in{device};
    Fooyin::PlaylistParser::detectEncoding(in, device);

    while(!in.atEnd()) {
        processCueLine(in.readLine().trimmed(), track, trackPath, {}, false, true, tracks);

        if(m_sheet.type == u"BINARY") {
            qInfo() << "[CUE] Unsupported file type:" << m_sheet.type;
            return {};
        }
    }

    finaliseLastTrack(track, filepath, false, tracks);

    return tracks;
}

void CueParser::processCueLine(const QString& line, Track& track, QString& trackPath, const QDir& dir,
                               bool skipNotFound, bool skipFile, TrackList& tracks)
{
    const QStringList parts = splitCueLine(line);
    if(parts.size() < 2) {
        return;
    }

    const QString& field = parts.at(0);
    const QString& value = parts.at(1);

    if(field.compare(u"PERFORMER", Qt::CaseInsensitive) == 0) {
        if(track.isValid()) {
            track.setArtists({value});
        }
        else {
            m_sheet.albumArtist = value;
        }
    }
    else if(field.compare(u"TITLE", Qt::CaseInsensitive) == 0) {
        if(track.isValid()) {
            track.setTitle(value);
        }
        else {
            m_sheet.album = value;
        }
    }
    else if(field.compare(u"COMPOSER", Qt::CaseInsensitive) == 0
            || field.compare(u"SONGWRITER", Qt::CaseInsensitive) == 0) {
        if(track.isValid()) {
            track.setComposer(value);
        }
        else {
            m_sheet.composer = value;
        }
    }
    else if(field.compare(u"FILE", Qt::CaseInsensitive) == 0) {
        if(!skipFile && dir.exists()) {
            if(QDir::isAbsolutePath(value)) {
                trackPath = QDir::cleanPath(value);
            }
            else {
                trackPath = QDir::cleanPath(dir.absoluteFilePath(value));
            }
        }

        m_currentFile = Fooyin::Track{trackPath};
        Fooyin::Tagging::readMetaData(m_currentFile);
        m_hasValidIndex = false;
        track           = m_currentFile;

        if(parts.size() > 2) {
            m_sheet.type = parts.at(2);
        }
    }
    else if(field.compare(u"REM", Qt::CaseInsensitive) == 0) {
        readRemLine(parts.sliced(1));
    }
    else if(field.compare(u"TRACK", Qt::CaseInsensitive) == 0) {
        if(QFile::exists(trackPath) || !skipNotFound) {
            if(track.isValid() && m_hasValidIndex) {
                finaliseTrack(track);
                tracks.emplace_back(track);
            }

            track = m_currentFile;

            if(const int trackNum = parts.at(1).toInt(); trackNum > 0) {
                track.setTrackNumber(trackNum);
            }
        }
    }
    else if(field.compare(u"INDEX", Qt::CaseInsensitive) == 0) {
        if(value == u"01" && parts.size() > 2) {
            if(const auto start = msfToMs(parts.at(2))) {
                track.setOffset(start.value());

                if(const auto count = tracks.size(); count > 0) {
                    Fooyin::Track& prevTrack = tracks.at(count - 1);
                    if(m_hasValidIndex && prevTrack.filepath() == track.filepath()) {
                        prevTrack.setDuration(track.offset() - prevTrack.offset());
                    }
                }

                m_hasValidIndex = true;
            }
        }
    }
}

void CueParser::readRemLine(const QStringList& lineParts)
{
    if(lineParts.size() < 2) {
        return;
    }

    const QString& field = lineParts.at(0);
    const QString& value = lineParts.at(1);

    if(field.compare(u"DATE", Qt::CaseInsensitive) == 0) {
        m_sheet.date = value;
    }
    else if(field.compare(u"DISCNUMBER", Qt::CaseInsensitive) == 0) {
        if(const int disc = value.toInt(); disc > 0) {
            m_sheet.disc = disc;
        }
    }
    else if(field.compare(u"GENRE", Qt::CaseInsensitive) == 0) {
        m_sheet.genre = value;
    }
    else if(field.compare(u"COMMENT", Qt::CaseInsensitive) == 0) {
        m_sheet.comment = value;
    }
}

void CueParser::finaliseTrack(Track& track)
{
    track.setCuePath(m_sheet.cuePath);
    track.setModifiedTime(std::max(track.modifiedTime(), m_sheet.lastModified));

    track.setAlbumArtists({m_sheet.albumArtist});
    track.setAlbum(m_sheet.album);
    track.setGenres({m_sheet.genre});
    track.setDate(m_sheet.date);
    track.setDiscNumber(m_sheet.disc);
    track.setComment(m_sheet.comment);
    track.setComposer(m_sheet.composer);
}

void CueParser::finaliseLastTrack(Track& track, const QString& trackPath, bool skipNotFound, Fooyin::TrackList& tracks)
{
    if(track.isValid() && (QFile::exists(trackPath) || !skipNotFound)) {
        finaliseTrack(track);
        if(track.duration() > 0) {
            track.setDuration(track.duration() - track.offset());
        }
        tracks.emplace_back(track);
    }
}
} // namespace Fooyin
