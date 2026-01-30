/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "libraryscanner.h"

#include "database/trackdatabase.h"
#include "internalcoresettings.h"
#include "librarywatcher.h"
#include "playlist/playlistloader.h"

#include <core/coresettings.h>
#include <core/library/libraryinfo.h>
#include <core/playlist/playlist.h>
#include <core/playlist/playlistparser.h>
#include <core/track.h>
#include <utils/database/dbconnectionhandler.h>
#include <utils/database/dbconnectionpool.h>
#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>
#include <utils/timer.h>
#include <utils/utils.h>

#include <QBuffer>
#include <QDir>
#include <QDirIterator>
#include <QFileSystemWatcher>
#include <QHash>
#include <QLoggingCategory>
#include <QRegularExpression>

#include <ranges>

Q_LOGGING_CATEGORY(LIB_SCANNER, "fy.scanner")

using namespace Qt::StringLiterals;

constexpr auto BatchSize   = 250;
constexpr auto ArchivePath = R"(unpack://%1|%2|file://%3!)";

namespace {
void sortFiles(QFileInfoList& files)
{
    std::ranges::sort(files, {}, &QFileInfo::filePath);
    std::ranges::stable_sort(files, [](const QFileInfo& a, const QFileInfo& b) {
        const bool aIsCue = a.suffix().compare("cue"_L1, Qt::CaseInsensitive) == 0;
        const bool bIsCue = b.suffix().compare("cue"_L1, Qt::CaseInsensitive) == 0;
        if(aIsCue && !bIsCue) {
            return true;
        }
        if(!aIsCue && bIsCue) {
            return false;
        }
        return false;
    });
}

std::optional<QFileInfo> findMatchingCue(const QFileInfo& file)
{
    static const QStringList cueExtensions{u"*.cue"_s};

    const QDir dir           = file.absoluteDir();
    const QFileInfoList cues = dir.entryInfoList(cueExtensions, QDir::Files);

    for(const auto& cue : cues) {
        if(cue.completeBaseName() == file.completeBaseName() || cue.fileName().contains(file.fileName())) {
            return cue;
        }
    }

    return {};
}

QFileInfoList getFiles(const QStringList& paths, const QStringList& restrictExtensions,
                       const QStringList& excludeExtensions, const QStringList& playlistExtensions)
{
    const Fooyin::Timer timer;

    QFileInfoList files;

    QStringList nameFilters{restrictExtensions};
    QStringList playlistFilters{playlistExtensions};

    for(const auto& ext : excludeExtensions) {
        nameFilters.removeAll(ext);
        playlistFilters.removeAll(ext);
    }

    for(const QString& path : paths) {
        const QFileInfo file{path};

        if(files.contains(file)) {
            continue;
        }

        const QString suffix = file.suffix().toLower();

        if(file.isDir()) {
            QDirIterator dirIt{file.absoluteFilePath(), Fooyin::Utils::extensionsToWildcards(nameFilters), QDir::Files,
                               QDirIterator::Subdirectories | QDirIterator::FollowSymlinks};
            while(dirIt.hasNext()) {
                dirIt.next();
                const QFileInfo info = dirIt.fileInfo();
                if(info.size() > 0) {
                    files.append(info);
                }
            }
        }
        else {
            if(playlistFilters.contains(suffix)) {
                files.emplace_back(file.absoluteFilePath());
            }
            else if(nameFilters.contains(suffix)) {
                if(const auto cue = findMatchingCue(file)) {
                    files.emplace_back(cue.value());
                }
                files.emplace_back(file.absoluteFilePath());
            }
        }
    }

    sortFiles(files);

    qCInfo(LIB_SCANNER) << "Found" << files.size() << "files in" << timer.elapsedFormatted();

    return files;
}

void readFileProperties(Fooyin::Track& track)
{
    const QFileInfo fileInfo{track.filepath()};

    if(track.addedTime() == 0) {
        track.setAddedTime(QDateTime::currentMSecsSinceEpoch());
    }
    if(track.modifiedTime() == 0) {
        const QDateTime modifiedTime = fileInfo.lastModified();
        track.setModifiedTime(modifiedTime.isValid() ? modifiedTime.toMSecsSinceEpoch() : 0);
    }
    if(track.fileSize() == 0) {
        track.setFileSize(fileInfo.size());
    }
}

void applyCueTrackTags(Fooyin::TrackList& tracks)
{
    static const QRegularExpression cueTrackRegex{u"^CUE_TRACK(\\d+)_(.+)$"_s,
                                                  QRegularExpression::CaseInsensitiveOption};

    for(Fooyin::Track& track : tracks) {
        // Normalize track number to 2 digits for comparison
        const QString thisTrackNum = track.trackNumber().rightJustified(2, u'0');

        QStringList tagsToRemove;
        const auto extraTags = track.extraTags();

        for(auto it = extraTags.cbegin(); it != extraTags.cend(); ++it) {
            const QString& tagName              = it.key();
            const QRegularExpressionMatch match = cueTrackRegex.match(tagName);

            if(match.hasMatch()) {
                tagsToRemove.append(tagName);

                // Normalize tag's track number and compare (handles both CUE_TRACK1_* and CUE_TRACK01_*)
                const QString tagTrackNum = match.captured(1).rightJustified(2, u'0');
                if(tagTrackNum == thisTrackNum) {
                    const QString field       = match.captured(2).toUpper();
                    const QStringList& values = it.value();

                    if(field == "COMPOSER"_L1) {
                        track.setComposers(values);
                    }
                    else if(field == "PERFORMER"_L1) {
                        track.setPerformers(values);
                    }
                    else if(field == "ARTIST"_L1) {
                        track.setArtists(values);
                    }
                    else if(field == "GENRE"_L1) {
                        track.setGenres(values);
                    }
                    else if(!values.isEmpty()) {
                        // Store other fields as extra tags with just the field name
                        track.replaceExtraTag(field, values);
                    }
                }
            }
        }

        // Remove all Cue_track* tags from extra tags
        for(const QString& tag : tagsToRemove) {
            track.removeExtraTag(tag);
        }

        // Remove parent file metadata that doesn't apply to individual cue tracks
        track.removeExtraTag(u"CUESHEET"_s);
        track.removeExtraTag(u"TOTALTRACKS"_s);
    }
}
} // namespace

