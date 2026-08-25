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

#include "fileopsworker.h"
#include "fileopssettings.h"
#include "testutils.h"

#include <utils/settings/settingsmanager.h>

#include <QFile>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

using namespace Qt::StringLiterals;

namespace Fooyin::FileOps {
namespace {
struct WorkerRun
{
    FileOperations operations;
    std::vector<FileOpResult> results;
};

struct FileOpsFixture
{
    QTemporaryDir tempDir;
    QString libraryRoot;
    QString letterDir;
    QString artistDir;
    QString albumDir;
    Track track;

    FileOpsFixture()
        : libraryRoot{tempDir.filePath(u"library"_s)}
        , letterDir{libraryRoot + u"/D"_s}
        , artistDir{letterDir + u"/Claude Debussy"_s}
        , albumDir{artistDir + u"/Album"_s}
    {
        EXPECT_TRUE(tempDir.isValid());
        EXPECT_TRUE(QDir{}.mkpath(albumDir));

        track = createTrack(albumDir + u"/track.flac"_s);
        track.setLibraryId(1);
    }

    Track createTrack(const QString& filepath, const QByteArray& contents = {})
    {
        EXPECT_TRUE(QDir{}.mkpath(QFileInfo{filepath}.absolutePath()));
        QFile file{filepath};
        EXPECT_TRUE(file.open(QIODevice::WriteOnly));
        EXPECT_EQ(file.write(contents), contents.size());
        file.close();

        Track newTrack;
        newTrack.setFilePath(filepath);
        return newTrack;
    }

    FileOperations simulate(bool removeEmptyParents, bool wholeDir = true, bool removeEmpty = true,
                            TrackList tracks = {})
    {
        FileOpPreset preset;
        preset.op          = Operation::Move;
        preset.dest        = tempDir.filePath(u"destination"_s);
        preset.filename    = u"%filename%"_s;
        preset.wholeDir    = wholeDir;
        preset.removeEmpty = removeEmpty;
        return process(preset, std::move(tracks), removeEmptyParents, false).operations;
    }

