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
#include "core/tagging/tagreader.h"

#include <utils/utils.h>

#include <QDir>

using namespace Fy::Utils;

namespace Fy::Core::Library {
LibraryScanner::LibraryScanner(LibraryInfo* info, DB::Database* database, QObject* parent)
    : Worker{parent}
    , m_library{info}
    , m_database{database}
    , m_libraryDatabase{database->connectionName(), info->id}
{ }

void LibraryScanner::closeThread()
{
    stopThread();
    m_database->closeDatabase();
}

void LibraryScanner::stopThread()
{
    setState(State::Idle);
}

void LibraryScanner::scanLibrary(const TrackList& tracks)
{
    if(!m_library) {
        return;
    }

    setState(Running);

    TrackPathMap trackMap{};
    TrackList tracksToDelete{};

    // TODO: Don't delete if disk/top level is inaccessible
    //       and ask for confirmation.
    for(const Track& track : tracks) {
        if(!File::exists(track.filepath())) {
            tracksToDelete.emplace_back(track);
        }
        else {
            trackMap.emplace(track.filepath(), track);
            if(track.hasCover() && !File::exists(track.coverPath())) {
                m_tagReader.storeCover(track);
            }
        }
        if(!mayRun()) {
            return;
        }
    }

    const bool deletedSuccess = m_libraryDatabase.deleteTracks(tracksToDelete);

    if(deletedSuccess && !tracksToDelete.empty()) {
        emit tracksDeleted(tracksToDelete);
    }

    if(!mayRun()) {
        return;
    }

    getAndSaveAllFiles(trackMap);

    setState(Idle);
}

void LibraryScanner::updateTracks(const TrackList& tracks)
{
    for(const Track& track : tracks) {
        const bool saved = m_tagReader.writeMetaData(track);
        if(saved) {
            m_libraryDatabase.updateTrack(track);
        }
    }
}

void LibraryScanner::storeTracks(TrackList& tracks)
{
    if(!mayRun()) {
        return;
    }

    if(storeCovers(tracks)) {
        m_libraryDatabase.storeTracks(tracks);
    }

    if(!mayRun()) {
        return;
    }
}

bool LibraryScanner::storeCovers(TrackList& tracks)
{
    if(tracks.empty()) {
        return false;
    }

    for(auto& track : tracks) {
        if(track.libraryId() < 0) {
            track.setLibraryId(m_library->id);
        }

        if(!track.hasCover()) {
            const QString coverPath = m_tagReader.storeCover(track);
            track.setCoverPath(coverPath);
        }
    }
    return true;
}

QStringList LibraryScanner::getFiles(QDir& baseDirectory)
{
    QStringList ret;
    QList<QDir> stack{baseDirectory};

    const QStringList soundFileExtensions{"*.mp3",
                                          "*.ogg",
                                          "*.opus",
                                          "*.oga",
                                          "*.m4a",
                                          "*.wav",
                                          "*.flac",
                                          "*.aac",
                                          "*.wma",
                                          "*.mpc",
                                          "*.aiff",
                                          "*.ape",
                                          "*.webm",
                                          "*.mp4"};

    while(!stack.isEmpty()) {
        const QDir dir              = stack.takeFirst();
        const QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for(const auto& subDir : subDirs) {
            if(!mayRun()) {
                return {};
            }
            stack.append(QDir{subDir.absoluteFilePath()});
        }
        const QFileInfoList files = dir.entryInfoList(soundFileExtensions, QDir::Files);
        for(const auto& file : files) {
            if(!mayRun()) {
                return {};
            }
            ret.append(file.absoluteFilePath());
        }
    }
    return ret;
}

bool LibraryScanner::getAndSaveAllFiles(const TrackPathMap& tracks)
{
    QDir dir(m_library->path);

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
            const Track& libraryTrack = tracks.at(filepath);
            if(libraryTrack.id() >= 0) {
                if(libraryTrack.modifiedTime() == lastModified) {
                    continue;
                }

                Track changedTrack{libraryTrack};
                fileWasRead = m_tagReader.readMetaData(changedTrack, Tagging::Quality::Fast);
                if(fileWasRead) {
                    // Regenerate hash
                    changedTrack.generateHash();
                    tracksToUpdate.emplace_back(changedTrack);
                    continue;
                }
            }
        }

        Track track{filepath};
        track.setLibraryId(m_library->id);

        fileWasRead = m_tagReader.readMetaData(track, Tagging::Quality::Quality);
        if(fileWasRead) {
            track.generateHash();
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
} // namespace Fy::Core::Library
