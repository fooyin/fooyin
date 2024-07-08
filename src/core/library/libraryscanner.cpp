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
#include "library/libraryinfo.h"
#include "librarywatcher.h"
#include "playlist/playlistloader.h"

#include <core/playlist/playlist.h>
#include <core/playlist/playlistparser.h>
#include <core/tagging/tagloader.h>
#include <core/track.h>
#include <utils/database/dbconnectionhandler.h>
#include <utils/database/dbconnectionpool.h>
#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>
#include <utils/timer.h>
#include <utils/utils.h>

#include <QBuffer>
#include <QDir>
#include <QFileSystemWatcher>
#include <QRegularExpression>

#include <ranges>
#include <stack>

constexpr auto BatchSize = 250;

namespace {
struct LibraryDirectory
{
    QString directory;
    QStringList files;
    QStringList playlists;
};
using LibraryDirectories = std::vector<LibraryDirectory>;

bool fileIsValid(const QString& file)
{
    const QFileInfo fileInfo{file};
    return fileInfo.size() > 0;
}

bool hasMatchingExtension(const QString& file, const QStringList& extensions)
{
    if(file.isEmpty()) {
        return false;
    }

    return std::ranges::any_of(extensions, [&file](const QString& wildcard) {
        const QRegularExpression re{QRegularExpression::wildcardToRegularExpression(wildcard)};
        const QRegularExpressionMatch match = re.match(file);
        return match.hasMatch();
    });
}

std::optional<QString> findMatchingCue(const QFileInfo& file)
{
    static const QStringList cueExtensions{QStringLiteral("*.cue")};

    const QDir dir           = file.absoluteDir();
    const QFileInfoList cues = dir.entryInfoList(cueExtensions, QDir::Files);

    for(const auto& cue : cues) {
        if(cue.completeBaseName() == file.completeBaseName() || cue.fileName().contains(file.fileName())) {
            return cue.absoluteFilePath();
        }
    }

    return {};
}

LibraryDirectories getDirectories(const QString& baseDirectory, const QStringList& trackExtensions)
{
    LibraryDirectories dirs;

    std::stack<QDir> stack;
    stack.emplace(baseDirectory);

    static const QStringList playlistExtensions{Fooyin::Playlist::supportedPlaylistExtensions()};
    static const QStringList cueExtensions{QStringLiteral("*.cue")};

    while(!stack.empty()) {
        QDir dir = stack.top();
        stack.pop();

        const QFileInfo info{dir.absolutePath()};
        QStringList files;
        QStringList playlists;

        if(info.isDir()) {
            QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
            for(const auto& subDir : subDirs | std::views::reverse) {
                stack.emplace(subDir.absoluteFilePath());
            }
            const QFileInfoList foundFiles = dir.entryInfoList(trackExtensions, QDir::Files);
            for(const auto& file : foundFiles) {
                const QString path = file.absoluteFilePath();
                if(fileIsValid(path)) {
                    files.append(path);
                }
            }
            const QFileInfoList foundCues = dir.entryInfoList(cueExtensions, QDir::Files);
            for(const auto& file : foundCues) {
                playlists.append(file.absoluteFilePath());
            }

            dirs.emplace_back(dir.absolutePath(), files, playlists);
        }
        else {
            dir.cdUp();
            if(hasMatchingExtension(info.fileName(), trackExtensions)) {
                files.append(info.absoluteFilePath());
                if(const auto cue = findMatchingCue(info)) {
                    playlists.append(cue.value());
                }
            }
            else if(hasMatchingExtension(info.fileName(), playlistExtensions)) {
                playlists.append(info.absoluteFilePath());
            }

            dirs.emplace_back(dir.absolutePath(), files, playlists);
        }
    }

    return dirs;
}

LibraryDirectories getDirectories(const QList<QUrl>& urls, const QStringList& trackExtensions)
{
    std::unordered_map<QString, LibraryDirectory> processedDirs;

    static const QStringList cueExtensions{QStringLiteral("*.cue")};

    for(const QUrl& url : urls) {
        const QFileInfo file{url.toLocalFile()};
        const auto dir = file.absolutePath();

        if(processedDirs.contains(dir)) {
            auto& libDir = processedDirs.at(dir);
            if(hasMatchingExtension(file.fileName(), trackExtensions)) {
                const QString path = file.absoluteFilePath();
                if(fileIsValid(path)) {
                    libDir.files.append(file.absoluteFilePath());
                }
            }
            else if(!libDir.playlists.contains(file.absoluteFilePath())
                    && hasMatchingExtension(file.fileName(), cueExtensions)) {
                libDir.playlists.append(file.absoluteFilePath());
            }
        }
        else {
            const auto libDirs = getDirectories(file.absoluteFilePath(), trackExtensions);
            for(const auto& libDir : libDirs) {
                processedDirs.emplace(libDir.directory, libDir);
            }
        }
    }

    LibraryDirectories dirs;
    dirs.reserve(processedDirs.size());

    for(const auto& [_, dir] : processedDirs) {
        dirs.emplace_back(dir);
    }

    return dirs;
}
} // namespace

