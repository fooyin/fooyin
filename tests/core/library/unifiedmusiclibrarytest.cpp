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

#include "core/library/unifiedmusiclibrary.h"
#include "core/database/dbschema.h"
#include "core/internalcoresettings.h"
#include "core/library/librarymanager.h"
#include "core/library/libraryscanner.h"
#include "core/network/networkaccessmanager.h"
#include "core/network/remoteioservice.h"
#include "core/playlist/playlisthandler.h"
#include "core/playlist/playlistloader.h"
#include <core/coresettings.h>

#include <core/engine/audioloader.h>
#include <utils/database/dbconnectionhandler.h>
#include <utils/database/dbconnectionpool.h>
#include <utils/database/dbconnectionprovider.h>
#include <utils/database/dbquery.h>
#include <utils/settings/settingsmanager.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QSqlError>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>

using namespace Qt::StringLiterals;

constexpr auto CurrentSchemaVersion = 17;

namespace {
QCoreApplication* ensureCoreApplication()
{
    QStandardPaths::setTestModeEnabled(true);

    if(auto* app = QCoreApplication::instance()) {
        return app;
    }

    static int argc{1};
    static char appName[]        = "fooyin-unifiedmusiclibrary-test";
    static char* argv[]          = {appName, nullptr};
    static QCoreApplication* app = []() {
        auto* instance = new QCoreApplication(argc, argv);
        QCoreApplication::setApplicationName(QString::fromLatin1(appName));
        return instance;
    }();
    return app;
}

void initialiseTestEnvironment()
{
    static const bool initialised = []() {
        ensureCoreApplication();
        QLoggingCategory::setFilterRules(u"fy.db.info=false\nfy.library.info=false\nfy.scanner.info=false"_s);
        return true;
    }();

    static_cast<void>(initialised);
}

template <typename Predicate>
bool waitForCondition(Predicate&& predicate, int timeoutMs = 10000)
{
    QElapsedTimer timer;
    timer.start();

    while(timer.elapsed() < timeoutMs) {
        if(predicate()) {
            return true;
        }

        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(5);
    }

    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return predicate();
}

Fooyin::DbConnectionPoolPtr createTestDbPool(const QString& dbPath)
{
    static std::atomic_int counter{0};

    Fooyin::DbConnection::DbParams params;
    params.type           = u"QSQLITE"_s;
    params.connectOptions = u"QSQLITE_OPEN_URI"_s;
    params.filePath       = dbPath;

    const auto connectionName = u"fooyin-unifiedmusiclibrary-test-%1"_s.arg(counter.fetch_add(1));
    auto dbPool               = Fooyin::DbConnectionPool::create(params, connectionName);

    const Fooyin::DbConnectionHandler handler{dbPool};
    EXPECT_TRUE(handler.hasConnection());

    Fooyin::DbSchema schema{Fooyin::DbConnectionProvider{dbPool}};
    const auto upgradeResult = schema.upgradeDatabase(CurrentSchemaVersion, u"://dbschema.xml"_s);

    EXPECT_TRUE(upgradeResult == Fooyin::DbSchema::UpgradeResult::Success
                || upgradeResult == Fooyin::DbSchema::UpgradeResult::IsCurrent
                || upgradeResult == Fooyin::DbSchema::UpgradeResult::BackwardsCompatible);
    return dbPool;
}

void requireWorkingDatabase(const Fooyin::DbConnectionPoolPtr& dbPool)
{
    const Fooyin::DbConnectionProvider dbProvider{dbPool};
    Fooyin::DbQuery probeQuery{dbProvider.db(), u"SELECT 1;"_s};
    if(!probeQuery.exec()) {
        GTEST_SKIP() << probeQuery.lastError().text().toStdString();
    }
}

int insertLibrary(Fooyin::DbConnectionPoolPtr dbPool, const QString& path, const QString& name)
{
    const Fooyin::DbConnectionProvider dbProvider{std::move(dbPool)};

    Fooyin::DbQuery insertQuery{dbProvider.db(), u"INSERT INTO Libraries (Name, Path) VALUES (:name, :path);"_s};
    insertQuery.bindValue(u":name"_s, name);
    insertQuery.bindValue(u":path"_s, path);
    EXPECT_TRUE(insertQuery.exec()) << insertQuery.lastError().text().toStdString();

    Fooyin::DbQuery selectQuery{dbProvider.db(), u"SELECT LibraryID FROM Libraries WHERE Path = :path;"_s};
    selectQuery.bindValue(u":path"_s, path);
    EXPECT_TRUE(selectQuery.exec()) << selectQuery.lastError().text().toStdString();
    EXPECT_TRUE(selectQuery.next()) << selectQuery.lastError().text().toStdString();

    return selectQuery.value(0).toInt();
}

void writeFile(const QString& path, const QByteArray& data = "test")
{
    QFile file{path};
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    ASSERT_EQ(file.write(data), data.size());
}

QStringList sortedTrackTitles(const Fooyin::TrackList& tracks)
{
    QStringList titles;
    titles.reserve(static_cast<qsizetype>(tracks.size()));

    for(const auto& track : tracks) {
        titles.push_back(track.title());
    }

    std::ranges::sort(titles);
    return titles;
}

QStringList trackFileNames(const Fooyin::TrackList& tracks)
{
    QStringList fileNames;
    fileNames.reserve(static_cast<qsizetype>(tracks.size()));

    for(const auto& track : tracks) {
        fileNames.push_back(QFileInfo{track.filepath()}.fileName());
    }

    return fileNames;
}

QVariantList waitForSignal(QSignalSpy& spy, int timeoutMs = 10000)
{
    if(spy.isEmpty()) {
        EXPECT_TRUE(spy.wait(timeoutMs));
    }

    EXPECT_FALSE(spy.isEmpty());
    return spy.isEmpty() ? QVariantList{} : spy.takeFirst();
}

void expectFinishedSignal(const QVariantList& args, int expectedId, Fooyin::ScanRequest::Type expectedType)
{
    ASSERT_EQ(args.size(), 3);
    EXPECT_EQ(args.at(0).toInt(), expectedId);
    EXPECT_EQ(args.at(1).value<Fooyin::ScanRequest::Type>(), expectedType);
    EXPECT_FALSE(args.at(2).toBool());
}

void expectSummarySignal(const QVariantList& args, int expectedId, Fooyin::ScanRequest::Type expectedType,
                         const Fooyin::ScanSummaryCounts& expectedSummary)
{
    ASSERT_EQ(args.size(), 3);
    EXPECT_EQ(args.at(0).toInt(), expectedId);
    EXPECT_EQ(args.at(1).value<Fooyin::ScanRequest::Type>(), expectedType);

    const auto summary = args.at(2).value<Fooyin::ScanSummaryCounts>();
    EXPECT_EQ(summary.added, expectedSummary.added);
    EXPECT_EQ(summary.updated, expectedSummary.updated);
    EXPECT_EQ(summary.removed, expectedSummary.removed);
}

class FakeLibraryReader : public Fooyin::AudioReader
{
public:
    struct State
    {
        void setTitle(const QString& path, const QString& title)
        {
            const std::lock_guard lock(m_mutex);
            m_titles.insert(QFileInfo{path}.absoluteFilePath(), title);
        }