namespace Fooyin {
class LibraryScannerPrivate
{
public:
    LibraryScannerPrivate(LibraryScanner* self, DbConnectionPoolPtr dbPool,
                          std::shared_ptr<PlaylistLoader> playlistLoader, std::shared_ptr<AudioLoader> audioLoader,
                          SettingsManager* settings)
        : m_self{self}
        , m_settings{settings}
        , m_dbPool{std::move(dbPool)}
        , m_playlistLoader{std::move(playlistLoader)}
        , m_audioLoader{std::move(audioLoader)}
    { }

    void finishScan();
    void cleanupScan();

    void addWatcher(const Fooyin::LibraryInfo& library);

    void reportProgress(const QString& file) const;
    void fileScanned(const QString& file);

    Track matchMissingTrack(const Track& track);

    void checkBatchFinished();
    void removeMissingTrack(const Track& track);

    [[nodiscard]] TrackList readTracks(const QString& filepath);
    [[nodiscard]] TrackList readArchiveTracks(const QString& filepath);
    [[nodiscard]] TrackList readPlaylist(const QString& filepath);
    [[nodiscard]] TrackList readPlaylistTracks(const QString& filepath);
    [[nodiscard]] TrackList readEmbeddedPlaylistTracks(const Track& track);

    void updateExistingCueTracks(const TrackList& tracks, const QString& cue);
    void addNewCueTracks(const QString& cue, const QString& filename);
    void readCue(const QString& cue, bool onlyModified);

    void setTrackProps(Track& track);
    void setTrackProps(Track& track, const QString& file);

    void updateExistingTrack(Track& track, const QString& file);
    void readNewTrack(const QString& file);

    void readFile(const QString& file, bool onlyModified);
    void populateExistingTracks(const TrackList& tracks, bool includeMissing = true);
    bool getAndSaveAllTracks(const QStringList& paths, const TrackList& tracks, bool onlyModified);

    void changeLibraryStatus(LibraryInfo::Status status);

    LibraryScanner* m_self;

    SettingsManager* m_settings;
    DbConnectionPoolPtr m_dbPool;
    std::shared_ptr<PlaylistLoader> m_playlistLoader;
    std::shared_ptr<AudioLoader> m_audioLoader;

    std::unique_ptr<DbConnectionHandler> m_dbHandler;

    bool m_monitor{false};
    LibraryInfo m_currentLibrary;
    TrackDatabase m_trackDatabase;

    TrackList m_tracksToStore;
    TrackList m_tracksToUpdate;

    QHash<QString, TrackList> m_trackPaths;
    QHash<QString, TrackList> m_existingArchives;
    QHash<QString, TrackList> m_missingFiles;
    QHash<QString, Track> m_missingHashes;
    QHash<QString, TrackList> m_existingCueTracks;
    QHash<QString, TrackList> m_missingCueTracks;
    std::set<QString> m_cueFilesScanned;

    std::set<QString> m_filesScanned;
    size_t m_totalFiles{0};

