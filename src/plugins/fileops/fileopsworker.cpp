/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <core/engine/audioinput.h>
#include <core/engine/audioloader.h>
#include <core/internalcoresettings.h>
#include <core/library/libraryinfo.h>
#include <core/library/musiclibrary.h>
#include <core/scripting/scriptparser.h>
#include <utils/fileutils.h>
#include <utils/settings/settingsmanager.h>

#include <QLoggingCategory>
#include <QRegularExpression>

#include <ranges>

Q_LOGGING_CATEGORY(FILEOPS, "fy.fileops")

using namespace Qt::StringLiterals;

namespace {
class FileOpsScriptEnvironment : public Fooyin::ScriptEnvironment,
                                 public Fooyin::ScriptEvaluationEnvironment
{
public:
    [[nodiscard]] const ScriptEvaluationEnvironment* evaluationEnvironment() const override
    {
        return this;
    }

    [[nodiscard]] Fooyin::TrackListContextPolicy trackListContextPolicy() const override
    {
        return Fooyin::TrackListContextPolicy::Unresolved;
    }

    [[nodiscard]] QString trackListPlaceholder() const override
    {
        return {};
    }

    [[nodiscard]] bool escapeRichText() const override
    {
        return false;
    }

    [[nodiscard]] bool replacePathSeparators() const override
    {
        return true;
    }
};

QString replaceSeparators(const QString& input)
{
    static const QRegularExpression regex{uR"([/\\])"_s};
    QString output{input};
    return output.replace(regex, "-"_L1);
}

QString cleanArchiveEntryPath(const QString& path)
{
    QString cleaned = QDir::cleanPath(QDir::fromNativeSeparators(path));
    if(cleaned.isEmpty() || cleaned == "."_L1 || cleaned == ".."_L1 || cleaned.startsWith("../"_L1)
       || cleaned.contains("/../"_L1) || QDir::isAbsolutePath(cleaned) || cleaned.contains(":"_L1)) {
        return {};
    }
    return cleaned;
}

QString entryPathRelativeToTrackLevel(const QString& entryPath, const Fooyin::Track& track)
{
    const QString trackLevel = cleanArchiveEntryPath(track.relativeArchivePath());
    if(trackLevel.isEmpty() || trackLevel == "."_L1) {
        return entryPath;
    }

    const QString prefix = trackLevel + "/"_L1;
    if(entryPath.startsWith(prefix)) {
        return entryPath.mid(prefix.size());
    }

    return entryPath;
}

QString archiveEntryKey(const QString& archivePath, const QString& entryPath)
{
    return archivePath + "\n"_L1 + entryPath;
}
} // namespace