    WorkerRun process(const FileOpPreset& preset, TrackList tracks = {}, bool removeEmptyParents = false,
                      bool execute = true)
    {
        if(tracks.empty()) {
            tracks = {track};
        }

        SettingsManager settings{tempDir.filePath(u"settings.ini"_s)};
        settings.fileSet(Settings::RemoveEmptyParentFolders, removeEmptyParents);

        Testing::StubMusicLibrary library;
        library.setLibrary({.name = u"Test"_s, .path = libraryRoot, .id = 1, .status = LibraryInfo::Status::Idle});
        library.setTracks(tracks);
        FileOpsWorker worker{&library, {}, tracks, &settings};

        WorkerRun run;
        QObject::connect(&worker, &FileOpsWorker::simulated,
                         [&run](const FileOperations& operations) { run.operations = operations; });
        QObject::connect(&worker, &FileOpsWorker::operationCompleted,
                         [&run](const FileOpResult& result) { run.results.push_back(result); });

        worker.simulate(preset);
        if(execute) {
            worker.run();
        }
        return run;
    }
};

QStringList removalSources(const FileOperations& operations)
{
    QStringList sources;
    for(const FileOpsItem& operation : operations) {
        if(operation.op == Operation::Remove) {
            sources.push_back(operation.source);
        }
    }
    return sources;
}

FileOpPreset preset(Operation operation, const QString& destination, const QString& filename = u"%filename%"_s)
{
    return {.op = operation, .name = {}, .dest = destination, .filename = filename};
}
} // namespace

TEST(FileOpsWorkerTest, RemovesOnlySourceFolderByDefault)
{
    FileOpsFixture simulation;

    EXPECT_EQ(removalSources(simulation.simulate(false)), QStringList{simulation.albumDir});
}

TEST(FileOpsWorkerTest, RemovesParentFoldersWhenEnabled)
{
    FileOpsFixture simulation;

    const QStringList expected{simulation.albumDir, simulation.artistDir, simulation.letterDir};
    EXPECT_EQ(removalSources(simulation.simulate(true)), expected);
}

TEST(FileOpsWorkerTest, CopiesFile)
{
    FileOpsFixture simulation;
    const QByteArray contents{"file contents"};
    simulation.track             = simulation.createTrack(simulation.track.filepath(), contents);
    const QString destinationDir = simulation.tempDir.filePath(u"copy"_s);
    const QString destination    = destinationDir + u"/track.flac"_s;

    const WorkerRun run = simulation.process(preset(Operation::Copy, destinationDir));

    ASSERT_EQ(run.results.size(), 2);
    EXPECT_EQ(run.results[0].operation.op, Operation::Create);
    EXPECT_EQ(run.results[0].status, FileOpStatus::Succeeded);
    EXPECT_EQ(run.results[1].operation.op, Operation::Copy);
    EXPECT_EQ(run.results[1].status, FileOpStatus::Succeeded);
    EXPECT_TRUE(QFileInfo::exists(simulation.track.filepath()));

    QFile copiedFile{destination};
    ASSERT_TRUE(copiedFile.open(QIODevice::ReadOnly));
    EXPECT_EQ(copiedFile.readAll(), contents);
}

TEST(FileOpsWorkerTest, CopiesMultipleFilesAndCreatesDestination)
{
    FileOpsFixture simulation;
    Track secondTrack            = simulation.createTrack(simulation.albumDir + u"/second.flac"_s);
    const QString destinationDir = simulation.tempDir.filePath(u"copy"_s);

    const WorkerRun run = simulation.process(preset(Operation::Copy, destinationDir), {simulation.track, secondTrack});

    EXPECT_EQ(std::ranges::count(run.operations, Operation::Create, &FileOpsItem::op), 1);
    EXPECT_EQ(std::ranges::count(run.operations, Operation::Copy, &FileOpsItem::op), 2);
    EXPECT_TRUE(QFileInfo::exists(destinationDir + u"/track.flac"_s));
    EXPECT_TRUE(QFileInfo::exists(destinationDir + u"/second.flac"_s));
}

TEST(FileOpsWorkerTest, CopiesWholeDirectoryIncludingNonTrackFiles)
{
    FileOpsFixture simulation;
    const QString coverPath = simulation.albumDir + u"/cover.jpg"_s;
    EXPECT_TRUE(QFile{coverPath}.open(QIODevice::WriteOnly));

    const QString destinationDir = simulation.tempDir.filePath(u"copy"_s);
    FileOpPreset copyPreset      = preset(Operation::Copy, destinationDir);
    copyPreset.wholeDir          = true;

    const WorkerRun run = simulation.process(copyPreset);

    EXPECT_EQ(std::ranges::count(run.operations, Operation::Copy, &FileOpsItem::op), 2);
    EXPECT_TRUE(QFileInfo::exists(destinationDir + u"/track.flac"_s));
    EXPECT_TRUE(QFileInfo::exists(destinationDir + u"/cover.jpg"_s));
}

TEST(FileOpsWorkerTest, ReportsCopyFailureWhenDestinationExists)
{
    FileOpsFixture simulation;
    const QString destinationDir = simulation.tempDir.filePath(u"copy"_s);
    const QString destination    = destinationDir + u"/track.flac"_s;
    simulation.createTrack(destination);

    const WorkerRun run = simulation.process(preset(Operation::Copy, destinationDir));

    ASSERT_EQ(run.results.size(), 1);
    EXPECT_EQ(run.results.front().operation.op, Operation::Copy);
    EXPECT_EQ(run.results.front().status, FileOpStatus::Failed);
    EXPECT_FALSE(run.results.front().error.isEmpty());
}

TEST(FileOpsWorkerTest, ReportsCopyFailureWhenSourceDoesNotExist)
{
    FileOpsFixture simulation;
    ASSERT_TRUE(QFile::remove(simulation.track.filepath()));

    const WorkerRun run = simulation.process(preset(Operation::Copy, simulation.tempDir.filePath(u"copy"_s)));

    ASSERT_EQ(run.results.size(), 2);
    EXPECT_EQ(run.results.back().operation.op, Operation::Copy);
    EXPECT_EQ(run.results.back().status, FileOpStatus::Failed);
    EXPECT_FALSE(run.results.back().error.isEmpty());
}

TEST(FileOpsWorkerTest, EvaluatesDestinationFilenameFromTrackMetadata)
{
    FileOpsFixture simulation;
    simulation.track.setTitle(u"Clair de lune"_s);
    const QString destinationDir = simulation.tempDir.filePath(u"copy"_s);

    const WorkerRun run = simulation.process(preset(Operation::Copy, destinationDir, u"%title%"_s), {}, false, false);

    ASSERT_EQ(run.operations.size(), 2);
    EXPECT_EQ(run.operations.back().destination, destinationDir + u"/Clair de lune.flac"_s);
}

TEST(FileOpsWorkerTest, MovesFile)
{
    FileOpsFixture simulation;
    const QByteArray contents{"file contents"};
    simulation.track             = simulation.createTrack(simulation.track.filepath(), contents);
    const QString destinationDir = simulation.tempDir.filePath(u"move"_s);
    const QString destination    = destinationDir + u"/track.flac"_s;

    const WorkerRun run = simulation.process(preset(Operation::Move, destinationDir));

    ASSERT_EQ(run.results.size(), 2);
    EXPECT_EQ(run.results.back().operation.op, Operation::Move);
    EXPECT_EQ(run.results.back().status, FileOpStatus::Succeeded);
    EXPECT_FALSE(QFileInfo::exists(simulation.track.filepath()));

    QFile movedFile{destination};
    ASSERT_TRUE(movedFile.open(QIODevice::ReadOnly));
    EXPECT_EQ(movedFile.readAll(), contents);
}

TEST(FileOpsWorkerTest, MoveWholeDirectoryPreservesRelativePaths)
{
    FileOpsFixture simulation;
    const QString artworkDir = simulation.albumDir + u"/artwork"_s;
    simulation.createTrack(artworkDir + u"/cover.jpg"_s);
    const QString destinationDir = simulation.tempDir.filePath(u"move"_s);
    FileOpPreset movePreset      = preset(Operation::Move, destinationDir);
    movePreset.wholeDir          = true;

    const WorkerRun run = simulation.process(movePreset);

    EXPECT_EQ(std::ranges::count(run.operations, Operation::Move, &FileOpsItem::op), 2);
    EXPECT_TRUE(QFileInfo::exists(destinationDir + u"/track.flac"_s));
    EXPECT_TRUE(QFileInfo::exists(destinationDir + u"/artwork/cover.jpg"_s));
}

TEST(FileOpsWorkerTest, ReportsMoveFailureWhenDestinationExists)
{
    FileOpsFixture simulation;
    const QString destinationDir = simulation.tempDir.filePath(u"move"_s);
    const QString destination    = destinationDir + u"/track.flac"_s;
    simulation.createTrack(destination);

    const WorkerRun run = simulation.process(preset(Operation::Move, destinationDir));

    ASSERT_EQ(run.results.size(), 1);
    EXPECT_EQ(run.results.front().operation.op, Operation::Move);
    EXPECT_EQ(run.results.front().status, FileOpStatus::Failed);
    EXPECT_TRUE(QFileInfo::exists(simulation.track.filepath()));
    EXPECT_TRUE(QFileInfo::exists(destination));
}

TEST(FileOpsWorkerTest, RenamesFile)
{
    FileOpsFixture simulation;
    const QString renamedPath = simulation.albumDir + u"/renamed.flac"_s;

    const WorkerRun run = simulation.process(preset(Operation::Rename, {}, u"renamed"_s));

    ASSERT_EQ(run.results.size(), 1);
    EXPECT_EQ(run.results.front().operation.op, Operation::Rename);
    EXPECT_EQ(run.results.front().status, FileOpStatus::Succeeded);
    EXPECT_FALSE(QFileInfo::exists(simulation.track.filepath()));
    EXPECT_TRUE(QFileInfo::exists(renamedPath));
}

TEST(FileOpsWorkerTest, RenameReplacesPathSeparators)
{
    FileOpsFixture simulation;

    const WorkerRun run = simulation.process(preset(Operation::Rename, {}, u"artist/track"_s));

    ASSERT_EQ(run.operations.size(), 1);
    EXPECT_EQ(run.operations.front().destination, simulation.albumDir + u"/artist-track.flac"_s);
    EXPECT_TRUE(QFileInfo::exists(run.operations.front().destination));
}

TEST(FileOpsWorkerTest, SkipsOperationWhenSourceAndDestinationMatch)
{
    FileOpsFixture simulation;

    const WorkerRun copyRun = simulation.process(preset(Operation::Copy, simulation.albumDir), {}, false, false);
    const WorkerRun moveRun = simulation.process(preset(Operation::Move, simulation.albumDir), {}, false, false);
    const WorkerRun renameRun
        = simulation.process(preset(Operation::Rename, {}, simulation.track.filename()), {}, false, false);

    EXPECT_TRUE(copyRun.operations.empty());
    EXPECT_TRUE(moveRun.operations.empty());
    EXPECT_TRUE(renameRun.operations.empty());
}
} // namespace Fooyin::FileOps