    std::unordered_map<int, LibraryWatcher> m_watchers;
};

void LibraryScannerPrivate::finishScan()
{
    if(m_self->state() != LibraryScanner::Paused) {
        m_self->setState(LibraryScanner::Idle);
        m_totalFiles = m_filesScanned.size();
        reportProgress({});
        cleanupScan();
        emit m_self->finished();
    }
}

void LibraryScannerPrivate::cleanupScan()
{
    m_filesScanned.clear();
    m_totalFiles = 0;
    m_tracksToStore.clear();
    m_tracksToUpdate.clear();
    m_trackPaths.clear();
    m_existingArchives.clear();
    m_missingFiles.clear();
    m_missingHashes.clear();
    m_existingCueTracks.clear();
    m_missingCueTracks.clear();
    m_cueFilesScanned.clear();
}

void LibraryScannerPrivate::addWatcher(const LibraryInfo& library)
{
    auto watchPaths = [this, library](const QString& path) {
        QStringList dirs = Utils::File::getAllSubdirectories(path);
        dirs.append(path);
        m_watchers[library.id].addPaths(dirs);
    };

    watchPaths(library.path);

    QObject::connect(&m_watchers.at(library.id), &LibraryWatcher::libraryDirsChanged, m_self,
                     [this, watchPaths, library](const QStringList& dirs) {
                         std::ranges::for_each(dirs, watchPaths);
                         emit m_self->directoriesChanged(library, dirs);
                     });
}

void LibraryScannerPrivate::reportProgress(const QString& file) const
{
    emit m_self->progressChanged(static_cast<int>(m_filesScanned.size()), file, static_cast<int>(m_totalFiles));
}

void LibraryScannerPrivate::fileScanned(const QString& file)
{
    m_filesScanned.emplace(file);
    reportProgress(file);
}

Track LibraryScannerPrivate::matchMissingTrack(const Track& track)
{
    const QString filename = track.filename();
    const QString hash     = track.hash();

    if(m_missingFiles.contains(filename)) {
        for(const auto& file : m_missingFiles.value(filename)) {
            if(file.hash() == hash) {
                return file;
            }
        }
    }

    if(m_missingHashes.contains(hash) && m_missingHashes.value(hash).duration() == track.duration()) {
        return m_missingHashes.value(hash);
    }

    return {};
}

void LibraryScannerPrivate::checkBatchFinished()
{
    if(m_tracksToStore.size() >= BatchSize || m_tracksToUpdate.size() > BatchSize) {
        if(m_tracksToStore.size() >= BatchSize) {
            m_trackDatabase.storeTracks(m_tracksToStore);
        }
        if(m_tracksToUpdate.size() >= BatchSize) {
            m_trackDatabase.updateTracks(m_tracksToUpdate);
        }
        emit m_self->scanUpdate({.addedTracks = m_tracksToStore, .updatedTracks = m_tracksToUpdate});
        m_tracksToStore.clear();
        m_tracksToUpdate.clear();
    }
}

void LibraryScannerPrivate::removeMissingTrack(const Track& track)
{
    if(m_missingFiles.contains(track.filename())) {
        auto& missingTracks = m_missingFiles[track.filename()];
        std::erase_if(missingTracks,
                      [&track](const Track& missingTrack) { return missingTrack.hash() == track.hash(); });
        if(missingTracks.empty()) {
            m_missingFiles.remove(track.filename());
        }
    }
}

TrackList LibraryScannerPrivate::readTracks(const QString& filepath)
{
    if(m_audioLoader->isArchive(filepath)) {
        return readArchiveTracks(filepath);
    }

    auto tagReader = m_audioLoader->readerForFile(filepath);
    if(!tagReader) {
        return {};
    }

    QFile file{filepath};
    if(!file.open(QIODevice::ReadOnly)) {
        qCInfo(LIB_SCANNER) << "Failed to open file:" << filepath;
        return {};
    }
    const AudioSource source{filepath, &file, nullptr};

    if(!tagReader->init(source)) {
        qCDebug(LIB_SCANNER) << "Unsupported file:" << filepath;
        return {};
    }

    TrackList tracks;
    const int subsongCount = std::max(tagReader->subsongCount(), 1);

    for(int subIndex{0}; subIndex < subsongCount; ++subIndex) {
        Track subTrack{filepath, subIndex};
        subTrack.setFileSize(file.size());

        source.device->seek(0);
        if(tagReader->readTrack(source, subTrack)) {
            subTrack.generateHash();
            tracks.push_back(subTrack);
        }
    }

    return tracks;
}

TrackList LibraryScannerPrivate::readArchiveTracks(const QString& filepath)
{
    auto archiveReader = m_audioLoader->archiveReaderForFile(filepath);
    if(!archiveReader) {
        return {};
    }

    if(!archiveReader->init(filepath)) {
        return {};
    }

    TrackList tracks;
    const QString type        = archiveReader->type();
    const QString archivePath = QLatin1String{ArchivePath}.arg(type).arg(filepath.size()).arg(filepath);
    const QFileInfo archiveInfo{filepath};
    const QDateTime modifiedTime = archiveInfo.lastModified();

    auto readEntry = [&](const QString& entry, QIODevice* device) {
        if(!m_self->mayRun()) {
            return;
        }

        auto fileReader = m_audioLoader->readerForFile(entry);
        if(!fileReader) {
            qCDebug(LIB_SCANNER) << "Unsupported file:" << entry;
            return;
        }

        if(!device->open(QIODevice::ReadOnly)) {
            qCInfo(LIB_SCANNER) << "Failed to open file:" << entry;
            return;
        }

        AudioSource source;
        source.filepath      = entry;
        source.device        = device;
        source.archiveReader = archiveReader.get();

        if(!fileReader->init(source)) {
            qCDebug(LIB_SCANNER) << "Unsupported file:" << entry;
            return;
        }

        const int subsongCount = std::max(fileReader->subsongCount(), 1);
        m_totalFiles += subsongCount;

        for(int subIndex{0}; subIndex < subsongCount; ++subIndex) {
            Track subTrack{archivePath + entry, subIndex};
            subTrack.setFileSize(device->size());
            subTrack.setModifiedTime(modifiedTime.isValid() ? modifiedTime.toMSecsSinceEpoch() : 0);
            source.filepath = subTrack.filepath();

            device->seek(0);
            if(fileReader->readTrack(source, subTrack)) {
                subTrack.generateHash();
                tracks.push_back(subTrack);
                fileScanned(subTrack.prettyFilepath());
            }
        }
    };

    if(archiveReader->readTracks(readEntry)) {
        qCDebug(LIB_SCANNER) << "Indexed" << tracks.size() << "tracks in" << filepath;
        return tracks;
    }

    return {};
}

TrackList LibraryScannerPrivate::readPlaylist(const QString& filepath)
{
    TrackList tracks;

    const TrackList playlistTracks = readPlaylistTracks(filepath);
    for(const Track& playlistTrack : playlistTracks) {
        const auto trackKey = playlistTrack.filepath();

        if(m_trackPaths.contains(trackKey)) {
            const auto existingTracks = m_trackPaths.value(trackKey);
            for(const Track& track : existingTracks) {
                if(track.uniqueFilepath() == playlistTrack.uniqueFilepath()) {
                    tracks.push_back(track);
                    break;
                }
            }
        }
        else {
            Track track{playlistTrack};
            track.generateHash();
            tracks.push_back(track);
        }
    }

    return tracks;
}

TrackList LibraryScannerPrivate::readPlaylistTracks(const QString& path)
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
        if(!m_self->mayRun()) {
            readEntry.cancel = true;
            return playlistTrack;
        }