        [[nodiscard]] QString titleForPath(const QString& path) const
        {
            const std::lock_guard lock(m_mutex);
            return m_titles.value(QFileInfo{path}.absoluteFilePath(), QFileInfo{path}.completeBaseName());
        }

    private:
        mutable std::mutex m_mutex;
        QHash<QString, QString> m_titles;
    };

    explicit FakeLibraryReader(std::shared_ptr<State> state)
        : m_state{std::move(state)}
    { }

    QStringList extensions() const override
    {
        return {u"mp3"_s};
    }

    bool canReadCover() const override
    {
        return false;
    }

    bool canWriteMetaData() const override
    {
        return false;
    }

    bool readTrack(const Fooyin::AudioSource& source, Fooyin::Track& track) override
    {
        const QFileInfo info{source.filepath};
        track.setTitle(m_state->titleForPath(info.absoluteFilePath()));
        track.setAlbum(u"Library Test Album"_s);
        track.setArtists({u"Library Test Artist"_s});
        track.setAlbumArtists({u"Library Test Artist"_s});

        const auto match = QRegularExpression{u"(\\d+)"_s}.match(info.completeBaseName());
        track.setTrackNumber(match.hasMatch() ? match.captured(1) : u"1"_s);
        track.setDiscNumber(u"1"_s);
        return true;
    }

private:
    std::shared_ptr<State> m_state;
};

struct LibraryTestContext
{
    LibraryTestContext()
        : settings{tempDir.filePath(u"settings.ini"_s)}
        , coreSettings{&settings}
        , dbPool{createTestDbPool(tempDir.filePath(u"library.db"_s))}
        , dbHandler{dbPool}
        , libraryManager{dbPool, &settings}
        , playlistLoader{std::make_shared<Fooyin::PlaylistLoader>()}
        , audioLoader{std::make_shared<Fooyin::AudioLoader>()}
        , readerState{std::make_shared<FakeLibraryReader::State>()}
        , network{std::make_shared<Fooyin::NetworkAccessManager>(&settings)}
        , remoteIo{std::make_shared<Fooyin::RemoteIoService>(network, &settings)}
        , library{&libraryManager, dbPool, playlistLoader, audioLoader, remoteIo, &settings}
        , playlistHandler{dbPool, audioLoader, &library, &settings}
    {
        audioLoader->addReader(u"fake-library-reader"_s,
                               [state = readerState]() { return std::make_unique<FakeLibraryReader>(state); });
    }

