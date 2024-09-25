/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "fileopsworker.h"

#include "fileopsregistry.h"

#include <core/internalcoresettings.h>
#include <core/library/libraryinfo.h>
#include <core/library/musiclibrary.h>
#include <core/scripting/scriptparser.h>
#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>

#include <QLoggingCategory>
#include <QRegularExpression>

Q_LOGGING_CATEGORY(FILEOPS, "fy.fileops")

namespace Fooyin::FileOps {
FileOpsWorker::FileOpsWorker(MusicLibrary* library, TrackList tracks, SettingsManager* settings, QObject* parent)
    : Worker{parent}
    , m_library{library}
    , m_settings{settings}
    , m_scriptParser{new FileOpsRegistry()}
    , m_tracks{std::move(tracks)}
    , m_isMonitoring{settings->value<Settings::Core::Internal::MonitorLibraries>()}
{ }

void FileOpsWorker::simulate(const FileOpPreset& preset)
{
    setState(Running);

    reset();
    m_preset = preset;

    const auto tracks = m_library->tracks();
    for(const Track& track : tracks) {
        if(!mayRun()) {
            return;
        }
        m_trackPaths.emplace(track.filepath(), track);
    }

    if(!preset.dest.isEmpty() || m_preset.op == Operation::Rename) {
        switch(m_preset.op) {
            case(Operation::Copy):
                simulateCopy();
                break;
            case(Operation::Move):
                simulateMove();
                break;
            case(Operation::Rename):
                simulateRename();
                break;
            case(Operation::Create):
            case(Operation::Remove):
                break;
        }
    }

    if(mayRun()) {
        emit simulated(m_operations);
    }

    setState(Idle);
}

void FileOpsWorker::run()
{
    setState(Running);

    if(m_isMonitoring) {
        m_settings->set<Settings::Core::Internal::MonitorLibraries>(false);
    }

    while(!m_operations.empty()) {
        if(!mayRun()) {
            return;
        }

        const FileOpsItem& item = m_operations.front();

        switch(item.op) {
            case(Operation::Create): {
                if(!QDir{}.mkpath(item.destination)) {
                    qCWarning(FILEOPS) << "Failed to create directory" << item.destination;
                }
                break;
            }
            case(Operation::Remove): {
                if(!QDir{}.rmdir(item.source)) {
                    qCWarning(FILEOPS) << "Failed to remove directory" << item.destination;
                }
                break;
            }
            case(Operation::Rename):
            case(Operation::Move): {
                renameFile(item);
                break;
            }
            case(Operation::Copy): {
                copyFile(item);
                break;
            }
        }

        emit operationFinished(item);
        m_operations.pop_front();
    }

    if(!m_tracksToUpdate.empty()) {
        m_library->updateTrackMetadata(m_tracksToUpdate);
    }

    setState(Idle);

    if(m_isMonitoring) {
        m_settings->set<Settings::Core::Internal::MonitorLibraries>(true);
    }
}

void FileOpsWorker::simulateMove()
{
    const QString path        = m_preset.dest + u"/" + m_preset.filename + QStringLiteral(".%extension%");
    const ParsedScript script = m_scriptParser.parse(path);

    std::set<QString> sourcePaths;

    for(const Track& track : m_tracks) {
        if(!mayRun()) {
            return;
        }

        const QString srcPath = track.path();
        if(m_tracksProcessed.contains(track.filepath()) || sourcePaths.contains(srcPath)) {
            continue;
        }

        m_tracksProcessed.emplace(track.filepath());

        const QString destFilepath = QDir::cleanPath(m_scriptParser.evaluate(script, track));
        if(track.filepath() == destFilepath) {
            // Nothing to do
            continue;
        }

        const QString destPath = QFileInfo{destFilepath}.absolutePath();

        if(m_preset.wholeDir) {
            sourcePaths.emplace(srcPath);

            const QDir srcDir{srcPath};

            const auto files = Utils::File::getFilesInDirRecursive(srcPath);
            for(const QString& file : files) {
                const QFileInfo info{file};
                const QDir fileDir{info.absolutePath()};

                handleEmptyDirs(fileDir, file);

                const QString relativePath = srcDir.relativeFilePath(file);
                const QString fileDestPath = QDir::cleanPath(destPath + u"/" + relativePath);

                const QString parentPath = QFileInfo{fileDestPath}.absolutePath();
                createDir(parentPath);

                if(m_trackPaths.contains(file)) {
                    const Track fileTrack  = m_trackPaths.equal_range(file).first->second;
                    const QString filePath = QDir::cleanPath(m_scriptParser.evaluate(script, fileTrack));
                    m_operations.emplace_back(Operation::Move, fileTrack.filenameExt(), fileTrack.filepath(), filePath);
                }
                else {
                    m_operations.emplace_back(Operation::Move, info.fileName(), file, fileDestPath);
                }
            }
        }
        else {
            const QDir trackDir{srcPath};
            handleEmptyDirs(trackDir, track.filepath());

            createDir(destPath);

            m_operations.emplace_back(Operation::Move, track.filenameExt(), track.filepath(), destFilepath);
        }
    }

    if(m_currentDir && !m_filesToMove.empty()) {
        addEmptyDirs(m_currentDir.value());
    }
}

void FileOpsWorker::simulateCopy()
{
    const QString path        = m_preset.dest + u"/" + m_preset.filename + QStringLiteral(".%extension%");
    const ParsedScript script = m_scriptParser.parse(path);

    std::set<QString> sourcePaths;

    for(const Track& track : m_tracks) {
        if(!mayRun()) {
            return;
        }

        const QString srcPath = track.path();
        if(m_tracksProcessed.contains(track.filepath()) || sourcePaths.contains(srcPath)) {
            continue;
        }

        m_tracksProcessed.emplace(track.filepath());

        const QString destFilepath = QDir::cleanPath(m_scriptParser.evaluate(script, track));
        if(track.filepath() == destFilepath) {
            // Nothing to do
            continue;
        }

        const QString destPath = QFileInfo{destFilepath}.absolutePath();

        if(m_preset.wholeDir) {
            sourcePaths.emplace(srcPath);

            const QDir srcDir{srcPath};

            const auto files = Utils::File::getFilesInDirRecursive(srcPath);
            for(const QString& file : files) {
                const QString relativePath = srcDir.relativeFilePath(file);
                const QString fileDestPath = QDir::cleanPath(destPath + u"/" + relativePath);

                const QString parentPath = QFileInfo{fileDestPath}.absolutePath();
                createDir(parentPath);

                if(m_trackPaths.contains(file)) {
                    const Track fileTrack = m_trackPaths.equal_range(file).first->second;
                    m_operations.emplace_back(Operation::Copy, fileTrack.filenameExt(), fileTrack.filepath(),
                                              fileDestPath);
                }
                else {
                    const QFileInfo info{file};
                    m_operations.emplace_back(Operation::Copy, info.fileName(), file, fileDestPath);
                }
            }
        }
        else {
            createDir(destPath);

            m_operations.emplace_back(Operation::Copy, track.filenameExt(), track.filepath(), destFilepath);
        }
    }
}

void FileOpsWorker::simulateRename()
{
    const QString path        = m_preset.filename + QStringLiteral(".%extension%");
    const ParsedScript script = m_scriptParser.parse(path);

    for(const Track& track : m_tracks) {
        if(!mayRun()) {
            return;
        }

        if(m_tracksProcessed.contains(track.filepath())) {
            continue;
        }

        m_tracksProcessed.emplace(track.filepath());

        QString destFilename = QDir::cleanPath(m_scriptParser.evaluate(script, track));
        destFilename         = FileOpsRegistry::replaceSeparators(destFilename);

        if(track.filenameExt() == destFilename) {
            continue;
        }

        const QString destFilepath = QDir::cleanPath(track.path() + u"/" + destFilename);

        m_operations.emplace_back(Operation::Rename, track.filenameExt(), track.filepath(), destFilepath);
    }
}

void FileOpsWorker::renameFile(const FileOpsItem& item)
{
    QFile file{item.source};

    if(!file.exists()) {
        qCWarning(FILEOPS) << "File doesn't exist:" << item.source;
        return;
    }

    if(!file.rename(item.destination)) {
        qCWarning(FILEOPS) << "Failed to move file from" << item.source << "to" << item.destination;
        return;
    }

    if(m_trackPaths.contains(item.source)) {
        auto tracks = m_trackPaths.equal_range(item.source);
        for(auto it = tracks.first; it != tracks.second; ++it) {
            auto& track = it->second;

            if(track.hasCue() && m_filesToMove.contains(track.cuePath())) {
                const QString cuePath = track.cuePath();

                const QDir srcDir{track.path()};
                const QString relativeCuePath = srcDir.relativeFilePath(cuePath);

                track.setFilePath(item.destination);
                const QString cueDest = QDir::cleanPath(track.path() + u"/" + relativeCuePath);
                track.setCuePath(cueDest);
            }
            else {
                track.setFilePath(item.destination);
            }

            if(const auto library = m_library->libraryForPath(item.destination)) {
                if(track.libraryId() != library->id) {
                    track.setLibraryId(library->id);
                }
            }
            else {
                track.setLibraryId(-1);
            }

            m_tracksToUpdate.push_back(track);
        }
    }
}

void FileOpsWorker::copyFile(const FileOpsItem& item)
{
    QFile file{item.source};

    if(!file.exists()) {
        qCWarning(FILEOPS) << "File doesn't exist:" << item.source;
        return;
    }

    if(!file.copy(item.destination)) {
        qCWarning(FILEOPS) << "Failed to copy file from" << item.source << "to" << item.destination;
    }
}

void FileOpsWorker::createDir(const QDir& dir)
{
    if(!m_dirsToCreate.contains(dir.absolutePath()) && !dir.exists()) {
        FileOpsItem item;
        item.op          = Operation::Create;
        item.destination = dir.absolutePath();
        m_operations.push_back(item);
        m_dirsToCreate.emplace(item.destination);
    }
}

void FileOpsWorker::removeDir(const QDir& dir)
{
    FileOpsItem item;
    item.op     = Operation::Remove;
    item.name   = dir.dirName();
    item.source = dir.absolutePath();
    m_operations.push_back(item);
}

void FileOpsWorker::reset()
{
    m_operations.clear();
    m_currentDir = {};
    m_tracksProcessed.clear();
    m_filesToMove.clear();
    m_dirsToCreate.clear();
    m_dirsToRemove.clear();
    m_tracksToUpdate.clear();
    m_trackPaths.clear();
}

void FileOpsWorker::handleEmptyDirs(const QDir& dir, const QString& filepath)
{
    if(m_preset.removeEmpty) {
        if(m_currentDir && m_currentDir != dir) {
            addEmptyDirs(m_currentDir.value());
            m_filesToMove.clear();
        }
        m_currentDir = dir;
        m_filesToMove.emplace(filepath);
    }
}

void FileOpsWorker::addEmptyDirs(const QDir& dir)
{
    QDir currentDir{dir};
    QString dirToRemove;

    while(true) {
        const QFileInfoList files = currentDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        bool isEmpty{true};

        for(const QFileInfo& fileInfo : files) {
            if(!m_filesToMove.contains(fileInfo.absoluteFilePath())) {
                isEmpty = false;
                break;
            }
        }

        if(isEmpty) {
            const QFileInfoList dirs = currentDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
            for(const QFileInfo& dirInfo : dirs) {
                const QDir subDir{dirInfo.absoluteFilePath()};
                if(!m_dirsToRemove.contains(subDir.absolutePath()) && !subDir.isEmpty()) {
                    isEmpty = false;
                    break;
                }
            }
            if(isEmpty) {
                m_dirsToRemove.emplace(currentDir.absolutePath());
                dirToRemove = currentDir.absolutePath();
            }
        }
        else {
            break;
        }

        if(!currentDir.cdUp()) {
            break;
        }
    }

    if(!dirToRemove.isEmpty()) {
        removeDir(dirToRemove);
    }
}
} // namespace Fooyin::FileOps
