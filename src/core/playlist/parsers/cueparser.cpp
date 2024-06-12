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

void setupEncoding(QTextStream& in, QIODevice* file)
{
    const QByteArray data = file->peek(1024);
    auto encoding         = QStringConverter::encodingForData(data);
    if(encoding) {
        in.setEncoding(encoding.value());
    }
    else {
        const auto encodingName = Fooyin::Utils::detectEncoding(data);
        if(!encodingName.isEmpty()) {
            encoding = QStringConverter::encodingForName(encodingName.constData());
            if(encoding) {
                in.setEncoding(encoding.value());
            }
        }
    }
}

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

void readRemLine(const QStringList& lineParts, CueSheet& sheet)
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

void finaliseTrack(const CueSheet& cue, Fooyin::Track& track)
{
    track.setCuePath(cue.cuePath);
    track.setModifiedTime(std::max(track.modifiedTime(), cue.lastModified));

    if(track.albumArtists().empty()) {
        track.setAlbumArtists({cue.albumArtist});
    }
    if(track.album().isEmpty()) {
        track.setAlbum(cue.album);
    }
    if(track.genres().empty()) {
        track.setGenres({cue.genre});
    }
    if(track.date().isEmpty()) {
        track.setDate(cue.date);
    }
    if(track.discNumber() <= 0) {
        track.setDiscNumber(cue.disc);
    }
    if(track.comment().isEmpty()) {
        track.setComment(cue.comment);
    }
    if(track.composer().isEmpty()) {
        track.setComposer(cue.composer);
    }
}

void finaliseLastTrack(CueSheet& sheet, Fooyin::Track& track, const QString& trackPath, bool skipNotFound,
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

void processCueLine(const QString& line, CueSheet& sheet, Fooyin::Track& track, QString& trackPath, const QDir& dir,
                    bool skipNotFound, bool skipFile, Fooyin::TrackList& tracks)
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
        if(!skipFile) {
            trackPath = dir.exists() ? (QDir::isAbsolutePath(value) ? value : dir.absoluteFilePath(value)) : trackPath;
        }
        if(parts.size() > 2) {
            sheet.type = parts.at(2);
        }
    }
    else if(field.compare(u"REM", Qt::CaseInsensitive) == 0) {
        readRemLine(parts.sliced(1), sheet);
    }
    else if(field.compare(u"TRACK", Qt::CaseInsensitive) == 0) {
        if(QFile::exists(trackPath) || !skipNotFound) {
            if(track.isValid()) {
                finaliseTrack(sheet, track);
                tracks.emplace_back(track);
            }

            track = Fooyin::Track{trackPath};
            Fooyin::Tagging::readMetaData(track);
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
                    prevTrack.setDuration(track.offset() - prevTrack.offset());
                }
            }
        }
    }
}

Fooyin::TrackList readCueTracks(QFile* file, const QString& filepath, const QDir& dir, bool skipNotFound)
{
    Fooyin::TrackList tracks;

    CueSheet sheet;
    sheet.cuePath = filepath;

    const QFileInfo cueInfo{*file};
    const QDateTime lastModified{cueInfo.lastModified()};
    if(lastModified.isValid()) {
        sheet.lastModified = static_cast<uint64_t>(lastModified.toMSecsSinceEpoch());
    }

    Fooyin::Track track;
    QString trackPath;

    QTextStream in{file};
    setupEncoding(in, file);

    while(!in.atEnd()) {
        processCueLine(in.readLine().trimmed(), sheet, track, trackPath, dir, skipNotFound, false, tracks);

        if(sheet.type == u"BINARY") {
            qInfo() << "[CUE] Unsupported file type:" << sheet.type;
            return {};
        }
    }

    finaliseLastTrack(sheet, track, trackPath, skipNotFound, tracks);

    return tracks;
}

Fooyin::TrackList readEmbeddedCueTracks(QIODevice* file, const QString& filepath)
{
    Fooyin::TrackList tracks;
    CueSheet sheet;

    sheet.cuePath = QStringLiteral("Embedded");

    Fooyin::Track track;
    QString trackPath{filepath};

    QTextStream in{file};
    setupEncoding(in, file);

    while(!in.atEnd()) {
        processCueLine(in.readLine().trimmed(), sheet, track, trackPath, {}, false, true, tracks);

        if(sheet.type == u"BINARY") {
            qInfo() << "[CUE] Unsupported file type:" << sheet.type;
            return {};
        }
    }

    finaliseLastTrack(sheet, track, filepath, false, tracks);

    return tracks;
}
} // namespace

namespace Fooyin {
TrackList CueParser::readPlaylist(const QString& file, bool skipNotFound)
{
    if(file.isEmpty()) {
        return {};
    }

    QFile cueFile{file};

    if(!cueFile.open(QIODevice::ReadOnly)) {
        qWarning() << QStringLiteral("Could not open cue file %1 for reading: %2")
                          .arg(cueFile.fileName(), cueFile.errorString());
        return {};
    }

    QDir dir{file};
    dir.cdUp();

    return readCueTracks(&cueFile, file, dir, skipNotFound);
}

TrackList CueParser::readEmbeddedCue(const QString& cueSheet, const QString& filepath)
{
    if(cueSheet.isEmpty() || filepath.isEmpty()) {
        return {};
    }

    QByteArray cueBytes{cueSheet.toUtf8()};
    QBuffer cueBuffer(&cueBytes);
    if(!cueBuffer.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << QStringLiteral("Can't open cue buffer for reading: %1").arg(cueBuffer.errorString());
        return {};
    }

    return readEmbeddedCueTracks(&cueBuffer, filepath);
}
} // namespace Fooyin