namespace Fooyin {
struct LibraryScanner::Private
{
    LibraryScanner* m_self;

    DbConnectionPoolPtr m_dbPool;
    std::shared_ptr<PlaylistLoader> m_playlistLoader;
    std::shared_ptr<TagLoader> m_tagLoader;
    SettingsManager* m_settings;

    std::unique_ptr<DbConnectionHandler> m_dbHandler;

    LibraryInfo m_currentLibrary;
    TrackDatabase m_trackDatabase;

    TrackList m_tracksToStore;
    TrackList m_tracksToUpdate;

    std::unordered_map<QString, Track> m_trackPaths;
    std::unordered_map<QString, Track> m_missingFiles;
    std::unordered_map<QString, Track> m_missingHashes;
    std::unordered_map<QString, TrackList> m_existingCueTracks;
    std::unordered_map<QString, TrackList> m_missingCueTracks;
    std::set<QString> m_cueFilesScanned;

    int m_tracksProcessed{0};
    int m_totalTracks{0};

    std::unordered_map<int, LibraryWatcher> m_watchers;

    Private(LibraryScanner* self, DbConnectionPoolPtr dbPool, std::shared_ptr<PlaylistLoader> playlistLoader,
            std::shared_ptr<TagLoader> tagLoader, SettingsManager* settings)
        : m_self{self}
        , m_dbPool{std::move(dbPool)}
        , m_playlistLoader{std::move(playlistLoader)}
        , m_tagLoader{std::move(tagLoader)}
        , m_settings{settings}
    { }

    void cleanupScan()
    {
        m_tracksProcessed = 0;
        m_totalTracks     = 0;
        m_tracksToStore.clear();
        m_tracksToUpdate.clear();
        m_trackPaths.clear();
        m_missingFiles.clear();
        m_missingHashes.clear();
        m_existingCueTracks.clear();
        m_missingCueTracks.clear();
        m_cueFilesScanned.clear();
    }

    void addWatcher(const Fooyin::LibraryInfo& library)
    {
        auto watchPaths = [this, library](const QString& path) {
            QStringList dirs = Utils::File::getAllSubdirectories(path);
            dirs.append(path);
            m_watchers[library.id].addPaths(dirs);
        };

        watchPaths(library.path);

        QObject::connect(&m_watchers.at(library.id), &LibraryWatcher::libraryDirChanged, m_self,
                         [this, watchPaths, library](const QString& dir) {
                             watchPaths(dir);
                             emit m_self->directoryChanged(library, dir);
                         });
    }

    void reportProgress() const
    {
        emit m_self->progressChanged(m_tracksProcessed, m_totalTracks);
    }

    void fileScanned()
    {
        ++m_tracksProcessed;
        reportProgress();
    }

    Track matchMissingTrack(Track& track)
    {
        const QString filename = track.filename();
        const QString hash     = track.hash();

        if(m_missingFiles.contains(filename) && m_missingFiles.at(filename).duration() == track.duration()) {
            return m_missingFiles.at(filename);
        }

        if(m_missingHashes.contains(hash) && m_missingHashes.at(hash).duration() == track.duration()) {
            return m_missingHashes.at(hash);
        }

        return {};
    };

    void storeTracks(TrackList& tracks)
    {
        if(!m_self->mayRun()) {
            return;
        }

        m_trackDatabase.storeTracks(tracks);
    }