    QTemporaryDir tempDir;
    Fooyin::SettingsManager settings;
    Fooyin::CoreSettings coreSettings;
    Fooyin::DbConnectionPoolPtr dbPool;
    Fooyin::DbConnectionHandler dbHandler;
    Fooyin::LibraryManager libraryManager;
    std::shared_ptr<Fooyin::PlaylistLoader> playlistLoader;
    std::shared_ptr<Fooyin::AudioLoader> audioLoader;
    std::shared_ptr<FakeLibraryReader::State> readerState;
    std::shared_ptr<Fooyin::NetworkAccessManager> network;
    std::shared_ptr<Fooyin::RemoteIoService> remoteIo;
    Fooyin::UnifiedMusicLibrary library;
    Fooyin::PlaylistHandler playlistHandler;
};
} // namespace

namespace Fooyin::Testing {
class UnifiedMusicLibraryTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        initialiseTestEnvironment();

        m_context = std::make_unique<LibraryTestContext>();
        ASSERT_TRUE(m_context->tempDir.isValid());
        requireWorkingDatabase(m_context->dbPool);
    }

    [[nodiscard]] LibraryTestContext& context() const
    {
        return *m_context;
    }

    QString createTrackFile(const QString& fileName, const QString& title)
    {
        const QString path = context().tempDir.filePath(fileName);
        writeFile(path);
        context().readerState->setTitle(path, title);
        return path;
    }

    LibraryInfo addLibrary(const QString& name)
    {
        const int libraryId = insertLibrary(context().dbPool, context().tempDir.path(), name);
        EXPECT_GT(libraryId, 0);

        context().libraryManager.reset();

        const auto libraryInfo = context().libraryManager.libraryInfo(libraryId);
        EXPECT_TRUE(libraryInfo.has_value());
        return libraryInfo.value_or(LibraryInfo{});
    }

    void waitForSuccessfulScan(const std::function<ScanRequest()>& startScan)
    {
        QSignalSpy finishedSpy{&context().library, &MusicLibrary::scanFinished};

        const ScanRequest request = startScan();
        expectFinishedSignal(waitForSignal(finishedSpy), request.id, ScanRequest::Library);
    }

private:
    std::unique_ptr<LibraryTestContext> m_context;
};

TEST_F(UnifiedMusicLibraryTest, ScanForChangesMakesTracksVisibleBeforeScanFinished)
{
    createTrackFile(u"initial_track.mp3"_s, u"Initial"_s);

    const LibraryInfo libraryInfo = addLibrary(u"Refresh Visibility"_s);
    ASSERT_GE(libraryInfo.id, 0);

    waitForSuccessfulScan([&]() { return context().library.rescan(libraryInfo); });
    ASSERT_EQ(context().library.tracks().size(), 1U);

    createTrackFile(u"new_track.mp3"_s, u"Added Later"_s);

    QSignalSpy summarySpy{&context().library, &MusicLibrary::scanSummary};
    QSignalSpy finishedSpy{&context().library, &MusicLibrary::scanFinished};

    const ScanRequest request       = context().library.refresh(libraryInfo);
    const QVariantList finishedArgs = waitForSignal(finishedSpy);
    const int tracksAtFinish        = static_cast<int>(context().library.tracks().size());
    const QVariantList summaryArgs  = waitForSignal(summarySpy);

    expectFinishedSignal(finishedArgs, request.id, ScanRequest::Library);
    expectSummarySignal(summaryArgs, request.id, ScanRequest::Library, {.added = 1, .updated = 0, .removed = 0});

    EXPECT_EQ(tracksAtFinish, 2);
    EXPECT_EQ(context().library.tracks().size(), 2U);
    EXPECT_EQ(sortedTrackTitles(context().library.tracks()), (QStringList{u"Added Later"_s, u"Initial"_s}));
}