namespace Fooyin::FileOps {
FileOpsWorker::FileOpsWorker(MusicLibrary* library, std::shared_ptr<AudioLoader> audioLoader, TrackList tracks,
                             SettingsManager* settings, QObject* parent)
    : Worker{parent}
    , m_library{library}
    , m_audioLoader{std::move(audioLoader)}
    , m_settings{settings}
    , m_tracks{std::move(tracks)}
    , m_isMonitoring{settings->value<Settings::Core::Internal::MonitorLibraries>()}
{ }

void FileOpsWorker::simulate(const FileOpPreset& preset)
{
    prepareOperations(preset, true);
}

void FileOpsWorker::deleteFiles()
{
    setState(Running);

    reset();

    if(!populateTrackPaths()) {
        setState(Idle);
        Q_EMIT deleteFinished({});
        Q_EMIT finished();
        return;
    }

    if(m_isMonitoring) {
        m_settings->set<Settings::Core::Internal::MonitorLibraries>(false);
    }

    const auto appendDeletedTrack = [this](const Track& deletedTrack) {
        if(std::ranges::find(m_tracksToDelete, deletedTrack) == m_tracksToDelete.cend()) {
            m_tracksToDelete.push_back(deletedTrack);
        }
    };

    for(const Track& track : m_tracks) {
        if(!mayRun()) {
            break;
        }

        const QString filepath = track.filepath();
        if(m_tracksProcessed.contains(filepath)) {
            continue;
        }
        m_tracksProcessed.emplace(filepath);

        if(!QFile::moveToTrash(filepath)) {
            qCWarning(FILEOPS) << "Failed to delete file" << filepath;
            continue;
        }

        appendDeletedTrack(track);

        if(m_trackPaths.contains(filepath)) {
            auto range = m_trackPaths.equal_range(filepath);
            for(auto it = range.first; it != range.second; ++it) {
                appendDeletedTrack(it->second);
            }
        }
    }

    if(!m_tracksToDelete.empty()) {
        m_library->deleteTracks(m_tracksToDelete);
    }

    setState(Idle);

    if(m_isMonitoring) {
        m_settings->set<Settings::Core::Internal::MonitorLibraries>(true);
    }

    Q_EMIT deleteFinished(m_tracksToDelete);
    Q_EMIT finished();
}

bool FileOpsWorker::prepareOperations(const FileOpPreset& preset, bool emitSimulation)
{
    setState(Running);

    reset();
    m_preset = preset;

    const bool canContinue = populateTrackPaths();

    if(canContinue && (!preset.dest.isEmpty() || m_preset.op == Operation::Rename)) {
        switch(m_preset.op) {
            case Operation::Copy:
                simulateCopy();
                break;
            case Operation::Extract:
                simulateExtract();
                break;
            case Operation::Move:
                simulateMove();
                break;
            case Operation::Rename:
                simulateRename();
                break;
            case Operation::Create:
            case Operation::Remove:
            case Operation::Delete:
            case Operation::RemoveArchive:
                break;
        }
    }

    const bool shouldContinue = canContinue && mayRun();
    if(emitSimulation && shouldContinue) {
        Q_EMIT simulated(m_operations);
    }

    setState(Idle);

    return shouldContinue;
}

bool FileOpsWorker::populateTrackPaths()
{
    const auto tracks = m_library->tracks();
    for(const Track& track : tracks) {
        if(!mayRun()) {
            return false;
        }
        m_trackPaths.emplace(track.filepath(), track);
    }

    return true;
}

void FileOpsWorker::run()
{
    setState(Running);

    if(m_isMonitoring) {
        m_settings->set<Settings::Core::Internal::MonitorLibraries>(false);
    }

    while(!m_operations.empty()) {
        if(!mayRun()) {
            break;
        }

        const FileOpsItem& item = m_operations.front();

        switch(item.op) {
            case Operation::Create: {
                const bool success = QDir{}.mkpath(item.destination);
                if(!success) {
                    qCWarning(FILEOPS) << "Failed to create directory" << item.destination;
                }
                break;
            }
            case Operation::Remove: {
                const bool success = QDir{}.rmdir(item.source);
                if(!success) {
                    qCWarning(FILEOPS) << "Failed to remove directory" << item.destination;
                }
                break;
            }
            case Operation::Rename:
            case Operation::Move: {
                renameFile(item);
                break;
            }
            case Operation::Copy: {
                copyFile(item);
                break;
            }
            case Operation::Extract: {
                if(extractFile(item)) {
                    m_successfulArchives.emplace(item.archivePath);
                    if(m_preset.removeSourceArchive && m_trackPaths.contains(item.source)) {
                        m_extractedTrackDestinations.emplace(item.source, item.destination);
                    }
                }
                else {
                    m_failedArchives.emplace(item.archivePath);
                }
                break;
            }
            case Operation::RemoveArchive: {
                removeArchive(item);
                break;
            }
            case Operation::Delete:
                break;
        }

        Q_EMIT operationFinished(item);
        m_operations.pop_front();
    }

    if(!m_tracksToUpdate.empty()) {
        m_library->updateTrackMetadata(m_tracksToUpdate);
    }

    if(!m_tracksToDelete.empty()) {
        m_library->deleteTracks(m_tracksToDelete);
    }

    setState(Idle);

    if(m_isMonitoring) {
        m_settings->set<Settings::Core::Internal::MonitorLibraries>(true);
    }

    Q_EMIT finished();
}

void FileOpsWorker::simulateMove()
{
    const QString path        = m_preset.dest + "/"_L1 + m_preset.filename + u".%extension%"_s;
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

        const QString destFilepath = QDir::cleanPath(evaluatePath(script, track));
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
                const QString fileDestPath = QDir::cleanPath(destPath + "/"_L1 + relativePath);

                const QString parentPath = QFileInfo{fileDestPath}.absolutePath();
                createDir(parentPath);

                if(m_trackPaths.contains(file)) {
                    const Track fileTrack  = m_trackPaths.equal_range(file).first->second;
                    const QString filePath = QDir::cleanPath(evaluatePath(script, fileTrack));
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
    const QString path        = m_preset.dest + "/"_L1 + m_preset.filename + u".%extension%"_s;
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

        const QString destFilepath = QDir::cleanPath(evaluatePath(script, track));
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
                const QString fileDestPath = QDir::cleanPath(destPath + "/"_L1 + relativePath);

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

void FileOpsWorker::simulateExtract()
{
    const QString path        = m_preset.dest + "/"_L1 + m_preset.filename + u".%extension%"_s;
    const ParsedScript script = m_scriptParser.parse(path);

    std::set<QString> archivePaths;

    for(const Track& track : m_tracks) {
        if(!mayRun()) {
            return;
        }

        if(!track.isInArchive() || m_tracksProcessed.contains(track.filepath())) {
            continue;
        }

        m_tracksProcessed.emplace(track.filepath());

        const QString destFilepath = QDir::cleanPath(evaluatePath(script, track));
        const QString destPath     = QFileInfo{destFilepath}.absolutePath();

        if(m_preset.wholeDir) {
            if(archivePaths.contains(track.archivePath())) {
                continue;
            }
            archivePaths.emplace(track.archivePath());

            auto archiveReader = m_audioLoader->archiveReaderForFile(track.archivePath());
            if(!archiveReader || !archiveReader->init(track.archivePath())) {
                qCWarning(FILEOPS) << "Failed to initialise archive reader for" << track.archivePath();
                continue;
            }

            std::unordered_map<QString, Track> archiveTracks;
            for(const auto& libraryTrack : m_trackPaths | std::views::values) {
                if(libraryTrack.isInArchive() && libraryTrack.archivePath() == track.archivePath()) {
                    archiveTracks.emplace(archiveEntryKey(libraryTrack.archivePath(),
                                                          cleanArchiveEntryPath(libraryTrack.pathInArchive())),
                                          libraryTrack);
                }
            }

            archiveReader->readEntries(
                [this, &track, &destPath, &script, &archiveTracks](const ArchiveEntryInfo& entry) {
                    if(!mayRun()) {
                        return false;
                    }
                    if(!entry.isRegularFile) {
                        return true;
                    }

                    const QString entryPath = cleanArchiveEntryPath(entry.path);
                    if(entryPath.isEmpty()) {
                        qCWarning(FILEOPS)
                            << "Skipping unsafe archive entry" << entry.path << "from" << track.archivePath();
                        return true;
                    }

                    const QString relativeEntryPath = entryPathRelativeToTrackLevel(entryPath, track);
                    const auto trackIt = archiveTracks.find(archiveEntryKey(track.archivePath(), entryPath));
                    const bool isTrack = trackIt != archiveTracks.cend();

                    const QString entryDestFilepath = isTrack ? QDir::cleanPath(evaluatePath(script, trackIt->second))
                                                              : QDir::cleanPath(destPath + "/"_L1 + relativeEntryPath);
                    const QString entryDestPath     = QFileInfo{entryDestFilepath}.absolutePath();
                    createDir(entryDestPath);

                    FileOpsItem item;
                    item.op           = Operation::Extract;
                    item.name         = QFileInfo{relativeEntryPath}.fileName();
                    item.source       = isTrack ? trackIt->second.filepath() : track.archivePath() + "/"_L1 + entryPath;
                    item.destination  = entryDestFilepath;
                    item.archivePath  = track.archivePath();
                    item.archiveEntry = entryPath;
                    m_operations.emplace_back(std::move(item));
                    return true;
                });

            if(m_preset.removeSourceArchive) {
                FileOpsItem item;
                item.op          = Operation::RemoveArchive;
                item.name        = QFileInfo{track.archivePath()}.fileName();
                item.source      = track.archivePath();
                item.archivePath = track.archivePath();
                m_operations.emplace_back(std::move(item));
            }
            continue;
        }

        createDir(destPath);

        FileOpsItem item;
        item.op           = Operation::Extract;
        item.name         = QFileInfo{track.pathInArchive()}.fileName();
        item.source       = track.prettyFilepath();
        item.destination  = destFilepath;
        item.archivePath  = track.archivePath();
        item.archiveEntry = track.pathInArchive();
        m_operations.push_back(std::move(item));
    }
}

void FileOpsWorker::simulateRename()
{
    const QString path        = m_preset.filename + u".%extension%"_s;
    const ParsedScript script = m_scriptParser.parse(path);

    for(const Track& track : m_tracks) {
        if(!mayRun()) {
            return;
        }

        if(m_tracksProcessed.contains(track.filepath())) {
            continue;
        }

        m_tracksProcessed.emplace(track.filepath());

        QString destFilename = QDir::cleanPath(evaluatePath(script, track));
        destFilename         = replaceSeparators(destFilename);

        if(track.filenameExt() == destFilename) {
            continue;
        }

        const QString destFilepath = QDir::cleanPath(track.path() + "/"_L1 + destFilename);

        m_operations.emplace_back(Operation::Rename, track.filenameExt(), track.filepath(), destFilepath);
    }
}

bool FileOpsWorker::renameFile(const FileOpsItem& item)
{
    QFile file{item.source};

    if(!file.exists()) {
        qCWarning(FILEOPS) << "File doesn't exist:" << item.source;
        return false;
    }

    if(!file.rename(item.destination)) {
        qCWarning(FILEOPS) << "Failed to move file from" << item.source << "to" << item.destination;
        return false;
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
                const QString cueDest = QDir::cleanPath(track.path() + "/"_L1 + relativeCuePath);
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

    return true;
}

QString FileOpsWorker::evaluatePath(const ParsedScript& script, const Track& track)
{
    static const FileOpsScriptEnvironment environment;
    const ScriptContext context{.environment = &environment};
    return m_scriptParser.evaluate(script, track, context);
}

bool FileOpsWorker::copyFile(const FileOpsItem& item)
{
    QFile file{item.source};

    if(!file.exists()) {
        qCWarning(FILEOPS) << "File doesn't exist:" << item.source;
        return false;
    }

    if(!file.copy(item.destination)) {
        qCWarning(FILEOPS) << "Failed to copy file from" << item.source << "to" << item.destination;
        return false;
    }

    return true;
}

bool FileOpsWorker::extractFile(const FileOpsItem& item)
{
    auto archiveReader = m_audioLoader->archiveReaderForFile(item.archivePath);
    if(!archiveReader || !archiveReader->init(item.archivePath)) {
        qCWarning(FILEOPS) << "Failed to initialise archive reader for" << item.archivePath;
        return false;
    }

    QFile file{item.destination};
    if(file.exists()) {
        qCWarning(FILEOPS) << "Destination file already exists:" << item.destination;
        return false;
    }

    if(!file.open(QIODevice::WriteOnly)) {
        qCWarning(FILEOPS) << "Failed to create file" << item.destination << file.errorString();
        return false;
    }

    if(!archiveReader->copyEntryToDevice(item.archiveEntry, &file, [this]() { return mayRun(); })) {
        qCWarning(FILEOPS) << "Failed to extract archive entry" << item.archiveEntry << "from" << item.archivePath
                           << "to" << item.destination;
        file.close();
        file.remove();
        return false;
    }

    return true;
}

bool FileOpsWorker::removeArchive(const FileOpsItem& item)
{
    if(m_failedArchives.contains(item.archivePath)) {
        qCWarning(FILEOPS) << "Skipping archive deletion after extraction failure:" << item.archivePath;
        return false;
    }

    if(!m_successfulArchives.contains(item.archivePath)) {
        qCWarning(FILEOPS) << "Skipping archive deletion without successful extraction:" << item.archivePath;
        return false;
    }

    if(!QFile::moveToTrash(item.archivePath)) {
        qCWarning(FILEOPS) << "Failed to delete source archive" << item.archivePath;
        return false;
    }

    updateExtractedArchiveTracks(item.archivePath);
    return true;
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
    m_failedArchives.clear();
    m_successfulArchives.clear();
    m_extractedTrackDestinations.clear();
    m_tracksToUpdate.clear();
    m_tracksToDelete.clear();
    m_trackPaths.clear();
}

void FileOpsWorker::updateExtractedArchiveTracks(const QString& archivePath)
{
    for(const auto& [source, destination] : m_extractedTrackDestinations) {
        if(!m_trackPaths.contains(source)) {
            continue;
        }

        auto tracks = m_trackPaths.equal_range(source);
        for(auto it = tracks.first; it != tracks.second; ++it) {
            auto& track = it->second;
            if(!track.isInArchive() || track.archivePath() != archivePath) {
                continue;
            }

            track.setFilePath(destination);
            if(const auto library = m_library->libraryForPath(destination)) {
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