    void checkBatchFinished()
    {
        if(m_tracksToStore.size() >= BatchSize || m_tracksToUpdate.size() > BatchSize) {
            if(m_tracksToStore.size() >= BatchSize) {
                storeTracks(m_tracksToStore);
            }
            if(m_tracksToUpdate.size() >= BatchSize) {
                storeTracks(m_tracksToUpdate);
            }
            emit m_self->scanUpdate({.addedTracks = m_tracksToStore, .updatedTracks = {}});
            m_tracksToStore.clear();
            m_tracksToUpdate.clear();
        }
    }

    void readFileProperties(Track& track)
    {
        const QFileInfo fileInfo{track.filepath()};

        if(track.modifiedTime() == 0) {
            const QDateTime modifiedTime = fileInfo.lastModified();
            track.setModifiedTime(modifiedTime.isValid() ? modifiedTime.toMSecsSinceEpoch() : 0);
        }
        if(track.fileSize() == 0) {
            track.setFileSize(fileInfo.size());
        }
    }

    [[nodiscard]] bool readTrackMetadata(Track& track) const
    {
        if(auto* tagParser = m_tagLoader->parserForTrack(track)) {
            if(tagParser->readMetaData(track)) {
                track.generateHash();
                return true;
            }
            qDebug() << "Failed to read metadata for" << track.filepath();
        }

        qDebug() << "Unsupported file:" << track.filepath();
        return false;
    }

    TrackList readPlaylistTracks(const QString& file) const
    {
        if(file.isEmpty()) {
            return {};
        }

        QFile playlistFile{file};
        if(!playlistFile.open(QIODevice::ReadOnly)) {
            qWarning() << QStringLiteral("Could not open playlistFile file %1 for reading: %2")
                              .arg(playlistFile.fileName(), playlistFile.errorString());
            return {};
        }

        const QFileInfo info{playlistFile};
        QDir dir{file};
        dir.cdUp();

        if(auto* parser = m_playlistLoader->parserForExtension(info.suffix())) {
            return parser->readPlaylist(&playlistFile, file, dir, true);
        }

        return {};
    }

    TrackList readEmbeddedPlaylistTracks(const Track& track) const
    {
        const auto cues = track.extraTag(QStringLiteral("CUESHEET"));
        QByteArray bytes{cues.front().toUtf8()};
        QBuffer buffer(&bytes);
        if(!buffer.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << QStringLiteral("Can't open buffer for reading: %1").arg(buffer.errorString());
            return {};
        }

        if(auto* parser = m_playlistLoader->parserForExtension(QStringLiteral("cue"))) {
            TrackList tracks = parser->readPlaylist(&buffer, track.filepath(), {}, false);
            for(auto& plTrack : tracks) {
                plTrack.generateHash();
            }
            return tracks;
        }

        return {};
    }

    void updateExistingCueTracks(const TrackList& tracks, const QDir& dir, const QString& cue)
    {
        std::unordered_map<QString, Track> existingTrackPaths;
        for(const Track& track : tracks) {
            existingTrackPaths.emplace(track.uniqueFilepath(), track);
        }

        const TrackList cueTracks = readPlaylistTracks(cue);
        for(const Track& cueTrack : cueTracks) {
            Track track{cueTrack};
            if(existingTrackPaths.contains(track.uniqueFilepath())) {
                track.setId(existingTrackPaths.at(track.uniqueFilepath()).id());
            }
            setTrackProps(track, dir);
            m_tracksToUpdate.push_back(track);
            m_cueFilesScanned.emplace(track.filepath());
        }
    }

    void addNewCueTracks(const QString& cue, const QDir& dir, const QString& filename)
    {
        if(m_missingCueTracks.contains(filename)) {
            TrackList refoundCueTracks = m_missingCueTracks.at(cue);
            for(Track& track : refoundCueTracks) {
                track.setCuePath(cue);
                m_tracksToUpdate.push_back(track);
            }
        }
        else {
            const TrackList cueTracks = readPlaylistTracks(cue);
            for(const Track& cueTrack : cueTracks) {
                Track track{cueTrack};
                setTrackProps(track, dir);
                m_tracksToStore.push_back(track);
                m_cueFilesScanned.emplace(track.filepath());
            }
        }
    }