        Track readTrack{playlistTrack};
        readFileProperties(readTrack);

        if(!m_audioLoader->readTrackMetadata(readTrack)) {
            return playlistTrack;
        }

        readTrack.generateHash();

        ++m_totalFiles;
        fileScanned(readTrack.prettyFilepath());

        return readTrack;
    };

    if(auto* parser = m_playlistLoader->parserForExtension(info.suffix())) {
        return parser->readPlaylist(&playlistFile, path, dir, readEntry,
                                    m_settings->value<Settings::Core::PlaylistSkipMissing>());
    }

    return {};
}

TrackList LibraryScannerPrivate::readEmbeddedPlaylistTracks(const Track& track)
{
    const auto cues = track.extraTag(u"CUESHEET"_s);
    QByteArray bytes{cues.front().toUtf8()};
    QBuffer buffer(&bytes);
    if(!buffer.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCInfo(LIB_SCANNER) << "Can't open buffer for reading:" << buffer.errorString();
        return {};
    }

    PlaylistParser::ReadPlaylistEntry readEntry;
    readEntry.readTrack = [this, &readEntry](const Track& playlistTrack) {
        if(!m_self->mayRun()) {
            readEntry.cancel = true;
            return playlistTrack;
        }

        Track readTrack{playlistTrack};

        if(!m_audioLoader->readTrackMetadata(readTrack)) {
            return playlistTrack;
        }

        readFileProperties(readTrack);
        readTrack.generateHash();

        ++m_totalFiles;
        fileScanned(readTrack.prettyFilepath());

        return readTrack;
    };

    if(auto* parser = m_playlistLoader->parserForExtension(u"cue"_s)) {
        TrackList tracks = parser->readPlaylist(&buffer, track.filepath(), {}, readEntry, false);
        applyCueTrackTags(tracks);
        for(auto& plTrack : tracks) {
            plTrack.generateHash();
        }
        return tracks;
    }

    return {};
}

void LibraryScannerPrivate::updateExistingCueTracks(const TrackList& tracks, const QString& cue)
{
    QHash<QString, Track> existingTrackPaths;
    for(const Track& track : tracks) {
        existingTrackPaths.insert(track.uniqueFilepath(), track);
    }

    const TrackList cueTracks = readPlaylistTracks(cue);
    for(const Track& cueTrack : cueTracks) {
        Track track{cueTrack};
        if(existingTrackPaths.contains(track.uniqueFilepath())) {
            track.setId(existingTrackPaths.value(track.uniqueFilepath()).id());
        }
        setTrackProps(track);
        m_tracksToUpdate.push_back(track);
        m_cueFilesScanned.emplace(track.filepath());
    }
}

void LibraryScannerPrivate::addNewCueTracks(const QString& cue, const QString& filename)
{
    if(m_missingCueTracks.contains(filename)) {
        TrackList refoundCueTracks = m_missingCueTracks.value(cue);
        for(Track& track : refoundCueTracks) {
            track.setCuePath(cue);
            m_tracksToUpdate.push_back(track);
        }
    }
    else {
        const TrackList cueTracks = readPlaylistTracks(cue);
        for(const Track& cueTrack : cueTracks) {
            Track track{cueTrack};
            setTrackProps(track);
            m_tracksToStore.push_back(track);
            m_cueFilesScanned.emplace(track.filepath());
        }
    }
}

