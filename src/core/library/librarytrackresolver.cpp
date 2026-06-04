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

#include <core/coresettings.h>
#include <core/engine/audioloader.h>
#include <core/network/remoteioservice.h>
#include <core/playlist/playlistloader.h>
#include <core/playlist/playlistparser.h>
#include <core/trackmetadatastore.h>

#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QUrl>
#include <optional>
#include <utility>

Q_DECLARE_LOGGING_CATEGORY(LIB_SCANNER)

using namespace Qt::StringLiterals;

constexpr auto ArchivePath = R"(unpack://%1|%2|file://%3!)";

namespace {
void applyExistingTrackIdentity(Fooyin::Track& track, const Fooyin::Track& existingTrack)
{
    track.setId(existingTrack.id());
    track.setLibraryId(existingTrack.libraryId());
    track.setAddedTime(existingTrack.addedTime());
    track.setIsEnabled(existingTrack.isEnabled());
}

bool cueTracksUseEmbeddedCue(const Fooyin::TrackList& tracks)
{
    return std::ranges::any_of(tracks, [](const Fooyin::Track& track) { return track.hasExtraTag(u"CUESHEET"_s); });
}

bool isRemotePlaylistUrl(const QUrl& url)
{
    return url.isValid() && Fooyin::Track::isRemotePath(url.toString());
}

QString playlistExtension(const QUrl& url)
{
    return QFileInfo{isRemotePlaylistUrl(url) ? url.path() : url.toLocalFile()}.suffix().toLower();
}

} // namespace

namespace Fooyin {
LibraryTrackResolver::LibraryTrackResolver(LibraryInfo currentLibrary, PlaylistLoader* playlistLoader,
                                           AudioLoader* audioLoader, const bool playlistSkipMissing,
                                           std::shared_ptr<RemoteIoService> remoteIo,
                                           std::shared_ptr<TrackMetadataStore> metadataStore,
                                           TrackDatabase* trackDatabase, LibraryScanState* state,
                                           LibraryScanWriter* writer, const TrackReloadOptions reloadOptions,
                                           FlushWritesHandler flushWrites)
    : m_currentLibrary{std::move(currentLibrary)}
    , m_playlistLoader{playlistLoader}
    , m_audioLoader{audioLoader}
    , m_remoteIo{std::move(remoteIo)}
    , m_metadataStore{std::move(metadataStore)}
    , m_trackDatabase{trackDatabase}
    , m_state{state}
    , m_writer{writer}
    , m_reloadOptions{reloadOptions}
    , m_flushWrites{std::move(flushWrites)}
    , m_playlistSkipMissing{playlistSkipMissing}
{ }

LibraryTrackResolver::LibraryTrackResolver(LibraryInfo currentLibrary, PlaylistLoader* playlistLoader,
                                           AudioLoader* audioLoader, const bool playlistSkipMissing,
                                           std::shared_ptr<TrackMetadataStore> metadataStore,
                                           TrackDatabase* trackDatabase, LibraryScanState* state,
                                           LibraryScanWriter* writer, const TrackReloadOptions reloadOptions,
                                           FlushWritesHandler flushWrites)
    : LibraryTrackResolver{std::move(currentLibrary),
                           playlistLoader,
                           audioLoader,
                           playlistSkipMissing,
                           nullptr,
                           std::move(metadataStore),
                           trackDatabase,
                           state,
                           writer,
                           reloadOptions,
                           std::move(flushWrites)}
{ }

TrackList LibraryTrackResolver::readTracks(const QString& filepath)
{
    if(m_audioLoader->isArchive(filepath)) {
        return readArchiveTracks(filepath);
    }

    const Track track{filepath, m_metadataStore};

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

        Track subTrack{filepath, subIndex, m_metadataStore};

        if(source.device) {
            subTrack.setFileSize(source.device->size());
            if(!source.device->seek(0)) {
                qCDebug(LIB_SCANNER) << "Failed to rewind file:" << filepath;
                break;
            }
        }

        if(loadedReader.reader->readTrack(source, subTrack)) {
            subTrack.setMetadataWasRead(true);
            subTrack.generateHash();
            tracks.push_back(subTrack);
        }
    }

    return tracks;
}

TrackList LibraryTrackResolver::readPlaylist(const QString& filepath)
{
    return readPlaylist(QUrl::fromLocalFile(filepath));
}

TrackList LibraryTrackResolver::readPlaylist(const QUrl& url)
{
    TrackList tracks;

    const TrackList playlistTracks = readPlaylistTracks(url);
    for(Track playlistTrack : playlistTracks) {
        if(const auto existingTrack = m_state->findExistingTrackByUniqueFilepath(playlistTrack)) {
            applyExistingTrackIdentity(playlistTrack, existingTrack.value());
            mergeReloadedTrackStats(playlistTrack, existingTrack.value(), m_reloadOptions);
        }

        playlistTrack.generateHash();
        tracks.push_back(playlistTrack);
    }

    return tracks;
}