    void readCue(const QString& cue, const QDir& baseDir, bool onlyModified)
    {
        const QFileInfo info{cue};
        const QDateTime lastModifiedTime{info.lastModified()};
        uint64_t lastModified{0};

        if(lastModifiedTime.isValid()) {
            lastModified = static_cast<uint64_t>(lastModifiedTime.toMSecsSinceEpoch());
        }

        if(m_existingCueTracks.contains(cue)) {
            const auto& tracks = m_existingCueTracks.at(cue);
            if(tracks.front().modifiedTime() < lastModified || !onlyModified) {
                updateExistingCueTracks(tracks, baseDir, cue);
            }
            else {
                for(const Track& track : tracks) {
                    m_cueFilesScanned.emplace(track.filepath());
                }
            }
        }
        else {
            addNewCueTracks(cue, baseDir, info.fileName());
        }
    }

    void setTrackProps(Track& track, const QDir& dir)
    {
        setTrackProps(track, dir, track.filepath());
    };

    void setTrackProps(Track& track, const QDir& dir, const QString& file)
    {
        readFileProperties(track);
        track.setFilePath(file);

        if(m_currentLibrary.id >= 0) {
            track.setLibraryId(m_currentLibrary.id);
            track.setRelativePath(dir.relativeFilePath(file));
        }
        track.generateHash();
        track.setIsEnabled(true);
    };

    void updateExistingTrack(Track& track, const QDir& dir, const QString& file)
    {
        setTrackProps(track, dir, file);
        m_missingFiles.erase(track.filename());

        if(track.hasExtraTag(QStringLiteral("CUESHEET"))) {
            std::unordered_map<QString, Track> existingTrackPaths;
            if(m_existingCueTracks.contains(track.filepath())) {
                const auto& tracks = m_existingCueTracks.at(track.filepath());
                for(const Track& existingTrack : tracks) {
                    existingTrackPaths.emplace(existingTrack.uniqueFilepath(), existingTrack);
                }
            }

            TrackList cueTracks = readEmbeddedPlaylistTracks(track);
            for(Track& cueTrack : cueTracks) {
                if(existingTrackPaths.contains(cueTrack.uniqueFilepath())) {
                    cueTrack.setId(existingTrackPaths.at(cueTrack.uniqueFilepath()).id());
                }
                setTrackProps(cueTrack, dir, file);
                m_tracksToUpdate.push_back(cueTrack);
                m_missingHashes.erase(cueTrack.hash());
            }
        }
        else {
            m_tracksToUpdate.push_back(track);
            m_missingHashes.erase(track.hash());
        }
    }

    void readNewTrack(const QDir& dir, const QString& file)
    {
        Track track{file};
        if(!readTrackMetadata(track)) {
            return;
        }

        Track refoundTrack = matchMissingTrack(track);
        if(refoundTrack.isInLibrary() || refoundTrack.isInDatabase()) {
            m_missingHashes.erase(refoundTrack.hash());
            m_missingFiles.erase(refoundTrack.filename());

            setTrackProps(refoundTrack, dir, file);
            m_tracksToUpdate.push_back(refoundTrack);
        }
        else {
            setTrackProps(track, dir, file);
            track.setAddedTime(QDateTime::currentMSecsSinceEpoch());

            if(track.hasExtraTag(QStringLiteral("CUESHEET"))) {
                TrackList cueTracks = readEmbeddedPlaylistTracks(track);
                for(Track& cueTrack : cueTracks) {
                    setTrackProps(cueTrack, dir, file);
                    m_tracksToStore.push_back(cueTrack);
                }
            }
            else {
                m_tracksToStore.push_back(track);
            }
        }
    }

    void readFile(const QString& file, const QDir& baseDir, bool onlyModified)
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
            const Track& libraryTrack = m_trackPaths.at(file);