void LibraryScannerPrivate::readCue(const QString& cue, bool onlyModified)
{
    const QFileInfo info{cue};
    const QDateTime lastModifiedTime{info.lastModified()};
    uint64_t lastModified{0};

    if(lastModifiedTime.isValid()) {
        lastModified = static_cast<uint64_t>(lastModifiedTime.toMSecsSinceEpoch());
    }

    if(m_existingCueTracks.contains(cue)) {
        const auto& tracks = m_existingCueTracks.value(cue);
        if(tracks.front().modifiedTime() < lastModified || !onlyModified) {
            updateExistingCueTracks(tracks, cue);
        }
        else {
            for(const Track& track : tracks) {
                m_cueFilesScanned.emplace(track.filepath());
            }
        }
    }
    else {
        addNewCueTracks(cue, info.fileName());
    }
}

void LibraryScannerPrivate::setTrackProps(Track& track)
{
    setTrackProps(track, track.filepath());
}

void LibraryScannerPrivate::setTrackProps(Track& track, const QString& file)
{
    readFileProperties(track);
    track.setFilePath(file);

    if(m_currentLibrary.id >= 0) {
        track.setLibraryId(m_currentLibrary.id);
    }
    track.generateHash();
    track.setIsEnabled(true);
}

void LibraryScannerPrivate::updateExistingTrack(Track& track, const QString& file)
{
    setTrackProps(track, file);
    removeMissingTrack(track);

    if(track.id() < 0) {
        const int id = m_trackDatabase.idForTrack(track);
        if(id < 0) {
            qCWarning(LIB_SCANNER) << "Attempting to update track not in database:" << file;
        }
        else {
            track.setId(id);
        }
    }

    if(track.hasExtraTag(u"CUESHEET"_s)) {
        QHash<QString, Track> existingTrackPaths;
        if(m_existingCueTracks.contains(track.filepath())) {
            const auto& tracks = m_existingCueTracks.value(track.filepath());
            for(const Track& existingTrack : tracks) {
                existingTrackPaths.insert(existingTrack.uniqueFilepath(), existingTrack);
            }
        }

        TrackList cueTracks = readEmbeddedPlaylistTracks(track);
        for(Track& cueTrack : cueTracks) {
            if(existingTrackPaths.contains(cueTrack.uniqueFilepath())) {
                cueTrack.setId(existingTrackPaths.value(cueTrack.uniqueFilepath()).id());
            }
            setTrackProps(cueTrack, file);
            m_tracksToUpdate.push_back(cueTrack);
            m_missingHashes.remove(cueTrack.hash());
        }
    }
    else {
        m_tracksToUpdate.push_back(track);
        m_missingHashes.remove(track.hash());
    }
}

void LibraryScannerPrivate::readNewTrack(const QString& file)
{
    qCDebug(LIB_SCANNER) << "Indexing new file:" << file;

    TrackList tracks = readTracks(file);
    if(tracks.empty()) {
        return;
    }

    for(Track& track : tracks) {
        Track refoundTrack = matchMissingTrack(track);
        if(refoundTrack.isInLibrary() || refoundTrack.isInDatabase()) {
            m_missingHashes.remove(refoundTrack.hash());
            removeMissingTrack(refoundTrack);

            setTrackProps(refoundTrack, file);
            m_tracksToUpdate.push_back(refoundTrack);
        }
        else {
            setTrackProps(track);
            track.setAddedTime(QDateTime::currentMSecsSinceEpoch());

            if(track.hasExtraTag(u"CUESHEET"_s)) {
                TrackList cueTracks = readEmbeddedPlaylistTracks(track);
                for(Track& cueTrack : cueTracks) {
                    setTrackProps(cueTrack, file);
                    m_tracksToStore.push_back(cueTrack);
                }
            }
            else {
                m_tracksToStore.push_back(track);
            }
        }
    }
}

void LibraryScannerPrivate::readFile(const QString& file, bool onlyModified)
{
    if(!m_self->mayRun()) {
        return;
    }

    if(m_cueFilesScanned.contains(file)) {
        return;
    }

    const QFileInfo info{file};
    const QDateTime lastModifiedTime{info.lastModified()};
    uint64_t lastModified{0};

    if(lastModifiedTime.isValid()) {
        lastModified = static_cast<uint64_t>(lastModifiedTime.toMSecsSinceEpoch());
    }

    if(m_trackPaths.contains(file)) {
        const Track& libraryTrack = m_trackPaths[file].front();

        if(!libraryTrack.isEnabled() || libraryTrack.libraryId() != m_currentLibrary.id
           || libraryTrack.modifiedTime() < lastModified || !onlyModified) {
            Track changedTrack(libraryTrack);
            if(!m_audioLoader->readTrackMetadata(changedTrack)) {
                return;
            }

            if(lastModifiedTime.isValid()) {
                changedTrack.setModifiedTime(lastModified);
            }

            updateExistingTrack(changedTrack, file);
        }
    }
    else if(m_existingArchives.contains(file)) {
        const Track& libraryTrack = m_existingArchives[file].front();

        if(!libraryTrack.isEnabled() || libraryTrack.libraryId() != m_currentLibrary.id
           || libraryTrack.modifiedTime() < lastModified || !onlyModified) {
            TrackList tracks = readArchiveTracks(file);
            for(Track& track : tracks) {
                updateExistingTrack(track, track.filepath());
            }
        }
    }
    else {
        readNewTrack(file);
    }
}