TEST_F(UnifiedMusicLibraryTest, MultipleScanUpdatesCommitInOrder)
{
    constexpr int TrackCount = 300;

    for(int i = 0; i < TrackCount; ++i) {
        const QString fileName = u"track_%1.mp3"_s.arg(i, 3, 10, QChar{u'0'});
        createTrackFile(fileName, u"Track %1"_s.arg(i));
    }

    const LibraryInfo libraryInfo = addLibrary(u"Batch Commit Order"_s);
    ASSERT_GE(libraryInfo.id, 0);

    QSignalSpy applySpy{&context().library, &MusicLibrary::scanApplyCompleted};
    QSignalSpy summarySpy{&context().library, &MusicLibrary::scanSummary};
    QSignalSpy finishedSpy{&context().library, &MusicLibrary::scanFinished};

    const ScanRequest request = context().library.rescan(libraryInfo);
    ASSERT_TRUE(waitForCondition([&]() { return applySpy.count() >= 2; }));

    const QVariantList finishedArgs = waitForSignal(finishedSpy);
    const int tracksAtFinish        = static_cast<int>(context().library.tracks().size());
    const QVariantList summaryArgs  = waitForSignal(summarySpy);

    for(int i{0}; i < applySpy.count(); ++i) {
        const QVariantList& applyArgs = applySpy.at(i);
        ASSERT_EQ(applyArgs.size(), 1);
        EXPECT_EQ(applyArgs.at(0).toInt(), request.id);
    }

    expectFinishedSignal(finishedArgs, request.id, ScanRequest::Library);
    expectSummarySignal(summaryArgs, request.id, ScanRequest::Library,
                        {.added = TrackCount, .updated = 0, .removed = 0});

    EXPECT_EQ(summarySpy.count(), 0);
    EXPECT_EQ(finishedSpy.count(), 0);
    EXPECT_EQ(tracksAtFinish, TrackCount);
    EXPECT_EQ(static_cast<int>(context().library.tracks().size()), TrackCount);
    EXPECT_EQ(applySpy.count(), 2);
}

TEST_F(UnifiedMusicLibraryTest, TrackRescanUpdatesMetadataBeforeScanFinished)
{
    const QString filePath = createTrackFile(u"single_track.mp3"_s, u"Before"_s);

    const LibraryInfo libraryInfo = addLibrary(u"Rescan Metadata"_s);
    ASSERT_GE(libraryInfo.id, 0);

    waitForSuccessfulScan([&]() { return context().library.rescan(libraryInfo); });
    ASSERT_EQ(context().library.tracks().size(), 1U);

    const Track existingTrack = context().library.tracks().front();
    ASSERT_TRUE(existingTrack.isValid());
    ASSERT_EQ(existingTrack.title(), u"Before"_s);

    context().readerState->setTitle(filePath, u"After"_s);

    QSignalSpy summarySpy{&context().library, &MusicLibrary::scanSummary};
    QSignalSpy finishedSpy{&context().library, &MusicLibrary::scanFinished};

    const ScanRequest request       = context().library.scanTracks({existingTrack});
    const QVariantList finishedArgs = waitForSignal(finishedSpy);
    const QString titleAtFinish     = context().library.trackForId(existingTrack.id()).title();
    const QVariantList summaryArgs  = waitForSignal(summarySpy);

    expectFinishedSignal(finishedArgs, request.id, ScanRequest::Tracks);
    expectSummarySignal(summaryArgs, request.id, ScanRequest::Tracks, {.added = 0, .updated = 1, .removed = 0});

    EXPECT_EQ(titleAtFinish, u"After"_s);
    EXPECT_EQ(context().library.trackForId(existingTrack.id()).title(), u"After"_s);
}

