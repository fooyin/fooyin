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

#include "database/database.h"
#include "database/librarydatabase.h"

#include <core/library/libraryinfo.h>
#include <core/tagging/tagreader.h>
#include <core/tagging/tagwriter.h>
#include <core/track.h>
#include <utils/utils.h>

#include <QDir>

namespace Fooyin {
struct LibraryScanner::Private
{
    LibraryScanner* self;
    LibraryInfo library;
    Database* database;
    LibraryDatabase libraryDatabase;
    TagReader tagReader;
    TagWriter tagWriter;

    Private(LibraryScanner* self, Database* database)
        : self{self}
        , database{database}
        , libraryDatabase{database->connectionName()}
    { }

    void storeTracks(TrackList& tracks)
    {
        if(!self->mayRun()) {
            return;
        }

        libraryDatabase.storeTracks(tracks);

        if(!self->mayRun()) {
            return;
        }
    }

    QStringList getFiles(QDir& baseDirectory) const
    {
        QStringList ret;
        QList<QDir> stack{baseDirectory};

        static const QStringList SupportedExtensions{"*.mp3", "*.ogg", "*.opus", "*.oga", "*.m4a",  "*.wav", "*.flac",
                                                     "*.wma", "*.mpc", "*.aiff", "*.ape", "*.webm", "*.mp4"};

        while(!stack.isEmpty()) {
            const QDir dir              = stack.takeFirst();
            const QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
            for(const auto& subDir : subDirs) {
                if(!self->mayRun()) {
                    return {};
                }
                stack.append(QDir{subDir.absoluteFilePath()});
            }
            const QFileInfoList files = dir.entryInfoList(SupportedExtensions, QDir::Files);
            for(const auto& file : files) {
                if(!self->mayRun()) {
                    return {};
                }
                ret.append(file.absoluteFilePath());
            }
        }
        return ret;
    }

    bool getAndSaveAllFiles(const TrackPathMap& tracks)
    {
        QDir dir{library.path};

        TrackList tracksToStore{};
        TrackList tracksToUpdate{};

        const QStringList files = getFiles(dir);

        int tracksProcessed{0};
        auto totalTracks = static_cast<double>(files.size());
        int currentProgress{-1};

        for(const auto& filepath : files) {
            if(!self->mayRun()) {
                return false;
            }

            const QFileInfo info{filepath};
            const QDateTime lastModifiedTime{info.lastModified()};
            uint64_t lastModified{0};

            if(lastModifiedTime.isValid()) {
                lastModified = static_cast<uint64_t>(lastModifiedTime.toMSecsSinceEpoch());
            }

            bool fileWasRead;

            if(tracks.contains(filepath)) {
                const Track& libraryTrack = tracks.at(filepath);
                if(libraryTrack.id() >= 0) {
                    if(libraryTrack.modifiedTime() == lastModified) {
                        totalTracks -= 1;
                        continue;
                    }

                    Track changedTrack{libraryTrack};
                    fileWasRead = tagReader.readMetaData(changedTrack);
                    if(fileWasRead) {
                        // Regenerate hash
                        changedTrack.generateHash();
                        tracksToUpdate.push_back(changedTrack);
                        continue;
                    }
                }
            }

            Track track{filepath};
            track.setLibraryId(library.id);

            fileWasRead = tagReader.readMetaData(track);
            if(fileWasRead) {
                track.generateHash();
                tracksToStore.push_back(track);

                ++tracksProcessed;
                const int progress = static_cast<int>((tracksProcessed / totalTracks) * 100);
                if(currentProgress != progress) {
                    currentProgress = progress;
                    QMetaObject::invokeMethod(self, "progressChanged", Q_ARG(int, currentProgress));
                }

                if(tracksToStore.size() >= 250) {
                    storeTracks(tracksToStore);
                    QMetaObject::invokeMethod(self, "addedTracks", Q_ARG(const TrackList&, tracksToStore));
                    tracksToStore.clear();
                }
            }
        }

        storeTracks(tracksToStore);
        storeTracks(tracksToUpdate);

        if(!tracksToStore.empty()) {
            QMetaObject::invokeMethod(self, "addedTracks", Q_ARG(const TrackList&, tracksToStore));
        }
        if(!tracksToUpdate.empty()) {
            QMetaObject::invokeMethod(self, "updatedTracks", Q_ARG(const TrackList&, tracksToUpdate));
        }

        tracksToStore.clear();
        tracksToUpdate.clear();

        return true;
    }

    void changeLibraryStatus(LibraryInfo::Status status)
    {
        library.status = status;
        QMetaObject::invokeMethod(self, "statusChanged", Q_ARG(const Fooyin::LibraryInfo&, library));
    }
};

LibraryScanner::LibraryScanner(Database* database, QObject* parent)
    : Worker{parent}
    , p{std::make_unique<Private>(this, database)}
{ }

LibraryScanner::~LibraryScanner() = default;

void LibraryScanner::closeThread()
{
    stopThread();
    p->database->closeDatabase();
}

void LibraryScanner::stopThread()
{
    emit progressChanged(100);
    setState(Idle);
    setState(State::Idle);
}

void LibraryScanner::scanLibrary(const LibraryInfo& library, const TrackList& tracks)
{
    if(state() == Running) {
        return;
    }
    setState(Running);

    p->library = library;

    p->changeLibraryStatus(LibraryInfo::Status::Scanning);

    TrackPathMap trackMap{};
    TrackList tracksToDelete{};

    if(!Utils::File::exists(p->library.path)) {
        // Root dir doesn't exist so leave to user to delete
        return;
    }
    for(const Track& track : tracks) {
        if(!Utils::File::exists(track.filepath())) {
            tracksToDelete.emplace_back(track);
        }
        else {
            trackMap.emplace(track.filepath(), track);
        }
    }

    const bool deletedSuccess = p->libraryDatabase.deleteTracks(tracksToDelete);

    if(deletedSuccess && !tracksToDelete.empty()) {
        emit tracksDeleted(tracksToDelete);
    }

    p->getAndSaveAllFiles(trackMap);

    setState(Idle);
    p->changeLibraryStatus(LibraryInfo::Status::Idle);

    emit finished();
}

void LibraryScanner::updateTracks(const TrackList& tracks)
{
    for(const Track& track : tracks) {
        const bool saved = p->tagWriter.writeMetaData(track);
        if(saved) {
            p->libraryDatabase.updateTrack(track);
        }
    }
}
} // namespace Fooyin

#include "moc_libraryscanner.cpp"