void LibraryScannerPrivate::populateExistingTracks(const TrackList& tracks, bool includeMissing)
{
    for(const Track& track : tracks) {
        m_trackPaths[track.filepath()].push_back(track);
        if(track.isInArchive()) {
            m_existingArchives[track.archivePath()].push_back(track);
        }

        if(includeMissing) {
            if(track.hasCue()) {
                const auto cuePath = track.hasEmbeddedCue() ? track.filepath() : track.cuePath();
                m_existingCueTracks[cuePath].emplace_back(track);
                if(!QFileInfo::exists(cuePath)) {
                    m_missingCueTracks[cuePath].emplace_back(track);
                }
            }

            if(!track.isInArchive()) {
                if(!QFileInfo::exists(track.filepath())) {
                    m_missingFiles[track.filename()].push_back(track);
                    m_missingHashes.emplace(track.hash(), track);
                }
            }
            else {
                if(!QFileInfo::exists(track.archivePath())) {
                    m_missingFiles[track.filename()].push_back(track);
                    m_missingHashes.emplace(track.hash(), track);
                }
            }
        }
    }
}

bool LibraryScannerPrivate::getAndSaveAllTracks(const QStringList& paths, const TrackList& tracks, bool onlyModified)
{
    populateExistingTracks(tracks);

    using namespace Settings::Core::Internal;

    QStringList restrictExtensions = m_settings->fileValue(LibraryRestrictTypes).toStringList();
    const QStringList excludeExtensions
        = m_settings->fileValue(LibraryExcludeTypes, QStringList{u"cue"_s}).toStringList();

    if(restrictExtensions.empty()) {
        restrictExtensions = m_audioLoader->supportedFileExtensions();
        restrictExtensions.append(u"cue"_s);
    }

    const auto files = getFiles(paths, restrictExtensions, excludeExtensions, {});

    m_totalFiles = files.size();
    reportProgress({});

    for(const auto& file : files) {
        if(!m_self->mayRun()) {
            return false;
        }

        const QString filepath = file.absoluteFilePath();

        if(file.suffix() == "cue"_L1) {
            readCue(filepath, onlyModified);
        }
        else {
            readFile(filepath, onlyModified);
        }

        fileScanned(filepath);
        checkBatchFinished();
    }

    for(auto it = m_missingFiles.cbegin(); it != m_missingFiles.cend(); ++it) {
        for(const Track& missingTrack : it.value()) {
            if(missingTrack.isInLibrary() || missingTrack.isEnabled()) {
                qCDebug(LIB_SCANNER) << "Track not found:" << missingTrack.prettyFilepath();

                Track disabledTrack(missingTrack);
                disabledTrack.setLibraryId(-1);
                disabledTrack.setIsEnabled(false);
                m_tracksToUpdate.push_back(disabledTrack);
            }
        }
    }

    m_trackDatabase.storeTracks(m_tracksToStore);
    m_trackDatabase.updateTracks(m_tracksToUpdate);

    if(!m_tracksToStore.empty() || !m_tracksToUpdate.empty()) {
        emit m_self->scanUpdate({m_tracksToStore, m_tracksToUpdate});
    }

    return true;
}

void LibraryScannerPrivate::changeLibraryStatus(LibraryInfo::Status status)
{
    m_currentLibrary.status = status;
    emit m_self->statusChanged(m_currentLibrary);
}

LibraryScanner::LibraryScanner(DbConnectionPoolPtr dbPool, std::shared_ptr<PlaylistLoader> playlistLoader,
                               std::shared_ptr<AudioLoader> audioLoader, SettingsManager* settings, QObject* parent)
    : Worker{parent}
    , p{std::make_unique<LibraryScannerPrivate>(this, std::move(dbPool), std::move(playlistLoader),
                                                std::move(audioLoader), settings)}
{ }

LibraryScanner::~LibraryScanner() = default;

void LibraryScanner::initialiseThread()
{
    Worker::initialiseThread();

    p->m_dbHandler = std::make_unique<DbConnectionHandler>(p->m_dbPool);
    p->m_trackDatabase.initialise(DbConnectionProvider{p->m_dbPool});
}

void LibraryScanner::stopThread()
{
    if(state() == Running) {
        QMetaObject::invokeMethod(
            this,
            [this]() {
                emit progressChanged(static_cast<int>(p->m_totalFiles), {}, static_cast<int>(p->m_totalFiles));
            },
            Qt::QueuedConnection);
    }

    setState(Idle);
}

void LibraryScanner::setMonitorLibraries(bool enabled)
{
    p->m_monitor = enabled;
}