std::shared_ptr<RemoteDownloadHandle>
LibraryTrackResolver::readPlaylistAsync(const QUrl& url, QObject* context,
                                        std::function<void(bool completed, TrackList tracks)> callback)
{
    if(!isRemotePlaylistUrl(url)) {
        callback(true, readPlaylist(url));
        return {};
    }

    if(!url.isValid()) {
        callback(false, {});
        return {};
    }

    const QString extension = playlistExtension(url);
    auto* parser            = m_playlistLoader->parserForExtension(extension);
    if(!parser) {
        callback(false, {});
        return {};
    }

    auto readEntry       = std::make_shared<PlaylistParser::ReadPlaylistEntry>();
    readEntry->readTrack = [this, readEntry](const Track& playlistTrack) {
        if(!m_state->mayRun()) {
            readEntry->cancel = true;
            return playlistTrack;
        }

        Track readTrack{playlistTrack};
        readTrack.setMetadataStore(m_metadataStore);

        if(playlistTrack.isRemote()) {
            return readTrack;
        }

        readFileProperties(readTrack);

        if(!m_audioLoader->readTrackMetadata(readTrack)) {
            return playlistTrack;
        }

        readTrack.generateHash();
        m_state->progressScanned(readTrack.prettyFilepath());

        return readTrack;
    };
    readEntry->canLoadTrack = [this](const Track& playlistTrack) {
        if(playlistTrack.isRemote()) {
            return true;
        }
        return static_cast<bool>(m_audioLoader->loadDecoderForTrack(playlistTrack).decoder);
    };

    return m_remoteIo->download(
        url, context,
        [this, parser, url, readEntry, callback = std::move(callback)](std::optional<QByteArray> data,
                                                                       const QString& error) mutable {
            if(!data) {
                qCInfo(LIB_SCANNER) << "Could not download playlist" << url << "for reading:" << error;
                callback(false, {});
                return;
            }

            QBuffer buffer{&data.value()};
            if(!buffer.open(QIODevice::ReadOnly)) {
                callback(false, {});
                return;
            }

            TrackList parsedTracks
                = parser->readPlaylist(&buffer, url.toString(), {}, *readEntry, m_playlistSkipMissing);
            TrackList tracks;
            tracks.reserve(parsedTracks.size());

            for(Track playlistTrack : parsedTracks) {
                if(const auto existingTrack = m_state->findExistingTrackByUniqueFilepath(playlistTrack)) {
                    applyExistingTrackIdentity(playlistTrack, existingTrack.value());
                    mergeReloadedTrackStats(playlistTrack, existingTrack.value(), m_reloadOptions);
                }

                playlistTrack.generateHash();
                tracks.push_back(playlistTrack);
            }

            for(auto& track : tracks) {
                track.setMetadataStore(m_metadataStore);
            }

            callback(m_state->mayRun(), std::move(tracks));
        });
}

size_t LibraryTrackResolver::countPlaylistTracks(const QString& path) const
{
    return countPlaylistTracks(QUrl::fromLocalFile(path));
}

size_t LibraryTrackResolver::countPlaylistTracks(const QUrl& url) const
{
    if(!url.isValid()) {
        return 0;
    }

    const QString extension = playlistExtension(url);
    auto* parser            = m_playlistLoader->parserForExtension(extension);
    if(!parser) {
        return 0;
    }

    if(isRemotePlaylistUrl(url)) {
        return 0;
    }

    const QString path = url.toLocalFile();
    if(path.isEmpty()) {
        return 0;
    }

    QFile playlistFile{path};
    if(!playlistFile.open(QIODevice::ReadOnly)) {
        return 0;
    }

    QDir dir{path};
    dir.cdUp();

    return parser->countEntries(&playlistFile, path, dir);
}

TrackList LibraryTrackResolver::readPlaylistTracks(const QString& path)
{
    return readPlaylistTracks(QUrl::fromLocalFile(path));
}

