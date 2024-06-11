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
#include "playlist/parsers/cueparser.h"
#include "tagging/tagreader.h"

#include <core/playlist/playlistparser.h>
#include <core/track.h>
#include <utils/database/dbconnectionhandler.h>
#include <utils/database/dbconnectionpool.h>
#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>

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
    QStringList cues;
};
using LibraryDirectories = std::vector<LibraryDirectory>;

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

LibraryDirectories getDirectories(const QString& baseDirectory)
{
    LibraryDirectories dirs;

    std::stack<QDir> stack;
    stack.emplace(baseDirectory);

    static const QStringList fileExtensions = Fooyin::Track::supportedFileExtensions();
    static const QStringList cueExtensions{QStringLiteral("*.cue")};

    while(!stack.empty()) {
        QDir dir = stack.top();
        stack.pop();

        const QFileInfo info{dir.absolutePath()};
        QStringList files;
        QStringList cues;

        if(info.isDir()) {
            const QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
            for(const auto& subDir : subDirs) {
                stack.emplace(subDir.absoluteFilePath());
            }
            const QFileInfoList foundFiles = dir.entryInfoList(fileExtensions, QDir::Files);
            for(const auto& file : foundFiles) {
                files.append(file.absoluteFilePath());
            }
            const QFileInfoList foundCues = dir.entryInfoList(cueExtensions, QDir::Files);
            for(const auto& file : foundCues) {
                cues.append(file.absoluteFilePath());
            }

            dirs.emplace_back(dir.absolutePath(), files, cues);
        }
        else {
            dir.cdUp();
            if(hasMatchingExtension(info.fileName(), fileExtensions)) {
                files.append(info.absoluteFilePath());
                if(const auto cue = findMatchingCue(info)) {
                    cues.append(cue.value());
                }
            }
            else if(hasMatchingExtension(info.fileName(), cueExtensions)) {
                cues.append(info.absoluteFilePath());
            }

            dirs.emplace_back(dir.absolutePath(), files, cues);
        }
    }

    return dirs;
}

