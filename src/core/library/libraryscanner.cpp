/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "core/database/database.h"
#include "core/database/librarydatabase.h"
#include "core/tagging/tags.h"
#include "librarymanager.h"
#include "libraryutils.h"

#include <utils/utils.h>

#include <QDir>

namespace Core::Library {
LibraryScanner::LibraryScanner(LibraryManager* libraryManager, DB::Database* database, QObject* parent)
    : Worker{parent}
    , m_libraryManager{libraryManager}
    , m_database{database}
{ }

LibraryScanner::~LibraryScanner()
{
    m_database->closeDatabase();
}

void LibraryScanner::stopThread()
{
    setState(State::Idle);
}

void LibraryScanner::scanLibrary(const TrackPtrList& tracks, LibraryInfo* info)
{
    if(!info) {
        return;
    }

    if(isRunning()) {
        const LibraryQueueEntry libraryEntry{info, tracks};
        m_libraryQueue.emplace_back(libraryEntry);
        return;
    }

    DB::LibraryDatabase* libraryDatabase = m_database->libraryDatabase();

    setState(Running);

    TrackPathMap trackMap{};
    IdSet tracksToDelete{};

    // TODO: Don't delete if disk/top level is inaccessible
    //       and ask for confirmation.
    for(const auto& track : tracks) {
        if(!::Utils::File::exists(track->filepath())) {
            tracksToDelete.insert(track->id());
        }
        else {
            trackMap.emplace(track->filepath(), track);
            if(track->hasCover() && !::Utils::File::exists(track->coverPath())) {
                Utils::storeCover(*track);
            }
        }
        if(!mayRun()) {
            return;
        }
    }

    const bool deletedSuccess = libraryDatabase->deleteTracks(tracksToDelete);

    if(deletedSuccess && !tracksToDelete.empty()) {
        emit tracksDeleted(tracksToDelete);
    }

    if(!mayRun()) {
        return;
    }

    getAndSaveAllFiles(info->id, info->path, trackMap);

    setState(Idle);

    processQueue();
}

void LibraryScanner::scanAll(const TrackPtrList& tracks)
{
    const auto& libraries = m_libraryManager->allLibraries();

    for(const auto& info : libraries) {
        scanLibrary(tracks, info.get());
    }
}

void LibraryScanner::storeTracks(TrackList& tracks) const
{
    if(!mayRun()) {
        return;
    }

    DB::LibraryDatabase* libraryDatabase = m_database->libraryDatabase();

    libraryDatabase->storeTracks(tracks);

    if(!mayRun()) {
        return;
    }
}

QStringList LibraryScanner::getFiles(QDir& baseDirectory)
{
    QStringList ret;
    QList<QDir> stack{baseDirectory};

    const QStringList soundFileExtensions{"*.mp3", "*.ogg", "*.opus", "*.oga",  "*.m4a", "*.wav",  "*.flac",
                                          "*.aac", "*.wma", "*.mpc",  "*.aiff", "*.ape", "*.webm", "*.mp4"};

    while(!stack.isEmpty()) {
        const QDir dir = stack.takeFirst();
        for(const auto& subDir : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if(!mayRun()) {
                return {};
            }
            stack.append(QDir{subDir.absoluteFilePath()});
        }
        for(const auto& file : dir.entryInfoList(soundFileExtensions, QDir::Files)) {
            if(!mayRun()) {
                return {};
            }
            ret.append(file.absoluteFilePath());
        }
    }
    return ret;
}

bool LibraryScanner::getAndSaveAllFiles(int libraryId, const QString& path, const TrackPathMap& tracks)
{
    if(path.isEmpty() || !::Utils::File::exists(path)) {
        return false;
    }

    QDir dir(path);

    TrackList tracksToStore{};
    TrackList tracksToUpdate{};

    const QStringList files = getFiles(dir);

    for(const auto& filepath : files) {
        if(!mayRun()) {
            return false;
        }

        const QFileInfo info{filepath};
        const QDateTime lastModifiedTime{info.lastModified()};
        uint64_t lastModified{0};

        if(lastModifiedTime.isValid()) {
            lastModified = static_cast<uint64_t>(lastModifiedTime.toMSecsSinceEpoch());
        };

        bool fileWasRead;

        if(tracks.count(filepath)) {
            const Track* libraryTrack = tracks.at(filepath);
            if(libraryTrack->id() >= 0) {
                if(libraryTrack->modifiedTime() == lastModified) {
                    continue;
                }

                Track changedTrack{*libraryTrack};
                changedTrack.resetIds();
                fileWasRead = Tagging::readMetaData(changedTrack, Tagging::Quality::Fast);
                if(fileWasRead) {
                    tracksToUpdate.emplace_back(changedTrack);
                    continue;
                }
            }
        }

        Track track{filepath};
        track.setLibraryId(libraryId);

        fileWasRead = Tagging::readMetaData(track, Tagging::Quality::Quality);
        if(fileWasRead) {
            tracksToStore.emplace_back(track);
            if(tracksToStore.size() >= 500) {
                storeTracks(tracksToStore);
                emit addedTracks(tracksToStore);
                tracksToStore.clear();
            }
        }
    }

    storeTracks(tracksToStore);
    storeTracks(tracksToUpdate);

    if(!tracksToStore.empty()) {
        emit addedTracks(tracksToStore);
    }
    if(!tracksToUpdate.empty()) {
        emit updatedTracks(tracksToUpdate);
    }

    tracksToStore.clear();

    return true;
}

void LibraryScanner::processQueue()
{
    if(!m_libraryQueue.empty()) {
        const LibraryQueueEntry libraryEntry = m_libraryQueue.front();
        m_libraryQueue.pop_front();
        scanLibrary(libraryEntry.tracks, libraryEntry.library);
    }
}
} // namespace Core::Library
