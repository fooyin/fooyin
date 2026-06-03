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

#include "cueparser.h"

#include <core/constants.h>
#include <core/track.h>

#include <QBuffer>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QRegularExpression>

Q_LOGGING_CATEGORY(CUE, "fy.cue")

using namespace Qt::StringLiterals;

constexpr auto CueLineRegex    = R"lit((\S+)\s+(?:"([^"]+)"|(\S+))\s*(?:"([^"]+)"|(\S+))?)lit";
constexpr auto TrackIndexRegex = R"lit((\d{1,3}):(\d{2}):(\d{2}))lit";

struct CueSheet
{
    QString cuePath;
    QString type;
    QString performer;
    QString album;
    QString composer;
    QString genre;
    QString date;
    QString comment;
    QString disc;
    uint64_t lastModified{0};

    float rgAlbumGain{Fooyin::Constants::InvalidGain};
    float rgAlbumPeak{Fooyin::Constants::InvalidPeak};

    Fooyin::Track currentFile;

    bool singleTrackFile{false};
    bool hasValidIndex{false};
    bool addedTrack{false};
    bool hasTrackPerformer{false};

    bool skipNotFound{false};
    bool skipFile{false};
};

namespace {
Fooyin::Track unreadableCueTrack(const QString& path)
{
    Fooyin::Track track{path};
    track.setIsEnabled(false);
    return track;
}

float parseGain(const QString& gainStr)
{
    static const QRegularExpression regex{uR"([+-]?\d+(\.\d+))"_s};
    const QRegularExpressionMatch match = regex.match(gainStr);

    if(match.hasMatch()) {
        bool ok{false};
        const float gain = match.captured(0).toFloat(&ok);
        if(ok) {
            return gain;
        }
    }

    return Fooyin::Constants::InvalidGain;
}

float parsePeak(const QString& peakStr)
{
    bool ok{false};
    const float peak = peakStr.toFloat(&ok);
    if(ok) {
        return peak;
    }

    return Fooyin::Constants::InvalidPeak;
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

bool isTopLevelCueLine(const QString& line)
{
    return !line.isEmpty() && !line.front().isSpace();
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

QString findMatchingFile(const QString& filepath)
{
    const QFileInfo info{filepath};
    const QDir dir         = info.absoluteDir();
    const QString baseName = info.completeBaseName();
    const QString fileName = info.fileName();

    const auto files = dir.entryList(QDir::Files);

    // Find exact match first
    for(const QString& file : files) {
        if(file.compare(fileName, Qt::CaseInsensitive) == 0) {
            return dir.absoluteFilePath(file);
        }
    }

    // Else find matching filename without extension
    for(const QString& file : files) {
        const QFileInfo candidateInfo{file};
        if(candidateInfo.suffix().compare(u"cue"_s, Qt::CaseInsensitive) == 0) {
            continue;
        }

        if(candidateInfo.completeBaseName().compare(baseName, Qt::CaseInsensitive) == 0) {
            return dir.absoluteFilePath(file);
        }
    }

    return filepath;
}

bool shouldKeepUnreadableTrack(const CueSheet& sheet, const QString& trackPath)
{
    return QFile::exists(trackPath) || !sheet.skipNotFound;
}

template <typename Setter>
void applyIfNotEmpty(const QString& value, Setter&& setter)
{
    if(!value.isEmpty()) {
        setter(value);
    }
}

template <typename Setter>
void applyIfValid(float value, float invalidValue, Setter&& setter)
{
    if(value != invalidValue) {
        setter(value);
    }
}

void readAlbumRemLine(CueSheet& sheet, const QStringList& lineParts)
{
    if(lineParts.size() < 2) {
        return;
    }

    const QString& field = lineParts.at(0);
    const QString& value = lineParts.at(1);

    if(field.compare("DATE"_L1, Qt::CaseInsensitive) == 0) {
        sheet.date = value;
    }
    else if(field.compare("DISCNUMBER"_L1, Qt::CaseInsensitive) == 0) {
        sheet.disc = value;
    }
    else if(field.compare("GENRE"_L1, Qt::CaseInsensitive) == 0) {
        sheet.genre = value;
    }
    else if(field.compare("COMMENT"_L1, Qt::CaseInsensitive) == 0) {
        sheet.comment = value;
    }
    else if(field.compare("REPLAYGAIN_ALBUM_GAIN"_L1, Qt::CaseInsensitive) == 0) {
        sheet.rgAlbumGain = parseGain(value);
    }
    else if(field.compare("REPLAYGAIN_ALBUM_PEAK"_L1, Qt::CaseInsensitive) == 0) {
        sheet.rgAlbumPeak = parsePeak(value);
    }
}

void readTrackRemLine(Fooyin::Track& track, const QStringList& lineParts)
{
    if(lineParts.size() < 2) {
        return;
    }

    const QString& field = lineParts.at(0);
    const QString& value = lineParts.at(1);

    if(field.compare("REPLAYGAIN_TRACK_GAIN"_L1, Qt::CaseInsensitive) == 0) {
        track.setRGTrackGain(parseGain(value));
    }
    else if(field.compare("REPLAYGAIN_TRACK_PEAK"_L1, Qt::CaseInsensitive) == 0) {
        track.setRGTrackPeak(parsePeak(value));
    }
}

void applyCuePerformer(const CueSheet& sheet, Fooyin::Track& track)
{
    applyIfNotEmpty(sheet.performer, [&sheet, &track](const QString& value) {
        if(sheet.hasTrackPerformer) {
            track.setAlbumArtists({value});
        }
        else {
            track.setArtists({value});
        }
    });
}

void finaliseTrack(const CueSheet& sheet, Fooyin::Track& track, bool applyPerformer = true)
{
    track.setCuePath(sheet.cuePath);
    track.setModifiedTime(std::max(track.modifiedTime(), sheet.lastModified));

    applyIfNotEmpty(sheet.album, [&track](const QString& value) { track.setAlbum(value); });
    applyIfNotEmpty(sheet.genre, [&track](const QString& value) { track.setGenres({value}); });
    applyIfNotEmpty(sheet.date, [&track](const QString& value) { track.setDate(value); });
    applyIfNotEmpty(sheet.disc, [&track](const QString& value) { track.setDiscNumber(value); });
    applyIfNotEmpty(sheet.comment, [&track](const QString& value) { track.setComment(value); });
    applyIfNotEmpty(sheet.composer, [&track](const QString& value) { track.setComposers({value}); });
    applyIfValid(sheet.rgAlbumGain, Fooyin::Constants::InvalidGain,
                 [&track](float value) { track.setRGAlbumGain(value); });
    applyIfValid(sheet.rgAlbumPeak, Fooyin::Constants::InvalidPeak,
                 [&track](float value) { track.setRGAlbumPeak(value); });

    if(applyPerformer) {
        applyCuePerformer(sheet, track);
    }
}

void finaliseLastTrack(const CueSheet& sheet, Fooyin::Track& track, const QString& trackPath, Fooyin::TrackList& tracks)
{
    if(track.isValid() && (QFile::exists(trackPath) || !sheet.skipNotFound)) {
        finaliseTrack(sheet, track, false);
        if(track.duration() > 0 && track.duration() > track.offset()) {
            track.setDuration(track.duration() - track.offset());
        }
        tracks.emplace_back(track);
    }
}

void finaliseDurations(Fooyin::TrackList& tracks)
{
    if(tracks.size() <= 1) {
        return;
    }

    for(auto it = tracks.begin(); it != tracks.end() - 1; ++it) {
        auto currentTrack = it;
        auto nextTrack    = it + 1;

        if(currentTrack->filepath() == nextTrack->filepath()) {
            currentTrack->setDuration(nextTrack->offset() - currentTrack->offset());
        }
    }
}
} // namespace

namespace Fooyin {
QString CueParser::name() const
{
    return u"CUE"_s;
}

QStringList CueParser::supportedExtensions() const
{
    static const QStringList extensions{u"cue"_s};
    return extensions;
}

bool CueParser::saveIsSupported() const
{
    // TODO: Implement saving cue files
    return false;
}

size_t CueParser::countEntries(QIODevice* device, const QString& /*filepath*/, const QDir& /*dir*/) const
{
    QByteArray cue = toUtf8(device);
    QBuffer buffer{&cue};
    if(!buffer.open(QIODevice::ReadOnly)) {
        return 0;
    }

    size_t entries{0};

    while(!buffer.atEnd()) {
        const QString line = QString::fromUtf8(buffer.readLine()).trimmed();
        if(line.startsWith("TRACK "_L1, Qt::CaseInsensitive)) {
            ++entries;
        }
    }

    return entries;
}

TrackList CueParser::readPlaylist(QIODevice* device, const QString& filepath, const QDir& dir,
                                  const ReadPlaylistEntry& readEntry, bool skipNotFound)
{
    if(dir.path() == "."_L1) {
        return readEmbeddedCueTracks(device, filepath, readEntry);
    }

    return readCueTracks(device, filepath, dir, readEntry, skipNotFound);
}

TrackList CueParser::readCueTracks(QIODevice* device, const QString& filepath, const QDir& dir,
                                   const ReadPlaylistEntry& readEntry, bool skipNotFound)
{
    TrackList tracks;

    CueSheet sheet;
    sheet.cuePath      = filepath;
    sheet.skipNotFound = skipNotFound;
    sheet.skipFile     = false;

    const QFileInfo cueInfo{filepath};
    const QDateTime lastModified{cueInfo.lastModified()};
    if(lastModified.isValid()) {
        sheet.lastModified = static_cast<uint64_t>(lastModified.toMSecsSinceEpoch());
    }

    Track track;
    QString trackPath;

    QByteArray m3u = toUtf8(device);
    QBuffer buffer{&m3u};
    if(!buffer.open(QIODevice::ReadOnly)) {
        return {};
    }

    while(!buffer.atEnd() && !readEntry.cancel) {
        const QString line = QString::fromUtf8(buffer.readLine());
        processCueLine(sheet, line, track, trackPath, dir, readEntry, tracks);
    }

    if(readEntry.cancel) {
        return {};
    }

    finaliseLastTrack(sheet, track, trackPath, tracks);

    for(auto& parsedTrack : tracks) {
        finaliseTrack(sheet, parsedTrack);
    }

    finaliseDurations(tracks);

    return tracks;
}

TrackList CueParser::readEmbeddedCueTracks(QIODevice* device, const QString& filepath,
                                           const ReadPlaylistEntry& readEntry)
{
    TrackList tracks;

    CueSheet sheet;
    sheet.cuePath      = u"Embedded"_s;
    sheet.skipNotFound = false;
    sheet.skipFile     = true;

    Track track;
    QString trackPath{filepath};

    QByteArray m3u = toUtf8(device);
    QBuffer buffer{&m3u};
    if(!buffer.open(QIODevice::ReadOnly)) {
        return {};
    }

    while(!buffer.atEnd() && !readEntry.cancel) {
        const QString line = QString::fromUtf8(buffer.readLine());
        processCueLine(sheet, line, track, trackPath, {}, readEntry, tracks);
    }

    if(readEntry.cancel) {
        return {};
    }

    finaliseLastTrack(sheet, track, filepath, tracks);

    for(auto& parsedTrack : tracks) {
        finaliseTrack(sheet, parsedTrack);
    }

    finaliseDurations(tracks);

    return tracks;
}

void CueParser::processCueLine(CueSheet& sheet, const QString& line, Track& track, QString& trackPath, const QDir& dir,
                               const ReadPlaylistEntry& readEntry, TrackList& tracks)
{
    const QString trimmedLine = line.trimmed();
    const bool topLevelLine   = isTopLevelCueLine(line);

    const QStringList parts = splitCueLine(trimmedLine);
    if(parts.size() < 2) {
        return;
    }

    const QString& field = parts.at(0);
    const QString& value = parts.at(1);

    if(field.compare("PERFORMER"_L1, Qt::CaseInsensitive) == 0) {
        if(!topLevelLine && track.isValid()) {
            track.setArtists({value});
            sheet.hasTrackPerformer = true;
        }
        else {
            sheet.performer = value;
        }
    }
    else if(field.compare("TITLE"_L1, Qt::CaseInsensitive) == 0) {
        if(!topLevelLine && track.isValid()) {
            track.setTitle(value);
        }
        else {
            sheet.album = value;
        }
    }
    else if(field.compare("COMPOSER"_L1, Qt::CaseInsensitive) == 0
            || field.compare("SONGWRITER"_L1, Qt::CaseInsensitive) == 0) {
        if(!topLevelLine && track.isValid()) {
            track.setComposers({value});
        }
        else {
            sheet.composer = value;
        }
    }
    else if(field.compare("FILE"_L1, Qt::CaseInsensitive) == 0) {
        const QString cueFileValue{value};

        if(!sheet.skipFile && dir.exists()) {
            const QString requestedPath
                = QDir::isAbsolutePath(value) ? QDir::cleanPath(value) : QDir::cleanPath(dir.absoluteFilePath(value));

            if(QDir::isAbsolutePath(value)) {
                trackPath = QDir::cleanPath(value);
            }
            else {
                trackPath = QDir::cleanPath(dir.absoluteFilePath(value));
            }

            if(!QFile::exists(trackPath)) {
                trackPath = findMatchingFile(trackPath);
            }

            if(trackPath != requestedPath) {
                qCInfo(CUE) << "Matched alternate CUE image file:" << requestedPath << "->" << trackPath
                            << "cue=" << sheet.cuePath;
            }
        }

        const bool fileExists = QFile::exists(trackPath);

        qCDebug(CUE) << "Resolved CUE FILE entry:" << cueFileValue << "to" << trackPath << "exists=" << fileExists
                     << "cue=" << sheet.cuePath;

        if(track.isValid() && !sheet.addedTrack && sheet.hasValidIndex) {
            finaliseLastTrack(sheet, track, trackPath, tracks);
            track            = {};
            sheet.addedTrack = true;
        }

        if(fileExists || !sheet.skipNotFound) {
            sheet.currentFile = readEntry.readTrack(Track{trackPath});

            if(!sheet.currentFile.metadataWasRead()) {
                const bool canLoadTrack = !readEntry.canLoadTrack || readEntry.canLoadTrack(sheet.currentFile);

                if(!canLoadTrack) {
                    qCWarning(CUE) << "Unable to read CUE image file:" << trackPath << "cue=" << sheet.cuePath
                                   << "exists=" << fileExists;
                    if(shouldKeepUnreadableTrack(sheet, trackPath)) {
                        sheet.currentFile = unreadableCueTrack(trackPath);
                    }
                    else {
                        sheet.currentFile = {};
                    }
                }
                else {
                    qCInfo(CUE) << "Using CUE metadata fallback for playable file:" << trackPath
                                << "cue=" << sheet.cuePath;
                }
            }

            if(!track.trackNumber().isEmpty()) {
                sheet.singleTrackFile = true;
            }

            track = sheet.currentFile;

            if(parts.size() > 2) {
                sheet.type = parts.at(2);
            }
        }
        else {
            qCWarning(CUE) << "Referenced CUE image file not found:" << trackPath << "cue=" << sheet.cuePath;
        }
    }
    else if(field.compare("REM"_L1, Qt::CaseInsensitive) == 0) {
        if(topLevelLine) {
            readAlbumRemLine(sheet, parts.sliced(1));
        }
        else {
            readTrackRemLine(track, parts.sliced(1));
            readAlbumRemLine(sheet, parts.sliced(1));
        }
    }
    else if(field.compare("TRACK"_L1, Qt::CaseInsensitive) == 0) {
        if(QFile::exists(trackPath) || !sheet.skipNotFound) {
            if(track.isValid() && !sheet.addedTrack && sheet.hasValidIndex) {
                finaliseTrack(sheet, track, false);
                tracks.emplace_back(track);
            }

            track                 = sheet.currentFile;
            sheet.singleTrackFile = false;
            sheet.hasValidIndex   = false;
            sheet.addedTrack      = false;

            track.setTrackNumber(parts.at(1));
        }
    }
    else if(field.compare("INDEX"_L1, Qt::CaseInsensitive) == 0) {
        if(value == "01"_L1 && parts.size() > 2) {
            if(const auto start = msfToMs(parts.at(2))) {
                if(track.trackNumber() == "01"_L1 || !sheet.singleTrackFile) {
                    track.setOffset(start.value());
                }
                sheet.hasValidIndex = true;
            }
        }
    }
}
} // namespace Fooyin
