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

#include "database/database.h"
#include "database/trackdatabase.h"
#include "internalcoresettings.h"
#include "library/libraryinfo.h"
#include "librarywatcher.h"
#include "tagging/tagreader.h"

#include <core/track.h>
#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>

#include <QDir>
#include <QFileSystemWatcher>

#include <ranges>

constexpr auto BatchSize = 250;

namespace {
Fooyin::Track matchMissingTrack(const Fooyin::TrackFieldMap& missingFiles, const Fooyin::TrackFieldMap& missingHashes,
                                Fooyin::Track& track)
{
    const QString filename = track.filename();
    const QString hash     = track.generateHash();

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

    int tracksProcessed{0};
    double totalTracks{0};
    int currentProgress{-1};

    std::unordered_map<int, LibraryWatcher> watchers;

    Private(LibraryScanner* self_, DbConnectionPoolPtr dbPool_, SettingsManager* settings_)
        : self{self_}
        , dbPool{std::move(dbPool_)}
        , settings{settings_}
    { }

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

    bool getAndSaveAllTracks(const QString& path, const TrackList& tracks)
    {
        const QDir dir{path};

        TrackList tracksToStore;
        TrackList tracksToUpdate;

        TrackFieldMap trackPaths;
        TrackFieldMap missingFiles;
        TrackFieldMap missingHashes;

        for(const Track& track : tracks) {
            trackPaths.emplace(track.filepath(), track);

            if(!QFileInfo::exists(track.filepath())) {
                missingFiles.emplace(track.filename(), track);
                missingHashes.emplace(track.hash(), track);
            }
        }

        const QStringList files = Utils::File::getFilesInDir(dir, Track::supportedFileExtensions());

        tracksProcessed = 0;
        totalTracks     = static_cast<double>(files.size());
        currentProgress = -1;

        for(const auto& filepath : files) {
            if(!self->mayRun()) {
                return false;
            }

            ++tracksProcessed;

            const QFileInfo info{filepath};
            const QDateTime lastModifiedTime{info.lastModified()};
            uint64_t lastModified{0};

            if(lastModifiedTime.isValid()) {
                lastModified = static_cast<uint64_t>(lastModifiedTime.toMSecsSinceEpoch());
            }

            auto setTrackProps = [this, &filepath, &dir](Track& track) {
                track.setFilePath(filepath);
                track.setLibraryId(currentLibrary.id);
                track.setRelativePath(dir.relativeFilePath(filepath));
                track.setIsEnabled(true);
            };

            if(trackPaths.contains(filepath)) {
                const Track& libraryTrack = trackPaths.at(filepath);

                if(!libraryTrack.isEnabled() || libraryTrack.libraryId() != currentLibrary.id
                   || libraryTrack.modifiedTime() != lastModified) {
                    Track changedTrack{libraryTrack};
                    if(Tagging::readMetaData(changedTrack)) {
                        changedTrack.generateHash();
                        setTrackProps(changedTrack);

                        tracksToUpdate.push_back(changedTrack);
                        missingHashes.erase(changedTrack.hash());
                        missingFiles.erase(changedTrack.filename());
                    }
                }
            }
            else {
                Track track{filepath};

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
                        tracksToStore.push_back(track);
                    }

                    if(tracksToStore.size() >= BatchSize) {
                        storeTracks(tracksToStore);
                        emit self->scanUpdate({.addedTracks = tracksToStore});
                        tracksToStore.clear();
                    }
                }
            }

            reportProgress();
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
    p->dbHandler = std::make_unique<DbConnectionHandler>(p->dbPool);

    p->trackDatabase.initialise(DbConnectionProvider{p->dbPool});
}

void LibraryScanner::stopThread()
{
    if(state() == Running) {
        emit progressChanged(100);
    }

    setState(Idle);
}

void LibraryScanner::setupWatchers(const LibraryInfoMap& libraries, bool enabled)
{
    for(const auto& library : libraries | std::views::values) {
        if(!enabled && library.status == LibraryInfo::Status::Monitoring) {
            LibraryInfo updatedLibrary{library};
            updatedLibrary.status = LibraryInfo::Status::Idle;
            emit statusChanged(updatedLibrary);
        }
        else if(!p->watchers.contains(library.id)) {
            p->addWatcher(library);
        }
    }

    if(!enabled) {
        p->watchers.clear();
    }
}

void LibraryScanner::scanLibrary(const LibraryInfo& library, const TrackList& tracks)
{
    setState(Running);

    p->currentLibrary = library;

    p->changeLibraryStatus(LibraryInfo::Status::Scanning);

    if(p->currentLibrary.id >= 0 && QFileInfo::exists(p->currentLibrary.path)) {
        if(p->settings->value<Settings::Core::Internal::MonitorLibraries>() && !p->watchers.contains(library.id)) {
            p->addWatcher(library);
        }
        p->getAndSaveAllTracks(library.path, tracks);
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

    p->getAndSaveAllTracks(dir, tracks);

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
            track.generateHash();
            tracksToStore.push_back(track);
        }

        p->reportProgress();
    }

    p->storeTracks(tracksToStore);

    std::ranges::copy(tracksToStore, std::back_inserter(tracksScanned));

    emit scannedTracks(tracksScanned);

    handleFinished();
}
} // namespace Fooyin

#include "moc_libraryscanner.cpp"