            if(!libraryTrack.isEnabled() || libraryTrack.libraryId() != m_currentLibrary.id
               || libraryTrack.modifiedTime() < lastModified || !onlyModified) {
                Track changedTrack{libraryTrack};
                if(!readTrackMetadata(changedTrack)) {
                    return;
                }

                updateExistingTrack(changedTrack, baseDir, file);
            }
        }
        else {
            readNewTrack(baseDir, file);
        }
    }

    bool getAndSaveAllTracks(const QString& path, const TrackList& tracks, bool onlyModified)
    {
        for(const Track& track : tracks) {
            m_trackPaths.emplace(track.filepath(), track);

            if(track.hasCue()) {
                const auto cuePath = track.cuePath() == u"Embedded" ? track.filepath() : track.cuePath();
                m_existingCueTracks[cuePath].emplace_back(track);
                if(!QFileInfo::exists(cuePath)) {
                    m_missingCueTracks[cuePath].emplace_back(track);
                }
            }

            if(!QFileInfo::exists(track.filepath())) {
                m_missingFiles.emplace(track.filename(), track);
                m_missingHashes.emplace(track.hash(), track);
            }
        }

        reportProgress();

        const QDir baseDir{path};
        const auto dirs = getDirectories(path, Utils::extensionsToWildcards(m_tagLoader->supportedFileExtensions()));

        m_tracksProcessed = 0;
        m_totalTracks     = std::accumulate(dirs.cbegin(), dirs.cend(), 0, [](int sum, const LibraryDirectory& dir) {
            return sum + static_cast<int>(dir.files.size()) + static_cast<int>(dir.playlists.size());
        });

        for(const auto& [_, files, cues] : dirs) {
            for(const auto& cue : cues) {
                if(!m_self->mayRun()) {
                    return false;
                }

                readCue(cue, baseDir, onlyModified);

                fileScanned();
            }

            for(const auto& file : files) {
                if(!m_self->mayRun()) {
                    return false;
                }

                readFile(file, baseDir, onlyModified);

                fileScanned();
                checkBatchFinished();
            }
        }

        for(auto& track : m_missingFiles | std::views::values) {
            if(track.isInLibrary() || track.isEnabled()) {
                track.setLibraryId(-1);
                track.setIsEnabled(false);
                m_tracksToUpdate.push_back(track);
            }
        }

        storeTracks(m_tracksToStore);
        storeTracks(m_tracksToUpdate);

        if(!m_tracksToStore.empty() || !m_tracksToUpdate.empty()) {
            emit m_self->scanUpdate({m_tracksToStore, m_tracksToUpdate});
        }

        return true;
    }

    void changeLibraryStatus(LibraryInfo::Status status)
    {
        m_currentLibrary.status = status;
        emit m_self->statusChanged(m_currentLibrary);
    }
};

LibraryScanner::LibraryScanner(DbConnectionPoolPtr dbPool, std::shared_ptr<PlaylistLoader> playlistLoader,
                               std::shared_ptr<TagLoader> tagLoader, SettingsManager* settings, QObject* parent)
    : Worker{parent}
    , p{std::make_unique<Private>(this, std::move(dbPool), std::move(playlistLoader), std::move(tagLoader), settings)}
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
                p->m_tracksProcessed = p->m_totalTracks;
                emit progressChanged(p->m_tracksProcessed, p->m_totalTracks);
            },
            Qt::QueuedConnection);
    }

    setState(Idle);
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
        if(p->m_settings->value<Settings::Core::Internal::MonitorLibraries>() && !p->m_watchers.contains(library.id)) {
            p->addWatcher(library);
        }
        p->getAndSaveAllTracks(library.path, tracks, onlyModified);
        p->cleanupScan();
    }

    qDebug() << "[Library] Scan of" << library.name << "took" << timer.elapsedFormatted();

    if(state() == Paused) {
        p->changeLibraryStatus(LibraryInfo::Status::Pending);
    }
    else {
        p->changeLibraryStatus(p->m_settings->value<Settings::Core::Internal::MonitorLibraries>()
                                   ? LibraryInfo::Status::Monitoring
                                   : LibraryInfo::Status::Idle);
        setState(Idle);
        emit finished();
    }
}

void LibraryScanner::scanLibraryDirectory(const LibraryInfo& library, const QString& dir, const TrackList& tracks)
{
    setState(Running);

    p->m_currentLibrary = library;

    p->changeLibraryStatus(LibraryInfo::Status::Scanning);

    p->getAndSaveAllTracks(dir, tracks, true);
    p->cleanupScan();

    if(state() == Paused) {
        p->changeLibraryStatus(LibraryInfo::Status::Pending);
    }
    else {
        p->changeLibraryStatus(p->m_settings->value<Settings::Core::Internal::MonitorLibraries>()
                                   ? LibraryInfo::Status::Monitoring
                                   : LibraryInfo::Status::Idle);
        setState(Idle);
        emit finished();
    }
}