LibraryDirectories getDirectories(const QList<QUrl>& urls)
{
    std::unordered_map<QString, LibraryDirectory> processedDirs;

    static const QStringList fileExtensions = Fooyin::Track::supportedFileExtensions();
    static const QStringList cueExtensions{QStringLiteral("*.cue")};

    for(const QUrl& url : urls) {
        const QFileInfo file{url.toLocalFile()};
        const auto dir = file.absolutePath();

        if(processedDirs.contains(dir)) {
            auto& libDir = processedDirs.at(dir);
            if(hasMatchingExtension(file.fileName(), fileExtensions)) {
                libDir.files.append(file.absoluteFilePath());
            }
            else if(!libDir.cues.contains(file.absoluteFilePath())
                    && hasMatchingExtension(file.fileName(), cueExtensions)) {
                libDir.cues.append(file.absoluteFilePath());
            }
        }
        else {
            const auto libDirs = getDirectories(file.absoluteFilePath());
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

Fooyin::Track matchMissingTrack(const Fooyin::TrackFieldMap& missingFiles, const Fooyin::TrackFieldMap& missingHashes,
                                Fooyin::Track& track)
{
    const QString filename = track.filename();
    const QString hash     = track.hash();

    if(missingFiles.contains(filename) && missingFiles.at(filename).duration() == track.duration()) {
        return missingFiles.at(filename);
    }

    if(missingHashes.contains(hash) && missingHashes.at(hash).duration() == track.duration()) {
        return missingHashes.at(hash);
    }

    return {};
};
} // namespace

namespace Fooyin {
struct LibraryScanner::Private
{
    LibraryScanner* self;

    DbConnectionPoolPtr dbPool;
    SettingsManager* settings;

    std::unique_ptr<DbConnectionHandler> dbHandler;

    LibraryInfo currentLibrary;
    TrackDatabase trackDatabase;
    std::unique_ptr<CueParser> cueParser;

    TrackList tracksToStore;
    TrackList tracksToUpdate;

    TrackFieldMap trackPaths;
    TrackFieldMap missingFiles;
    TrackFieldMap missingHashes;
    std::unordered_map<QString, uint64_t> existingCues;
    std::unordered_map<QString, TrackList> missingCueTracks;
    std::set<QString> cueFilesScanned;

    int tracksProcessed{0};
    double totalTracks{0};
    int currentProgress{-1};

    std::unordered_map<int, LibraryWatcher> watchers;

    Private(LibraryScanner* self_, DbConnectionPoolPtr dbPool_, SettingsManager* settings_)
        : self{self_}
        , dbPool{std::move(dbPool_)}
        , settings{settings_}
        , cueParser{std::make_unique<CueParser>()}
    { }

    void cleanupScan()
    {
        tracksToStore.clear();
        tracksToUpdate.clear();
        trackPaths.clear();
        missingFiles.clear();
        missingHashes.clear();
        existingCues.clear();
        missingCueTracks.clear();
        cueFilesScanned.clear();
    }

    void addWatcher(const Fooyin::LibraryInfo& library)
    {
        auto watchPaths = [this, library](const QString& path) {
            QStringList dirs = Utils::File::getAllSubdirectories(path);
            dirs.append(path);
            watchers[library.id].addPaths(dirs);
        };

        watchPaths(library.path);

        QObject::connect(&watchers.at(library.id), &LibraryWatcher::libraryDirChanged, self,
                         [this, watchPaths, library](const QString& dir) {
                             watchPaths(dir);
                             emit self->directoryChanged(library, dir);
                         });
    }

    void reportProgress()
    {
        const int progress = static_cast<int>((tracksProcessed / totalTracks) * 100);
        if(currentProgress != progress) {
            currentProgress = progress;
            emit self->progressChanged(currentProgress);
        }
    }

    void storeTracks(TrackList& tracks)
    {
        if(!self->mayRun()) {
            return;
        }

        trackDatabase.storeTracks(tracks);
    }

    void readCue(const QString& cue, const QDir& baseDir, bool onlyModified)
    {
        const QFileInfo info{cue};
        const QDateTime lastModifiedTime{info.lastModified()};
        uint64_t lastModified{0};

        if(lastModifiedTime.isValid()) {
            lastModified = static_cast<uint64_t>(lastModifiedTime.toMSecsSinceEpoch());
        }

        auto setTrackProps = [this, &baseDir](Track& track) {
            if(currentLibrary.id >= 0) {
                track.setLibraryId(currentLibrary.id);
                track.setRelativePath(baseDir.relativeFilePath(track.filepath()));
            }
            track.generateHash();
            track.setIsEnabled(true);
        };

        if(existingCues.contains(cue)) {
            if(existingCues.at(cue) != lastModified || !onlyModified) {
                const TrackList cueTracks = cueParser->readPlaylist(cue);
                for(const Track& cueTrack : cueTracks) {
                    Track track{cueTrack};
                    setTrackProps(track);
                    tracksToUpdate.push_back(track);
                    cueFilesScanned.emplace(track.filepath());
                }
            }
        }
        else {
            if(missingCueTracks.contains(info.fileName())) {
                TrackList refoundCueTracks = missingCueTracks.at(cue);
                for(Track& track : refoundCueTracks) {
                    track.setCuePath(cue);
                    tracksToUpdate.push_back(track);
                }
            }
            else {
                const TrackList cueTracks = cueParser->readPlaylist(cue);
                for(const Track& cueTrack : cueTracks) {
                    Track track{cueTrack};
                    setTrackProps(track);
                    tracksToStore.push_back(track);
                    cueFilesScanned.emplace(track.filepath());
                }
            }
        }
    }

    void readFile(const QString& file, const QDir& baseDir, bool onlyModified)
    {
        if(!self->mayRun()) {
            return;
        }

        if(cueFilesScanned.contains(file)) {
            return;
        }

        const QFileInfo info{file};
        const QDateTime lastModifiedTime{info.lastModified()};
        uint64_t lastModified{0};

        if(lastModifiedTime.isValid()) {
            lastModified = static_cast<uint64_t>(lastModifiedTime.toMSecsSinceEpoch());
        }

        auto setTrackProps = [this, &file, &baseDir](Track& track) {
            track.setFilePath(file);
            if(currentLibrary.id >= 0) {
                track.setLibraryId(currentLibrary.id);
                track.setRelativePath(baseDir.relativeFilePath(file));
            }
            track.setIsEnabled(true);
        };

        if(trackPaths.contains(file)) {
            const Track& libraryTrack = trackPaths.at(file);

            if(!libraryTrack.isEnabled() || libraryTrack.libraryId() != currentLibrary.id
               || libraryTrack.modifiedTime() != lastModified || !onlyModified) {
                Track changedTrack{libraryTrack};
                if(Tagging::readMetaData(changedTrack)) {
                    setTrackProps(changedTrack);

                    missingFiles.erase(changedTrack.filename());

                    if(changedTrack.hasExtraTag(QStringLiteral("CUESHEET"))) {
                        const auto cueSheets = changedTrack.extraTag(QStringLiteral("CUESHEET"));
                        TrackList cueTracks  = cueParser->readEmbeddedCue(cueSheets.front(), file);
                        for(Track& cueTrack : cueTracks) {
                            setTrackProps(cueTrack);
                            tracksToUpdate.push_back(cueTrack);
                            missingHashes.erase(cueTrack.hash());
                        }
                    }
                    else {
                        tracksToUpdate.push_back(changedTrack);
                        missingHashes.erase(changedTrack.hash());
                    }
                }
            }
        }
        else {
            Track track{file};

            if(Tagging::readMetaData(track)) {
                Track refoundTrack = matchMissingTrack(missingFiles, missingHashes, track);

                if(refoundTrack.isInLibrary() || refoundTrack.isInDatabase()) {
                    missingHashes.erase(refoundTrack.hash());
                    missingFiles.erase(refoundTrack.filename());

                    setTrackProps(refoundTrack);
                    tracksToUpdate.push_back(refoundTrack);
                }
                else {
                    setTrackProps(track);

                    if(track.hasExtraTag(QStringLiteral("CUESHEET"))) {
                        const auto cueSheets = track.extraTag(QStringLiteral("CUESHEET"));
                        TrackList cueTracks  = cueParser->readEmbeddedCue(cueSheets.front(), file);
                        for(Track& cueTrack : cueTracks) {
                            setTrackProps(cueTrack);
                            tracksToStore.push_back(cueTrack);
                        }
                    }
                    else {
                        tracksToStore.push_back(track);
                    }
                }

                if(tracksToStore.size() >= BatchSize) {
                    storeTracks(tracksToStore);
                    emit self->scanUpdate({.addedTracks = tracksToStore, .updatedTracks = {}});
                    tracksToStore.clear();
                }
            }
        }
    }

    bool getAndSaveAllTracks(const QString& path, const TrackList& tracks, bool onlyModified)
    {
        for(const Track& track : tracks) {
            trackPaths.emplace(track.uniqueFilepath(), track);

            if(track.hasCue()) {
                existingCues.emplace(track.cuePath(), track.modifiedTime());
                if(!QFileInfo::exists(track.cuePath())) {
                    missingCueTracks[track.cuePath()].emplace_back(track);
                }
            }

            if(!QFileInfo::exists(track.filepath())) {
                missingFiles.emplace(track.filename(), track);
                missingHashes.emplace(track.hash(), track);
            }
        }

        const QDir baseDir{path};
        const auto dirs = getDirectories(path);

        tracksProcessed = 0;
        totalTracks     = static_cast<double>(
            std::accumulate(dirs.cbegin(), dirs.cend(), 0, [](int sum, const LibraryDirectory& dir) {
                return sum + static_cast<int>(dir.files.size()) + static_cast<int>(dir.cues.size());
            }));
        currentProgress = 0;

        for(const auto& [_, files, cues] : dirs) {
            for(const auto& cue : cues) {
                if(!self->mayRun()) {
                    return false;
                }

                readCue(cue, baseDir, onlyModified);

                ++tracksProcessed;
                reportProgress();
            }

            for(const auto& file : files) {
                if(!self->mayRun()) {
                    return false;
                }

                readFile(file, baseDir, onlyModified);

                ++tracksProcessed;
                reportProgress();
            }
        }

        for(auto& track : missingFiles | std::views::values) {
            if(track.isInLibrary() || track.isEnabled()) {
                track.setLibraryId(-1);
                track.setIsEnabled(false);
                tracksToUpdate.push_back(track);
            }
        }

        storeTracks(tracksToStore);
        storeTracks(tracksToUpdate);

        if(!tracksToStore.empty() || !tracksToUpdate.empty()) {
            emit self->scanUpdate({tracksToStore, tracksToUpdate});
        }

        return true;
    }

    void changeLibraryStatus(LibraryInfo::Status status)
    {
        currentLibrary.status = status;
        emit self->statusChanged(currentLibrary);
    }
};

LibraryScanner::LibraryScanner(DbConnectionPoolPtr dbPool, SettingsManager* settings, QObject* parent)
    : Worker{parent}
    , p{std::make_unique<Private>(this, std::move(dbPool), settings)}
{ }

LibraryScanner::~LibraryScanner() = default;

void LibraryScanner::initialiseThread()
{
    Worker::initialiseThread();

    p->dbHandler = std::make_unique<DbConnectionHandler>(p->dbPool);
    p->trackDatabase.initialise(DbConnectionProvider{p->dbPool});
}

void LibraryScanner::stopThread()
{
    if(state() == Running) {
        QMetaObject::invokeMethod(
            this, [this]() { emit progressChanged(100); }, Qt::QueuedConnection);
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
        else if(!p->watchers.contains(library.id)) {
            p->addWatcher(library);
            LibraryInfo updatedLibrary{library};
            updatedLibrary.status = LibraryInfo::Status::Monitoring;
            emit statusChanged(updatedLibrary);
        }
    }

    if(!enabled) {
        p->watchers.clear();
    }
}

void LibraryScanner::scanLibrary(const LibraryInfo& library, const TrackList& tracks, bool onlyModified)
{
    setState(Running);

    p->currentLibrary = library;
    p->changeLibraryStatus(LibraryInfo::Status::Scanning);

    if(p->currentLibrary.id >= 0 && QFileInfo::exists(p->currentLibrary.path)) {
        if(p->settings->value<Settings::Core::Internal::MonitorLibraries>() && !p->watchers.contains(library.id)) {
            p->addWatcher(library);
        }
        p->getAndSaveAllTracks(library.path, tracks, onlyModified);
        p->cleanupScan();
    }

    if(state() == Paused) {
        p->changeLibraryStatus(LibraryInfo::Status::Pending);
    }
    else {
        p->changeLibraryStatus(p->settings->value<Settings::Core::Internal::MonitorLibraries>()
                                   ? LibraryInfo::Status::Monitoring
                                   : LibraryInfo::Status::Idle);
        setState(Idle);
        emit finished();
    }
}

void LibraryScanner::scanLibraryDirectory(const LibraryInfo& library, const QString& dir, const TrackList& tracks)
{
    setState(Running);

    p->currentLibrary = library;

    p->changeLibraryStatus(LibraryInfo::Status::Scanning);

    p->getAndSaveAllTracks(dir, tracks, true);
    p->cleanupScan();

    if(state() == Paused) {
        p->changeLibraryStatus(LibraryInfo::Status::Pending);
    }
    else {
        p->changeLibraryStatus(p->settings->value<Settings::Core::Internal::MonitorLibraries>()
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

    TrackFieldMap trackMap;
    std::ranges::transform(libraryTracks, std::inserter(trackMap, trackMap.end()),
                           [](const Track& track) { return std::make_pair(track.filepath(), track); });

    p->tracksProcessed = 0;
    p->totalTracks     = static_cast<double>(tracks.size());
    p->currentProgress = -1;

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

        ++p->tracksProcessed;

        if(trackMap.contains(track.filepath())) {
            tracksScanned.push_back(trackMap.at(track.filepath()));
        }
        else if(Tagging::readMetaData(track)) {
            tracksToStore.push_back(track);
        }

        p->reportProgress();
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

    p->tracksProcessed = 0;
    p->currentProgress = 0;

    const auto handleFinished = [this]() {
        if(state() != Paused) {
            setState(Idle);
            emit finished();
        }
    };

    const LibraryDirectories dirs = getDirectories(urls);

    p->totalTracks
        = static_cast<double>(std::accumulate(dirs.cbegin(), dirs.cend(), 0, [](int sum, const LibraryDirectory& dir) {
              return sum + static_cast<int>(dir.files.size()) + static_cast<int>(dir.cues.size());
          }));
    std::set<QString> cueFilesScanned;

    for(const auto& [_, files, cues] : dirs) {
        if(!mayRun()) {
            handleFinished();
            return;
        }

        for(const auto& cue : cues) {
            const TrackList cueTracks = p->cueParser->readPlaylist(cue);
            for(const Track& cueTrack : cueTracks) {
                const auto trackKey = cueTrack.filepath();
                if(trackMap.contains(trackKey)) {
                    const auto existingTracks = trackMap.equal_range(trackKey);
                    for(auto it = existingTracks.first; it != existingTracks.second; ++it) {
                        if(it->second.uniqueFilepath() == cueTrack.uniqueFilepath()) {
                            tracksScanned.push_back(it->second);
                            break;
                        }
                    }
                }
                else {
                    Track track{cueTrack};
                    track.generateHash();
                    tracksToStore.push_back(track);
                }

                cueFilesScanned.emplace(cueTrack.filepath());
            }

            ++p->tracksProcessed;
            p->reportProgress();
        }

        for(const auto& file : files) {
            if(!cueFilesScanned.contains(file)) {
                if(trackMap.contains(file)) {
                    const auto existingTracks = trackMap.equal_range(file);
                    for(auto it = existingTracks.first; it != existingTracks.second; ++it) {
                        tracksScanned.push_back(it->second);
                    }
                }
                else {
                    Track track{file};
                    if(Tagging::readMetaData(track)) {
                        if(track.hasExtraTag(QStringLiteral("CUESHEET"))) {
                            const auto cueSheets      = track.extraTag(QStringLiteral("CUESHEET"));
                            const TrackList cueTracks = p->cueParser->readEmbeddedCue(cueSheets.front(), file);
                            std::ranges::copy(cueTracks, std::back_inserter(tracksToStore));
                        }
                        else {
                            tracksToStore.push_back(track);
                        }
                    }
                }
            }

            ++p->tracksProcessed;
            p->reportProgress();
        }
    }

    p->storeTracks(tracksToStore);

    emit scannedTracks(tracksToStore, tracksScanned);

    handleFinished();
}
} // namespace Fooyin

#include "moc_libraryscanner.cpp"