TrackList LibraryTrackResolver::readPlaylistTracks(const QUrl& url)
{
    if(!url.isValid()) {
        return {};
    }

    const QString extension = playlistExtension(url);
    auto* parser            = m_playlistLoader->parserForExtension(extension);
    if(!parser) {
        return {};
    }

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [this, &readEntry](const Track& playlistTrack) {
        if(!m_state->mayRun()) {
            readEntry.cancel = true;
            return playlistTrack;
        }

        Track readTrack{playlistTrack};
        readTrack.setMetadataStore(m_metadataStore);

        if(playlistTrack.isRemote()) {
            return readTrack;
        }

        readFileProperties(readTrack);

        if(!m_audioLoader->readTrackMetadata(readTrack)) {
            return playlistTrack;
        }

        readTrack.generateHash();
        m_state->progressScanned(readTrack.prettyFilepath());

        return readTrack;
    };
    readEntry.canLoadTrack = [this](const Track& playlistTrack) {
        if(playlistTrack.isRemote()) {
            return true;
        }
        return static_cast<bool>(m_audioLoader->loadDecoderForTrack(playlistTrack).decoder);
    };

    if(isRemotePlaylistUrl(url)) {
        return {};
    }

    const QString path = url.toLocalFile();
    if(path.isEmpty()) {
        return {};
    }

    QFile playlistFile{path};
    if(!playlistFile.open(QIODevice::ReadOnly)) {
        qCInfo(LIB_SCANNER) << "Could not open file" << path << "for reading:" << playlistFile.errorString();
        return {};
    }

    QDir dir{path};
    dir.cdUp();

    TrackList tracks = parser->readPlaylist(&playlistFile, path, dir, readEntry, m_playlistSkipMissing);
    for(auto& track : tracks) {
        track.setMetadataStore(m_metadataStore);
    }
    return tracks;
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
        readTrack.setMetadataStore(m_metadataStore);
        if(!m_audioLoader->readTrackMetadata(readTrack)) {
            return playlistTrack;
        }

        readFileProperties(readTrack);
        readTrack.generateHash();
        m_state->progressScanned(readTrack.prettyFilepath());

        return readTrack;
    };
    readEntry.canLoadTrack = [this](const Track& playlistTrack) {
        return static_cast<bool>(m_audioLoader->loadDecoderForTrack(playlistTrack).decoder);
    };

    if(auto* parser = m_playlistLoader->parserForExtension(u"cue"_s)) {
        TrackList tracks = parser->readPlaylist(&buffer, track.filepath(), {}, readEntry, false);
        for(auto& playlistTrack : tracks) {
            playlistTrack.setMetadataStore(m_metadataStore);
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

    const TrackList cueTracks = readPlaylistTracks(cuePath);
    if(cueTracks.empty()) {
        if(m_state->existingCueTracks().contains(cuePath)) {
            m_state->markTracksSeen(m_state->existingCueTracks().at(cuePath));
        }
        return;
    }

    if(cueTracksUseEmbeddedCue(cueTracks)) {
        qCInfo(LIB_SCANNER) << "Skipping local cue in favour of embedded cue:" << cuePath;
        return;
    }

    if(m_state->existingCueTracks().contains(cuePath) && onlyModified
       && m_state->existingCueTracks().at(cuePath).front().modifiedTime() >= lastModified) {
        m_state->markTracksSeen(m_state->existingCueTracks().at(cuePath));
        for(const auto& track : m_state->existingCueTracks().at(cuePath)) {
            m_state->cueFilesScanned().emplace(normalisePath(track.filepath()));
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
            applyExistingTrackIdentity(track, existingTrack);
            mergeReloadedTrackStats(track, existingTrack, m_reloadOptions);
            updateExistingTrack(track, filepath);
        }
        else if(const auto existingTrack = m_state->findExistingTrackByUniqueFilepath(track)) {
            applyExistingTrackIdentity(track, *existingTrack);
            mergeReloadedTrackStats(track, *existingTrack, m_reloadOptions);
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
                applyExistingTrackIdentity(track, existingTrack);
                mergeReloadedTrackStats(track, existingTrack, m_reloadOptions);
            }

            const QDateTime createdTime{info.birthTime()};
            if(createdTime.isValid()) {
                track.setCreatedTime(static_cast<uint64_t>(createdTime.toMSecsSinceEpoch()));
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
                applyExistingTrackIdentity(track, existingTrack);
                mergeReloadedTrackStats(track, existingTrack, m_reloadOptions);
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

        const QString& entry = entryData.info.path;
        QIODevice* device    = entryData.device.get();

        bool readersFound = false;
        for(auto& fileReader : m_audioLoader->readersForFile(entry)) {
            readersFound = true;

            AudioSource source;
            source.filepath      = entryData.info.path;
            source.device        = device;
            source.archiveReader = archiveReader;
            source.modifiedTime  = entryData.info.modifiedTime;
            source.size          = entryData.info.size;

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

                Track subTrack{archivePath + entry, subIndex, m_metadataStore};

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
                    subTrack.setMetadataWasRead(true);
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
                applyExistingTrackIdentity(cueTrack, *existingCueTrack);
                mergeReloadedTrackStats(cueTrack, *existingCueTrack, m_reloadOptions);
            }
            else if(const Track movedCueTrack = m_state->matchRelocatedTrack(cueTrack);
                    movedCueTrack.isInLibrary() || movedCueTrack.isInDatabase()) {
                applyExistingTrackIdentity(cueTrack, movedCueTrack);
                mergeReloadedTrackStats(cueTrack, movedCueTrack, m_reloadOptions);
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
            applyExistingTrackIdentity(track, refoundTrack);
            mergeReloadedTrackStats(track, refoundTrack, m_reloadOptions);
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
                    applyExistingTrackIdentity(cueTrack, refoundCueTrack);
                    mergeReloadedTrackStats(cueTrack, refoundCueTrack, m_reloadOptions);
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