void LibraryScanner::setupWatchers(const LibraryInfoMap& libraries, bool enabled)
{
    for(const auto& library : libraries | std::views::values) {
        if(!enabled) {
            if(library.status == LibraryInfo::Status::Monitoring) {
                LibraryInfo updatedLibrary{library};
                updatedLibrary.status = LibraryInfo::Status::Idle;
                emit statusChanged(updatedLibrary);
            }
        }
        else if(!p->m_watchers.contains(library.id)) {
            p->addWatcher(library);
            LibraryInfo updatedLibrary{library};
            updatedLibrary.status = LibraryInfo::Status::Monitoring;
            emit statusChanged(updatedLibrary);
        }
    }

    if(!enabled) {
        p->m_watchers.clear();
    }
}

void LibraryScanner::scanLibrary(const LibraryInfo& library, const TrackList& tracks, bool onlyModified)
{
    setState(Running);

    p->m_currentLibrary = library;
    p->changeLibraryStatus(LibraryInfo::Status::Scanning);

    const Timer timer;

    if(p->m_currentLibrary.id >= 0 && QFileInfo::exists(p->m_currentLibrary.path)) {
        if(p->m_monitor && !p->m_watchers.contains(library.id)) {
            p->addWatcher(library);
        }
        p->getAndSaveAllTracks({library.path}, tracks, onlyModified);
        p->cleanupScan();
    }

    qCInfo(LIB_SCANNER) << "Scan of" << library.name << "took" << timer.elapsedFormatted();

    if(state() == Paused) {
        p->changeLibraryStatus(LibraryInfo::Status::Pending);
    }
    else {
        p->changeLibraryStatus(p->m_monitor ? LibraryInfo::Status::Monitoring : LibraryInfo::Status::Idle);
        setState(Idle);
        emit finished();
    }
}

void LibraryScanner::scanLibraryDirectoies(const LibraryInfo& library, const QStringList& dirs, const TrackList& tracks)
{
    setState(Running);

    p->m_currentLibrary = library;
    p->changeLibraryStatus(LibraryInfo::Status::Scanning);

    p->getAndSaveAllTracks(dirs, tracks, true);
    p->cleanupScan();

    if(state() == Paused) {
        p->changeLibraryStatus(LibraryInfo::Status::Pending);
    }
    else {
        p->changeLibraryStatus(p->m_monitor ? LibraryInfo::Status::Monitoring : LibraryInfo::Status::Idle);
        setState(Idle);
        emit finished();
    }
}

void LibraryScanner::scanTracks(const TrackList& /*libraryTracks*/, const TrackList& tracks, bool onlyModified)
{
    setState(Running);

    const Timer timer;

    p->m_totalFiles = tracks.size();

    TrackList tracksToUpdate;

    // Group embedded cue tracks by parent file
    QHash<QString, TrackList> embeddedCueTracks;

    for(const Track& track : tracks) {
        if(!mayRun()) {
            p->finishScan();
            return;
        }

        if(track.hasEmbeddedCue()) {
            embeddedCueTracks[track.filepath()].push_back(track);
            continue;
        }

        if(track.hasCue()) {
            // External cue tracks - skip for now (would need different handling)
            continue;
        }

        if(onlyModified) {
            const QFileInfo info{track.filepath()};
            const QDateTime lastModifiedTime{info.lastModified()};
            uint64_t lastModified{0};

            if(lastModifiedTime.isValid()) {
                lastModified = static_cast<uint64_t>(lastModifiedTime.toMSecsSinceEpoch());
            }

            if(track.modifiedTime() >= lastModified) {
                continue;
            }
        }

        Track updatedTrack{track.filepath()};

        if(p->m_audioLoader->readTrackMetadata(updatedTrack)) {
            updatedTrack.setId(track.id());
            updatedTrack.setLibraryId(track.libraryId());
            updatedTrack.setAddedTime(track.addedTime());
            readFileProperties(updatedTrack);
            updatedTrack.generateHash();

            tracksToUpdate.push_back(updatedTrack);
        }

        p->fileScanned(track.filepath());
    }

    // Process embedded cue tracks
    for(auto it = embeddedCueTracks.begin(); it != embeddedCueTracks.end(); ++it) {
        if(!mayRun()) {
            p->finishScan();
            return;
        }

        const QString& filepath = it.key();
        TrackList& cueTracks    = it.value();

        // Build a map of track number -> original track for preserving IDs
        QHash<QString, Track> originalTracksByNum;
        for(const Track& t : cueTracks) {
            originalTracksByNum.insert(t.trackNumber(), t);
        }

        // Re-read the parent file
        Track parentTrack(filepath);
        if(!p->m_audioLoader->readTrackMetadata(parentTrack)) {
            continue;
        }

        if(!parentTrack.hasExtraTag(u"CUESHEET"_s)) {
            continue;
        }

        // Regenerate cue tracks from the embedded cuesheet
        TrackList newCueTracks = p->readEmbeddedPlaylistTracks(parentTrack);

        for(Track& newTrack : newCueTracks) {
            const QString trackNum = newTrack.trackNumber();
            if(originalTracksByNum.contains(trackNum)) {
                const Track& original = originalTracksByNum.value(trackNum);
                newTrack.setId(original.id());
                newTrack.setLibraryId(original.libraryId());
                newTrack.setAddedTime(original.addedTime());
                tracksToUpdate.push_back(newTrack);
            }
        }

        p->fileScanned(filepath);
    }

    if(!tracksToUpdate.empty()) {
        p->m_trackDatabase.updateTracks(tracksToUpdate);
        p->m_trackDatabase.updateTrackStats(tracksToUpdate);

        emit scanUpdate({{}, tracksToUpdate});
    }

    qCInfo(LIB_SCANNER) << "Scan of" << p->m_totalFiles << "tracks took" << timer.elapsedFormatted();

    p->finishScan();
}

