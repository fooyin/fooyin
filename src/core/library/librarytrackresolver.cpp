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

#include "librarytrackresolver.h"

#include "database/trackdatabase.h"
#include "libraryscanstate.h"
#include "libraryscanutils.h"
#include "libraryscanwriter.h"
#include "playlist/playlistloader.h"

#include <core/coresettings.h>
#include <core/engine/audioloader.h>
#include <core/playlist/playlistparser.h>

#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <utility>

Q_DECLARE_LOGGING_CATEGORY(LIB_SCANNER)

using namespace Qt::StringLiterals;

constexpr auto ArchivePath = R"(unpack://%1|%2|file://%3!)";

namespace Fooyin {
LibraryTrackResolver::LibraryTrackResolver(LibraryInfo currentLibrary, PlaylistLoader* playlistLoader,
                                           AudioLoader* audioLoader, const bool playlistSkipMissing,
                                           TrackDatabase* trackDatabase, LibraryScanState* state,
                                           LibraryScanWriter* writer, FlushWritesHandler flushWrites)
    : m_currentLibrary{std::move(currentLibrary)}
    , m_playlistLoader{playlistLoader}
    , m_audioLoader{audioLoader}
    , m_trackDatabase{trackDatabase}
    , m_state{state}
    , m_writer{writer}
    , m_flushWrites{std::move(flushWrites)}
    , m_playlistSkipMissing{playlistSkipMissing}
{ }

TrackList LibraryTrackResolver::readTracks(const QString& filepath)
{
    if(m_audioLoader->isArchive(filepath)) {
        return readArchiveTracks(filepath);
    }

    const Track track{filepath};

    auto loadedReader = m_audioLoader->loadReaderForTrack(track);
    if(!loadedReader.reader) {
        qCDebug(LIB_SCANNER) << "Unsupported file:" << filepath;
        return {};
    }

    TrackList tracks;
    AudioSource source     = loadedReader.input.source;
    const int subsongCount = std::max(loadedReader.reader->subsongCount(), 1);

    for(int subIndex{0}; subIndex < subsongCount; ++subIndex) {
        if(!m_state->mayRun()) {
            return tracks;
        }

        Track subTrack{filepath, subIndex};

        if(source.device) {
            subTrack.setFileSize(source.device->size());
            if(!source.device->seek(0)) {
                qCDebug(LIB_SCANNER) << "Failed to rewind file:" << filepath;
                break;
            }
        }

        if(loadedReader.reader->readTrack(source, subTrack)) {
            subTrack.generateHash();
            tracks.push_back(subTrack);
        }
    }

    return tracks;
}

TrackList LibraryTrackResolver::readPlaylist(const QString& filepath)
{
    TrackList tracks;

    const TrackList playlistTracks = readPlaylistTracks(filepath);
    for(const Track& playlistTrack : playlistTracks) {
        if(const auto existingTrack = m_state->findExistingTrackByUniqueFilepath(playlistTrack)) {
            tracks.push_back(existingTrack.value());
        }
        else {
            Track track{playlistTrack};
            track.generateHash();
            tracks.push_back(track);
        }
    }

    return tracks;
}

size_t LibraryTrackResolver::countPlaylistTracks(const QString& path) const
{
    if(path.isEmpty()) {
        return 0;
    }

    QFile playlistFile{path};
    if(!playlistFile.open(QIODevice::ReadOnly)) {
        return 0;
    }

    const QFileInfo info{playlistFile};
    QDir dir{path};
    dir.cdUp();

    if(auto* parser = m_playlistLoader->parserForExtension(info.suffix())) {
        return parser->countEntries(&playlistFile, path, dir);
    }

    return 0;
}

TrackList LibraryTrackResolver::readPlaylistTracks(const QString& path)
{
    if(path.isEmpty()) {
        return {};
    }

    QFile playlistFile{path};
    if(!playlistFile.open(QIODevice::ReadOnly)) {
        qCInfo(LIB_SCANNER) << "Could not open file" << path << "for reading:" << playlistFile.errorString();
        return {};
    }

    const QFileInfo info{playlistFile};
    QDir dir{path};
    dir.cdUp();

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [this, &readEntry](const Track& playlistTrack) {
        if(!m_state->mayRun()) {
            readEntry.cancel = true;
            return playlistTrack;
        }

        Track readTrack{playlistTrack};
        readFileProperties(readTrack);

        if(!m_audioLoader->readTrackMetadata(readTrack)) {
            return playlistTrack;
        }

        readTrack.generateHash();
        m_state->progressScanned(readTrack.prettyFilepath());

        return readTrack;
    };

    if(auto* parser = m_playlistLoader->parserForExtension(info.suffix())) {
        return parser->readPlaylist(&playlistFile, path, dir, readEntry, m_playlistSkipMissing);
    }

    return {};
}

TrackList LibraryTrackResolver::readEmbeddedPlaylistTracks(const Track& track)
{
    const auto cues = track.extraTag(u"CUESHEET"_s);
    if(cues.empty()) {
        return {};
    }

    QByteArray bytes{cues.front().toUtf8()};
    QBuffer buffer(&bytes);
    if(!buffer.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCInfo(LIB_SCANNER) << "Can't open buffer for reading:" << buffer.errorString();
        return {};
    }

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [this, &readEntry](const Track& playlistTrack) {
        if(!m_state->mayRun()) {
            readEntry.cancel = true;
            return playlistTrack;
        }

        Track readTrack{playlistTrack};
        if(!m_audioLoader->readTrackMetadata(readTrack)) {
            return playlistTrack;
        }

        readFileProperties(readTrack);
        readTrack.generateHash();
        m_state->progressScanned(readTrack.prettyFilepath());

        return readTrack;
    };

    if(auto* parser = m_playlistLoader->parserForExtension(u"cue"_s)) {
        TrackList tracks = parser->readPlaylist(&buffer, track.filepath(), {}, readEntry, false);
        for(auto& playlistTrack : tracks) {
            playlistTrack.generateHash();
        }
        return tracks;
    }

    return {};
}

void LibraryTrackResolver::readCue(const QString& cue, const bool onlyModified)
{
    readCue(QFileInfo{normalisePath(cue)}, onlyModified);
}

void LibraryTrackResolver::readCue(const QFileInfo& info, const bool onlyModified)
{
    const QString cuePath = normalisePath(info.absoluteFilePath());
    const QDateTime lastModifiedTime{info.lastModified()};
    const uint64_t lastModified
        = lastModifiedTime.isValid() ? static_cast<uint64_t>(lastModifiedTime.toMSecsSinceEpoch()) : 0;

    if(m_state->existingCueTracks().contains(cuePath) && onlyModified
       && m_state->existingCueTracks().at(cuePath).front().modifiedTime() >= lastModified) {
        m_state->markTracksSeen(m_state->existingCueTracks().at(cuePath));
        for(const auto& track : m_state->existingCueTracks().at(cuePath)) {
            m_state->cueFilesScanned().emplace(normalisePath(track.filepath()));
        }
        return;
    }

    const TrackList cueTracks = readPlaylistTracks(cuePath);
    if(cueTracks.empty()) {
        if(m_state->existingCueTracks().contains(cuePath)) {
            m_state->markTracksSeen(m_state->existingCueTracks().at(cuePath));
        }
        return;
    }

    std::unordered_map<QString, Track> existingCueTracksByPath;
    if(m_state->existingCueTracks().contains(cuePath)) {
        for(const auto& existingCueTrack : m_state->existingCueTracks().at(cuePath)) {
            existingCueTracksByPath.emplace(existingCueTrack.uniqueFilepath(), existingCueTrack);
        }
    }

    for(Track track : cueTracks) {
        const QString filepath = normalisePath(track.filepath());

        if(existingCueTracksByPath.contains(track.uniqueFilepath())) {
            const auto& existingTrack = existingCueTracksByPath.at(track.uniqueFilepath());
            track.setId(existingTrack.id());
            track.setLibraryId(existingTrack.libraryId());
            track.setAddedTime(existingTrack.addedTime());
            updateExistingTrack(track, filepath);
        }
        else if(const auto existingTrack = m_state->findExistingTrackByUniqueFilepath(track)) {
            track.setId(existingTrack->id());
            track.setLibraryId(existingTrack->libraryId());
            track.setAddedTime(existingTrack->addedTime());
            updateExistingTrack(track, filepath);
        }
        else if(const int existingTrackId = m_trackDatabase->idForTrack(track); existingTrackId >= 0) {
            track.setId(existingTrackId);
            updateExistingTrack(track, filepath);
        }
        else if(const Track movedTrack = m_state->matchRelocatedTrack(track);
                movedTrack.isInLibrary() || movedTrack.isInDatabase()) {
            track.setId(movedTrack.id());
            track.setLibraryId(movedTrack.libraryId());
            track.setAddedTime(movedTrack.addedTime());
            updateExistingTrack(track, filepath);
        }
        else {
            setTrackProps(track, filepath);
            m_writer->storeTrack(track);
        }

        m_state->cueFilesScanned().emplace(filepath);
        m_flushWrites();
    }
}

void LibraryTrackResolver::readFile(const QString& file, const bool onlyModified)
{
    readFile(QFileInfo{file}, onlyModified);
}

void LibraryTrackResolver::readFile(const QFileInfo& info, const bool onlyModified)
{
    const QString file = normalisePath(info.absoluteFilePath());

    if(!m_state->mayRun() || m_state->cueFilesScanned().contains(file)) {
        return;
    }

    const QDateTime lastModifiedTime{info.lastModified()};
    const uint64_t lastModified
        = lastModifiedTime.isValid() ? static_cast<uint64_t>(lastModifiedTime.toMSecsSinceEpoch()) : 0;

    if(m_state->trackPaths().contains(file)) {
        const auto& existingTracks = m_state->trackPaths().at(file);
        const Track& libraryTrack  = existingTracks.front();
        const bool needsRefresh    = !libraryTrack.isEnabled() || libraryTrack.libraryId() != m_currentLibrary.id
                                  || libraryTrack.modifiedTime() < lastModified || !onlyModified;

        if(!needsRefresh) {
            m_state->markTracksSeen(existingTracks);
            return;
        }

        std::unordered_map<QString, Track> existingByPath;
        for(const auto& track : existingTracks) {
            existingByPath.emplace(track.uniqueFilepath(), track);
        }

        const TrackList tracks = readTracks(file);
        if(tracks.empty()) {
            m_state->markTracksSeen(existingTracks);
            return;
        }

        for(Track track : tracks) {
            if(existingByPath.contains(track.uniqueFilepath())) {
                const auto& existingTrack = existingByPath.at(track.uniqueFilepath());
                track.setId(existingTrack.id());
                track.setLibraryId(existingTrack.libraryId());
                track.setAddedTime(existingTrack.addedTime());
            }

            if(lastModifiedTime.isValid()) {
                track.setModifiedTime(lastModified);
            }

            updateExistingTrack(track, file);
            m_flushWrites();
        }

        return;
    }

    if(m_state->existingArchives().contains(file)) {
        const auto& existingTracks = m_state->existingArchives().at(file);
        const Track& libraryTrack  = existingTracks.front();
        const bool needsRefresh    = !libraryTrack.isEnabled() || libraryTrack.libraryId() != m_currentLibrary.id
                                  || libraryTrack.modifiedTime() < lastModified || !onlyModified;

        if(!needsRefresh) {
            m_state->markTracksSeen(existingTracks);
            return;
        }

        std::unordered_map<QString, Track> existingByPath;
        for(const auto& track : existingTracks) {
            existingByPath.emplace(track.uniqueFilepath(), track);
        }

        const TrackList tracks = readArchiveTracks(file);
        if(tracks.empty()) {
            m_state->markTracksSeen(existingTracks);
            return;
        }

        for(Track track : tracks) {
            if(existingByPath.contains(track.uniqueFilepath())) {
                const auto& existingTrack = existingByPath.at(track.uniqueFilepath());
                track.setId(existingTrack.id());
                track.setLibraryId(existingTrack.libraryId());
                track.setAddedTime(existingTrack.addedTime());
            }

            updateExistingTrack(track, track.filepath());
            m_flushWrites();
        }

        return;
    }

    readNewTrack(file);
}

TrackList LibraryTrackResolver::readArchiveTracks(const QString& filepath)
{
    auto archiveReader = m_audioLoader->archiveReaderForFile(filepath);
    if(!archiveReader || !archiveReader->init(filepath)) {
        return {};
    }

    TrackList tracks;
    const QString type        = archiveReader->type();
    const QString archivePath = QLatin1String{ArchivePath}.arg(type).arg(filepath.size()).arg(filepath);
    const QFileInfo archiveInfo{filepath};
    const QDateTime archiveMTime = archiveInfo.lastModified();

    auto readEntry = [this, &tracks, archiveReader = archiveReader.get(), archiveMTime,
                      archivePath](ArchiveEntryData&& entryData) {
        if(!m_state->mayRun()) {
            return;
        }

        const QString& entry = entryData.path;
        QIODevice* device    = entryData.device.get();

        bool readersFound = false;
        for(auto& fileReader : m_audioLoader->readersForFile(entry)) {
            readersFound = true;

            AudioSource source;
            source.filepath      = entryData.path;
            source.device        = device;
            source.archiveReader = archiveReader;
            source.modifiedTime  = entryData.modifiedTime;
            source.size          = entryData.size;

            if(source.device && !source.device->seek(0)) {
                continue;
            }

            if(!fileReader->init(source)) {
                continue;
            }

            const int subsongCount = std::max(fileReader->subsongCount(), 1);

            for(int subIndex{0}; subIndex < subsongCount; ++subIndex) {
                if(!m_state->mayRun()) {
                    return;
                }

                Track subTrack{archivePath + entry, subIndex};

                if(source.size > 0) {
                    subTrack.setFileSize(source.size);
                }
                else if(source.device) {
                    subTrack.setFileSize(source.device->size());
                }

                if(source.device && !source.device->seek(0)) {
                    qCDebug(LIB_SCANNER) << "Failed to rewind archive entry:" << entry;
                    break;
                }

                // Use the archive file mtime for library change detection.
                // Comparing per-entry timestamps against the outer archive timestamp
                // makes unchanged archives look modified on every rescan.
                // TODO: Maybe store both archive and entry mtime?
                const uint64_t modifiedTime
                    = archiveMTime.isValid() ? static_cast<uint64_t>(archiveMTime.toMSecsSinceEpoch()) : 0;
                subTrack.setModifiedTime(modifiedTime);

                source.filepath = subTrack.filepath();

                if(fileReader->readTrack(source, subTrack)) {
                    subTrack.generateHash();
                    tracks.push_back(subTrack);
                    m_state->fileScanned(subTrack.prettyFilepath());
                }
            }
            return;
        }

        if(!readersFound) {
            qCDebug(LIB_SCANNER) << "Unsupported file:" << entry;
        }
    };

    if(archiveReader->readTracks(readEntry)) {
        qCDebug(LIB_SCANNER) << "Indexed" << tracks.size() << "tracks in" << filepath;
        return tracks;
    }

    return {};
}

void LibraryTrackResolver::setTrackProps(Track& track)
{
    setTrackProps(track, track.isInArchive() ? track.filepath() : normalisePath(track.filepath()));
}

void LibraryTrackResolver::setTrackProps(Track& track, const QString& file)
{
    readFileProperties(track);

    // Archive entries must keep their unpack URL so each entry remains unique in the database
    const QString resolvedFile = (track.isInArchive() && !Track::isArchivePath(file)) ? track.filepath() : file;
    track.setFilePath(resolvedFile);

    if(m_currentLibrary.id >= 0) {
        track.setLibraryId(m_currentLibrary.id);
    }

    track.generateHash();
    track.setIsEnabled(true);
}

void LibraryTrackResolver::updateExistingTrack(Track& track, const QString& file)
{
    setTrackProps(track, file);

    if(track.hasExtraTag(u"CUESHEET"_s)) {
        const TrackList cueTracks = readEmbeddedPlaylistTracks(track);
        for(Track cueTrack : cueTracks) {
            if(const auto existingCueTrack = m_state->findExistingTrackByUniqueFilepath(cueTrack)) {
                cueTrack.setId(existingCueTrack->id());
                cueTrack.setLibraryId(existingCueTrack->libraryId());
                cueTrack.setAddedTime(existingCueTrack->addedTime());
            }
            else if(const Track movedCueTrack = m_state->matchRelocatedTrack(cueTrack);
                    movedCueTrack.isInLibrary() || movedCueTrack.isInDatabase()) {
                cueTrack.setId(movedCueTrack.id());
                cueTrack.setLibraryId(movedCueTrack.libraryId());
                cueTrack.setAddedTime(movedCueTrack.addedTime());
            }

            setTrackProps(cueTrack, file);
            m_writer->updateTrack(cueTrack);
            m_state->markTrackSeen(cueTrack);
        }

        return;
    }

    if(track.id() < 0) {
        const int id = m_trackDatabase->idForTrack(track);
        if(id < 0) {
            qCWarning(LIB_SCANNER) << "Attempting to update track not in database:" << file;
        }
        else {
            track.setId(id);
        }
    }

    m_writer->updateTrack(track);
    m_state->markTrackSeen(track);
}

void LibraryTrackResolver::readNewTrack(const QString& file)
{
    qCDebug(LIB_SCANNER) << "Indexing new file:" << file;

    const TrackList tracks = readTracks(file);
    if(tracks.empty()) {
        return;
    }

    for(Track track : tracks) {
        if(const Track refoundTrack = m_state->matchRelocatedTrack(track);
           refoundTrack.isInLibrary() || refoundTrack.isInDatabase()) {
            track.setId(refoundTrack.id());
            track.setLibraryId(refoundTrack.libraryId());
            track.setAddedTime(refoundTrack.addedTime());
            updateExistingTrack(track, file);
            continue;
        }

        setTrackProps(track, file);
        track.setAddedTime(QDateTime::currentMSecsSinceEpoch());

        if(track.hasExtraTag(u"CUESHEET"_s)) {
            const TrackList cueTracks = readEmbeddedPlaylistTracks(track);
            for(Track cueTrack : cueTracks) {
                if(const Track refoundCueTrack = m_state->matchRelocatedTrack(cueTrack);
                   refoundCueTrack.isInLibrary() || refoundCueTrack.isInDatabase()) {
                    cueTrack.setId(refoundCueTrack.id());
                    cueTrack.setLibraryId(refoundCueTrack.libraryId());
                    cueTrack.setAddedTime(refoundCueTrack.addedTime());
                    updateExistingTrack(cueTrack, file);
                }
                else {
                    setTrackProps(cueTrack, file);
                    cueTrack.setAddedTime(track.addedTime());
                    m_writer->storeTrack(cueTrack);
                }
            }
        }
        else {
            m_writer->storeTrack(track);
        }
    }
}
} // namespace Fooyin
