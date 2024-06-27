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

#include <core/track.h>
#include <utils/utils.h>

#include <QBuffer>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QRegularExpression>

constexpr auto CueLineRegex    = R"lit((\S+)\s+(?:"([^"]+)"|(\S+))\s*(?:"([^"]+)"|(\S+))?)lit";
constexpr auto TrackIndexRegex = R"lit((\d{1,3}):(\d{2}):(\d{2}))lit";

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

    Fooyin::Track currentFile;
    bool hasValidIndex{false};
};

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

void readRemLine(CueSheet& sheet, const QStringList& lineParts)
{
    if(lineParts.size() < 2) {
        return;
    }

    const QString& field = lineParts.at(0);
    const QString& value = lineParts.at(1);

    if(field.compare(u"DATE", Qt::CaseInsensitive) == 0) {
        sheet.date = value;
    }
    else if(field.compare(u"DISCNUMBER", Qt::CaseInsensitive) == 0) {
        if(const int disc = value.toInt(); disc > 0) {
            sheet.disc = disc;
        }
    }
    else if(field.compare(u"GENRE", Qt::CaseInsensitive) == 0) {
        sheet.genre = value;
    }
    else if(field.compare(u"COMMENT", Qt::CaseInsensitive) == 0) {
        sheet.comment = value;
    }
}

void finaliseTrack(const CueSheet& sheet, Fooyin::Track& track)
{
    track.setCuePath(sheet.cuePath);
    track.setModifiedTime(std::max(track.modifiedTime(), sheet.lastModified));

    track.setAlbumArtists({sheet.albumArtist});
    track.setAlbum(sheet.album);
    track.setGenres({sheet.genre});
    track.setDate(sheet.date);
    track.setDiscNumber(sheet.disc);
    track.setComment(sheet.comment);
    track.setComposer(sheet.composer);
}

void finaliseLastTrack(const CueSheet& sheet, Fooyin::Track& track, const QString& trackPath, bool skipNotFound,
                       Fooyin::TrackList& tracks)
{
    if(track.isValid() && (QFile::exists(trackPath) || !skipNotFound)) {
        finaliseTrack(sheet, track);
        if(track.duration() > 0) {
            track.setDuration(track.duration() - track.offset());
        }
        tracks.emplace_back(track);
    }
}
} // namespace

namespace Fooyin {
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
    if(dir.path() == u".") {
        return readEmbeddedCueTracks(device, filepath);
    }

    return readCueTracks(device, filepath, dir, skipNotFound);
}

Fooyin::TrackList CueParser::readCueTracks(QIODevice* device, const QString& filepath, const QDir& dir,
                                           bool skipNotFound)
{
    Fooyin::TrackList tracks;

    CueSheet sheet;
    sheet.cuePath = filepath;

    const QFileInfo cueInfo{filepath};
    const QDateTime lastModified{cueInfo.lastModified()};
    if(lastModified.isValid()) {
        sheet.lastModified = static_cast<uint64_t>(lastModified.toMSecsSinceEpoch());
    }

    Fooyin::Track track;
    QString trackPath;

    QTextStream in{device};
    Fooyin::PlaylistParser::detectEncoding(in, device);

    while(!in.atEnd()) {
        processCueLine(sheet, in.readLine().trimmed(), track, trackPath, dir, skipNotFound, false, tracks);

        if(sheet.type == u"BINARY") {
            qInfo() << "[CUE] Unsupported file type:" << sheet.type;
            return {};
        }
    }

    finaliseLastTrack(sheet, track, trackPath, skipNotFound, tracks);

    return tracks;
}

Fooyin::TrackList CueParser::readEmbeddedCueTracks(QIODevice* device, const QString& filepath)
{
    Fooyin::TrackList tracks;

    CueSheet sheet;
    sheet.cuePath = QStringLiteral("Embedded");

    Fooyin::Track track;
    QString trackPath{filepath};

    QTextStream in{device};
    Fooyin::PlaylistParser::detectEncoding(in, device);

    while(!in.atEnd()) {
        processCueLine(sheet, in.readLine().trimmed(), track, trackPath, {}, false, true, tracks);

        if(sheet.type == u"BINARY") {
            qInfo() << "[CUE] Unsupported file type:" << sheet.type;
            return {};
        }
    }

    finaliseLastTrack(sheet, track, filepath, false, tracks);

    return tracks;
}

void CueParser::processCueLine(CueSheet& sheet, const QString& line, Track& track, QString& trackPath, const QDir& dir,
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
            sheet.albumArtist = value;
        }
    }
    else if(field.compare(u"TITLE", Qt::CaseInsensitive) == 0) {
        if(track.isValid()) {
            track.setTitle(value);
        }
        else {
            sheet.album = value;
        }
    }
    else if(field.compare(u"COMPOSER", Qt::CaseInsensitive) == 0
            || field.compare(u"SONGWRITER", Qt::CaseInsensitive) == 0) {
        if(track.isValid()) {
            track.setComposer(value);
        }
        else {
            sheet.composer = value;
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

        if(QFile::exists(trackPath) || !skipNotFound) {
            sheet.currentFile   = PlaylistParser::readMetadata(Track{trackPath});
            sheet.hasValidIndex = false;
            track               = sheet.currentFile;

            if(parts.size() > 2) {
                sheet.type = parts.at(2);
            }
        }
    }
    else if(field.compare(u"REM", Qt::CaseInsensitive) == 0) {
        readRemLine(sheet, parts.sliced(1));
    }
    else if(field.compare(u"TRACK", Qt::CaseInsensitive) == 0) {
        if(QFile::exists(trackPath) || !skipNotFound) {
            if(track.isValid() && sheet.hasValidIndex) {
                finaliseTrack(sheet, track);
                tracks.emplace_back(track);
            }

            track = sheet.currentFile;

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
                    if(sheet.hasValidIndex && prevTrack.filepath() == track.filepath()) {
                        prevTrack.setDuration(track.offset() - prevTrack.offset());
                    }
                }

                sheet.hasValidIndex = true;
            }
        }
    }
}
} // namespace Fooyin