void LibraryScanner::scanFiles(const TrackList& libraryTracks, const QList<QUrl>& urls)
{
    setState(Running);

    const Timer timer;

    TrackList tracksScanned;
    TrackList playlistTracksScanned;

    p->populateExistingTracks(libraryTracks, false);

    using namespace Settings::Core::Internal;

    const QStringList playlistExtensions = Playlist::supportedPlaylistExtensions();
    QStringList restrictExtensions       = p->m_settings->fileValue(ExternalRestrictTypes).toStringList();
    const QStringList excludeExtensions  = p->m_settings->fileValue(ExternalExcludeTypes).toStringList();

    if(restrictExtensions.empty()) {
        restrictExtensions = p->m_audioLoader->supportedFileExtensions();
        restrictExtensions.append(u"cue"_s);
    }

    QStringList paths;
    std::ranges::transform(urls, std::back_inserter(paths), [](const QUrl& url) { return url.toLocalFile(); });

    const auto files = getFiles(paths, restrictExtensions, excludeExtensions, playlistExtensions);

    p->m_totalFiles = files.size();

    p->reportProgress({});

    for(const auto& file : files) {
        if(!mayRun()) {
            p->finishScan();
            return;
        }

        const QString filepath = file.absoluteFilePath();

        if(playlistExtensions.contains(file.suffix())) {
            const TrackList playlistTracks = p->readPlaylist(filepath);
            playlistTracksScanned.insert(playlistTracksScanned.end(), playlistTracks.cbegin(), playlistTracks.cend());
            p->fileScanned(filepath);
        }
        else {
            if(!p->m_filesScanned.contains(filepath)) {
                if(p->m_trackPaths.contains(filepath)) {
                    const auto existingTracks = p->m_trackPaths.value(filepath);
                    for(const Track& track : existingTracks) {
                        tracksScanned.push_back(track);
                    }
                }
                else if(p->m_existingArchives.contains(filepath)) {
                    const auto existingTracks = p->m_existingArchives.value(filepath);
                    for(const Track& track : existingTracks) {
                        tracksScanned.push_back(track);
                    }
                }
                else {
                    TrackList tracks = p->readTracks(filepath);
                    if(tracks.empty()) {
                        continue;
                    }
                    for(Track& track : tracks) {
                        readFileProperties(track);
                        track.setAddedTime(QDateTime::currentMSecsSinceEpoch());

                        if(track.hasExtraTag(u"CUESHEET"_s)) {
                            const TrackList cueTracks = p->readEmbeddedPlaylistTracks(track);
                            std::ranges::copy(cueTracks, std::back_inserter(tracksScanned));
                        }
                        else {
                            tracksScanned.push_back(track);
                        }
                    }
                }
            }

            p->fileScanned(filepath);
        }
    }

    if(!playlistTracksScanned.empty()) {
        p->m_trackDatabase.storeTracks(playlistTracksScanned);
        emit playlistLoaded(playlistTracksScanned);
    }

    if(!tracksScanned.empty()) {
        p->m_trackDatabase.storeTracks(tracksScanned);
        emit scannedTracks(tracksScanned);
    }

    qCInfo(LIB_SCANNER) << "Scan of" << p->m_totalFiles << "files took" << timer.elapsedFormatted();

    p->finishScan();
}

void LibraryScanner::scanPlaylist(const TrackList& libraryTracks, const QList<QUrl>& urls)
{
    setState(Running);

    const Timer timer;

    TrackList tracksScanned;

    p->populateExistingTracks(libraryTracks, false);
    p->reportProgress({});

    if(!mayRun()) {
        p->finishScan();
        return;
    }

    for(const auto& url : urls) {
        const TrackList playlistTracks = p->readPlaylist(url.toLocalFile());
        tracksScanned.insert(tracksScanned.end(), playlistTracks.cbegin(), playlistTracks.cend());
    }

    if(!tracksScanned.empty()) {
        p->m_trackDatabase.storeTracks(tracksScanned);
        emit playlistLoaded(tracksScanned);
    }

    qCInfo(LIB_SCANNER) << "Scan of playlist took" << timer.elapsedFormatted();

    p->finishScan();
}
} // namespace Fooyin

#include "moc_libraryscanner.cpp"