TEST_F(UnifiedMusicLibraryTest, OverlappingSortAndScanDoNotLoseNewTracks)
{
    ASSERT_TRUE(context().settings.set<Settings::Core::LibrarySortScript>(u"%title%"_s));

    createTrackFile(u"b.mp3"_s, u"Alpha"_s);
    createTrackFile(u"a.mp3"_s, u"Zulu"_s);

    const LibraryInfo libraryInfo = addLibrary(u"Sort Overlap"_s);
    ASSERT_GE(libraryInfo.id, 0);

    waitForSuccessfulScan([&]() { return context().library.rescan(libraryInfo); });
    ASSERT_EQ(trackFileNames(context().library.tracks()), (QStringList{u"b.mp3"_s, u"a.mp3"_s}));

    createTrackFile(u"c.mp3"_s, u"Beta"_s);

    QSignalSpy summarySpy{&context().library, &MusicLibrary::scanSummary};
    QSignalSpy finishedSpy{&context().library, &MusicLibrary::scanFinished};

    const ScanRequest request = context().library.refresh(libraryInfo);

    ASSERT_TRUE(context().settings.set<Settings::Core::LibrarySortScript>(u"%filepath%"_s));
    const QVariantList finishedArgs     = waitForSignal(finishedSpy);
    const QStringList fileNamesAtFinish = trackFileNames(context().library.tracks());
    const QVariantList summaryArgs      = waitForSignal(summarySpy);

    expectFinishedSignal(finishedArgs, request.id, ScanRequest::Library);
    expectSummarySignal(summaryArgs, request.id, ScanRequest::Library, {.added = 1, .updated = 0, .removed = 0});

    EXPECT_EQ(fileNamesAtFinish, (QStringList{u"a.mp3"_s, u"b.mp3"_s, u"c.mp3"_s}));
    EXPECT_EQ(sortedTrackTitles(context().library.tracks()), (QStringList{u"Alpha"_s, u"Beta"_s, u"Zulu"_s}));
}

TEST_F(UnifiedMusicLibraryTest, ReaddingLibraryUpdatesExistingPlaylistTracksAndEmitsMetadataChange)
{
    createTrackFile(u"restored_track.mp3"_s, u"Restored"_s);

    const LibraryInfo originalLibrary = addLibrary(u"Original Library"_s);
    ASSERT_GE(originalLibrary.id, 0);

    const ScanRequest initialRequest = context().library.rescan(originalLibrary);
    ASSERT_GE(initialRequest.id, 0);
    ASSERT_TRUE(waitForCondition([&]() { return context().library.tracks().size() == 1U; }));
    ASSERT_EQ(context().library.tracks().size(), 1U);

    const Track originalTrack = context().library.tracks().front();
    ASSERT_TRUE(originalTrack.isValid());
    ASSERT_TRUE(originalTrack.isInLibrary());

    auto* playlist = context().playlistHandler.createPlaylist(u"Regression Playlist"_s, context().library.tracks());
    ASSERT_NE(playlist, nullptr);
    ASSERT_EQ(playlist->trackCount(), 1);
    ASSERT_TRUE(playlist->tracks().front().isInLibrary());
    context().playlistHandler.savePlaylists();

    ASSERT_TRUE(context().libraryManager.removeLibrary(originalLibrary.id));
    ASSERT_TRUE(waitForCondition([&]() {
        const Track track = context().library.trackForId(originalTrack.id());
        return track.isValid() && !track.isInLibrary();
    }));

    context().playlistHandler.replacePlaylistTracks(playlist->id(), context().library.tracks());
    ASSERT_EQ(playlist->trackCount(), 1);
    ASSERT_FALSE(playlist->tracks().front().isInLibrary());

    const QSignalSpy metadataSpy{&context().library, &MusicLibrary::tracksMetadataChanged};

    const LibraryInfo readdedLibrary = addLibrary(u"Readded Library"_s);
    ASSERT_GE(readdedLibrary.id, 0);

    const ScanRequest request = context().library.rescan(readdedLibrary);
    ASSERT_GE(request.id, 0);

    const bool restored = waitForCondition([&]() {
        const Track track = context().library.trackForId(originalTrack.id());
        if(!track.isValid() || !track.isInLibrary()) {
            return false;
        }

        for(int i{0}; i < metadataSpy.count(); ++i) {
            const QVariantList& args = metadataSpy.at(i);
            if(args.size() != 1) {
                continue;
            }

            const auto tracks = args.front().value<TrackList>();
            if(std::ranges::any_of(tracks, [&](const Track& updatedTrack) {
                   return updatedTrack.id() == originalTrack.id() && updatedTrack.isInLibrary();
               })) {
                return true;
            }
        }

        return false;
    });

    EXPECT_TRUE(restored);

    bool sawRestoredTrack{false};
    for(int i{0}; i < metadataSpy.count(); ++i) {
        const QVariantList& args = metadataSpy.at(i);
        ASSERT_EQ(args.size(), 1);
        const auto tracks = args.front().value<TrackList>();
        if(std::ranges::any_of(
               tracks, [&](const Track& track) { return track.id() == originalTrack.id() && track.isInLibrary(); })) {
            sawRestoredTrack = true;
        }
    }

    EXPECT_TRUE(sawRestoredTrack);
    EXPECT_TRUE(context().library.trackForId(originalTrack.id()).isInLibrary());
    ASSERT_EQ(playlist->trackCount(), 1);
    EXPECT_TRUE(playlist->tracks().front().isInLibrary());
}
} // namespace Fooyin::Testing