void LibraryScanner::scanTracks(const TrackList& libraryTracks, const TrackList& tracks)
{
    setState(Running);

    TrackList tracksScanned;
    TrackList tracksToStore;

    std::unordered_map<QString, Track> trackMap;
    std::ranges::transform(libraryTracks, std::inserter(trackMap, trackMap.end()),
                           [](const Track& track) { return std::make_pair(track.filepath(), track); });

    p->m_tracksProcessed = 0;
    p->m_totalTracks     = static_cast<double>(tracks.size());

    const auto handleFinished = [this]() {
        if(state() != Paused) {
            setState(Idle);
            emit finished();
        }
    };

    for(const Track& pendingTrack : tracks) {
        if(!mayRun()) {
            handleFinished();
            return;
        }

        Track track{pendingTrack};

        if(trackMap.contains(track.filepath())) {
            tracksScanned.push_back(trackMap.at(track.filepath()));
        }
        else if(auto* parser = p->m_tagLoader->parserForTrack(track)) {
            if(parser->readMetaData(track)) {
                track.generateHash();
                tracksToStore.push_back(track);
            }
        }

        p->fileScanned();
    }

    p->storeTracks(tracksToStore);

    emit scannedTracks(tracksToStore, tracksScanned);

    handleFinished();
}

void LibraryScanner::scanFiles(const TrackList& libraryTracks, const QList<QUrl>& urls)
{
    setState(Running);

    TrackList tracksScanned;
    TrackList tracksToStore;

    std::unordered_multimap<QString, Track> trackMap;
    std::ranges::transform(libraryTracks, std::inserter(trackMap, trackMap.end()),
                           [](const Track& track) { return std::make_pair(track.filepath(), track); });

    p->m_tracksProcessed = 0;

    const auto handleFinished = [this]() {
        if(state() != Paused) {
            setState(Idle);
            p->m_tracksProcessed = p->m_totalTracks;
            p->reportProgress();
            emit finished();
        }
    };

    const LibraryDirectories dirs
        = getDirectories(urls, Utils::extensionsToWildcards(p->m_tagLoader->supportedFileExtensions()));

    p->m_totalTracks = std::accumulate(dirs.cbegin(), dirs.cend(), 0, [](size_t sum, const LibraryDirectory& dir) {
        return sum + dir.files.size() + dir.playlists.size();
    });
    std::set<QString> filesScanned;

    for(const auto& [_, files, playlists] : dirs) {
        if(!mayRun()) {
            handleFinished();
            return;
        }

        if(files.empty() && playlists.empty()) {
            handleFinished();
            return;
        }

        for(const auto& playlist : playlists) {
            const TrackList playlistTracks = p->readPlaylistTracks(playlist);
            for(const Track& playlistTrack : playlistTracks) {
                const auto trackKey = playlistTrack.filepath();

                if(trackMap.contains(trackKey)) {
                    const auto existingTracks = trackMap.equal_range(trackKey);
                    for(auto it = existingTracks.first; it != existingTracks.second; ++it) {
                        if(it->second.uniqueFilepath() == playlistTrack.uniqueFilepath()) {
                            tracksScanned.push_back(it->second);
                            break;
                        }
                    }
                }

                if(!trackMap.contains(trackKey)) {
                    Track track{playlistTrack};
                    track.generateHash();
                    tracksToStore.push_back(track);
                }

                filesScanned.emplace(playlistTrack.filepath());
            }

            p->fileScanned();
        }

        for(const auto& file : files) {
            if(!filesScanned.contains(file)) {
                if(trackMap.contains(file)) {
                    const auto existingTracks = trackMap.equal_range(file);
                    for(auto it = existingTracks.first; it != existingTracks.second; ++it) {
                        tracksScanned.push_back(it->second);
                    }
                }
                else {
                    Track track{file};
                    if(!p->readTrackMetadata(track)) {
                        continue;
                    }
                    p->readFileProperties(track);
                    track.setAddedTime(QDateTime::currentMSecsSinceEpoch());

                    if(track.hasExtraTag(QStringLiteral("CUESHEET"))) {
                        const TrackList cueTracks = p->readEmbeddedPlaylistTracks(track);
                        std::ranges::copy(cueTracks, std::back_inserter(tracksToStore));
                    }
                    else {
                        tracksToStore.push_back(track);
                    }
                }
            }

            p->fileScanned();
        }
    }

    p->storeTracks(tracksToStore);

    emit scannedTracks(tracksToStore, tracksScanned);

    handleFinished();
}
} // namespace Fooyin

#include "moc_libraryscanner.cpp"
