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

#include <core/application.h>
#include <core/corepaths.h>
#include <core/coresettings.h>
#include <core/database/database.h>
#include <core/internalcoresettings.h>
#include <core/playlist/parsers/m3uparser.h>
#include <core/playlist/parsers/plsparser.h>
#include <core/playlist/playlist.h>
#include <core/playlist/playlisthandler.h>
#include <core/playlist/playlistloader.h>
#include <core/playlist/playlistparser.h>
#include <utils/fypaths.h>
#include <utils/settings/settingsmanager.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <gtest/gtest.h>

using namespace Qt::StringLiterals;

constexpr auto PendingTrashKey = "Playlist/AutoExportPendingTrash"_L1;

void initDataResources()
{
    Q_INIT_RESOURCE(data);
    Q_INIT_RESOURCE(playlists);
}

namespace Fooyin::Testing {
QCoreApplication* ensureCoreApplication()
{
    initDataResources();

    static const QTemporaryDir testRoot{QDir::tempPath() + u"/fooyin-autoexport-test-XXXXXX"_s};
    if(!testRoot.isValid()) {
        return nullptr;
    }

    qputenv("XDG_CONFIG_HOME", testRoot.filePath(u"config"_s).toUtf8());
    qputenv("XDG_DATA_HOME", testRoot.filePath(u"data"_s).toUtf8());
    qputenv("XDG_STATE_HOME", testRoot.filePath(u"state"_s).toUtf8());

#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
    QStandardPaths::setTestModeEnabled(false);
#else
    QStandardPaths::setTestModeEnabled(true);
#endif

    if(auto* app = QCoreApplication::instance()) {
        return app;
    }

    static int argc{1};
    static char appName[]        = "fooyin-autoexport-test";
    static char* argv[]          = {appName, nullptr};
    static QCoreApplication* app = []() {
        auto* instance = new QCoreApplication(argc, argv);
        QCoreApplication::setApplicationName(QString::fromLatin1(appName) + u'-' + QDir{testRoot.path()}.dirName());
        return instance;
    }();
    return app;
}

Track makeTrack(const QString& path, int id)
{
    Track track{path, 0};
    track.setId(id);
    track.generateHash();
    return track;
}

class PlaylistAutoExportTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_NE(ensureCoreApplication(), nullptr);

        QFile::remove(Core::settingsPath());
        QFile::remove(Core::statePath());
        QFile::remove(Utils::sharePath() + u"/fooyin.db"_s);

        ASSERT_TRUE(m_exportDir.isValid());
        m_application = std::make_unique<Application>();
        ASSERT_EQ(m_application->database()->status(), Database::Status::Ok);
        m_application->playlistLoader()->addParser(std::make_unique<M3uParser>());

        settings()->fileSet(Settings::Core::Internal::AutoExportPlaylistsPath, m_exportDir.path());
        settings()->fileSet(Settings::Core::Internal::AutoExportPlaylistsType, u"m3u"_s);
        settings()->fileSet(Settings::Core::Internal::AutoExportPlaylists, true);
        settings()->fileSet(Settings::Core::Internal::AutoExportPlaylistsRemove, false);
    }

    void TearDown() override
    {
        QCoreApplication::processEvents();
    }

    SettingsManager* settings() const
    {
        return m_application->settingsManager();
    }

    PlaylistHandler* playlists() const
    {
        return m_application->playlistHandler();
    }

    QString exportPath(const QString& name) const
    {
        return m_exportDir.filePath(name + u".m3u"_s);
    }

    void flushEvents() const
    {
        for(int i{0}; i < 3; ++i) {
            QCoreApplication::processEvents();
        }
    }

    QByteArray exportContents(const QString& name) const
    {
        QFile file{exportPath(name)};
        if(!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return file.readAll();
    }

    bool writeExport(const QString& name, const QByteArray& contents) const
    {
        QFile file{exportPath(name)};
        return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(contents) == contents.size();
    }

    Playlist* createPlaylist(const QString& name, int trackId)
    {
        return playlists()->createPlaylist(name,
                                           {makeTrack(m_exportDir.filePath(u"song%1.flac"_s.arg(trackId)), trackId)});
    }

    void seedPendingTrash(const QStringList& paths) const
    {
        FyStateSettings stateSettings;
        stateSettings.setValue(PendingTrashKey, paths);
        stateSettings.sync();
    }

    QStringList pendingTrash() const
    {
        const FyStateSettings stateSettings;
        return stateSettings.value(PendingTrashKey).toStringList();
    }

    void reopenApplication()
    {
        settings()->storeSettings();

        m_application.reset();
        m_application = std::make_unique<Application>();

        ASSERT_EQ(m_application->database()->status(), Database::Status::Ok);
        m_application->playlistLoader()->addParser(std::make_unique<M3uParser>());
    }

    void populatePlaylists() const
    {
        ASSERT_TRUE(QMetaObject::invokeMethod(playlists(), "playlistsPopulated", Qt::DirectConnection));
        flushEvents();
    }

    QTemporaryDir m_exportDir;
    std::unique_ptr<Application> m_application;
};

TEST_F(PlaylistAutoExportTest, ExportsNewPlaylist)
{
    auto* playlist = createPlaylist(u"Exported"_s, 1);
    ASSERT_NE(playlist, nullptr);
    ASSERT_EQ(playlist->trackCount(), 1);
    flushEvents();

    EXPECT_TRUE(exportContents(u"Exported"_s).contains("song1.flac"));
}

TEST_F(PlaylistAutoExportTest, FollowsPathAndMetadataSettings)
{
    settings()->fileSet(Settings::Core::Internal::PlaylistSavePathType,
                        static_cast<int>(PlaylistParser::PathType::Absolute));
    settings()->fileSet(Settings::Core::Internal::PlaylistSaveMetadata, true);
    ASSERT_NE(createPlaylist(u"Configured"_s, 1), nullptr);
    flushEvents();

    const QByteArray contents = exportContents(u"Configured"_s);
    EXPECT_TRUE(contents.startsWith("#EXTM3U\n"));
    EXPECT_TRUE(contents.contains(m_exportDir.filePath(u"song1.flac"_s).toUtf8()));
}

TEST_F(PlaylistAutoExportTest, ExportsModifiedAndMissingPlaylistsWithoutRewritingUnchanged)
{
    auto* playlist = createPlaylist(u"Tracks"_s, 1);
    ASSERT_NE(playlist, nullptr);
    flushEvents();
    ASSERT_TRUE(exportContents(u"Tracks"_s).contains("song1.flac"));

    ASSERT_TRUE(writeExport(u"Default"_s, "unchanged"));
    ASSERT_NE(createPlaylist(u"Trigger"_s, 2), nullptr);
    flushEvents();
    EXPECT_EQ(exportContents(u"Default"_s), "unchanged");

    playlists()->replacePlaylistTracks(playlist->id(), {makeTrack(m_exportDir.filePath(u"song3.flac"_s), 3)});
    ASSERT_NE(createPlaylist(u"Modified trigger"_s, 4), nullptr);
    flushEvents();
    EXPECT_TRUE(exportContents(u"Tracks"_s).contains("song3.flac"));

    ASSERT_TRUE(QFile::remove(exportPath(u"Tracks"_s)));
    ASSERT_NE(createPlaylist(u"Missing trigger"_s, 5), nullptr);
    flushEvents();
    EXPECT_TRUE(exportContents(u"Tracks"_s).contains("song3.flac"));
}

TEST_F(PlaylistAutoExportTest, EmptyPlaylistIsExportedWhenRemovalIsDisabled)
{
    auto* playlist = createPlaylist(u"Empty"_s, 1);
    ASSERT_NE(playlist, nullptr);
    flushEvents();
    playlists()->clearPlaylistTracks(playlist->id());
    ASSERT_NE(createPlaylist(u"Trigger"_s, 2), nullptr);
    flushEvents();
    EXPECT_TRUE(QFile::exists(exportPath(u"Empty"_s)));
    EXPECT_TRUE(exportContents(u"Empty"_s).isEmpty());
}

TEST_F(PlaylistAutoExportTest, NewUnmodifiedEmptyPlaylistHasNoExport)
{
    ASSERT_NE(playlists()->createPlaylist(u"Empty"_s), nullptr);
    flushEvents();
    EXPECT_FALSE(QFile::exists(exportPath(u"Empty"_s)));
}

TEST_F(PlaylistAutoExportTest, EmptyPlaylistIsTrashedWhenRemovalIsEnabled)
{
    auto* playlist = createPlaylist(u"Empty"_s, 1);
    ASSERT_NE(playlist, nullptr);
    flushEvents();
    ASSERT_TRUE(QFile::exists(exportPath(u"Empty"_s)));

    settings()->fileSet(Settings::Core::Internal::AutoExportPlaylistsRemove, true);
    playlists()->clearPlaylistTracks(playlist->id());
    ASSERT_NE(createPlaylist(u"Trigger"_s, 1), nullptr);
    flushEvents();
    EXPECT_FALSE(QFile::exists(exportPath(u"Empty"_s)));
}

TEST_F(PlaylistAutoExportTest, RemovedPlaylistIsTrashedWhenRemovalIsEnabled)
{
    auto* playlist = createPlaylist(u"Removed"_s, 1);
    ASSERT_NE(playlist, nullptr);
    flushEvents();
    ASSERT_TRUE(QFile::exists(exportPath(u"Removed"_s)));

    settings()->fileSet(Settings::Core::Internal::AutoExportPlaylistsRemove, true);
    playlists()->removePlaylist(playlist->id());
    ASSERT_NE(createPlaylist(u"Trigger"_s, 2), nullptr);
    flushEvents();
    EXPECT_FALSE(QFile::exists(exportPath(u"Removed"_s)));
}

TEST_F(PlaylistAutoExportTest, RemovedPlaylistIsSavedWhenConfigured)
{
    auto* playlist = createPlaylist(u"Removed"_s, 1);
    ASSERT_NE(playlist, nullptr);
    flushEvents();
    ASSERT_TRUE(writeExport(u"Removed"_s, "stale"));

    settings()->fileSet(Settings::Core::Internal::AutoExportPlaylistsSaveRemoved, true);
    playlists()->removePlaylist(playlist->id());
    ASSERT_NE(createPlaylist(u"Trigger"_s, 2), nullptr);
    flushEvents();
    EXPECT_TRUE(exportContents(u"Removed"_s).contains("song1.flac"));
}

TEST_F(PlaylistAutoExportTest, RemovedPlaylistIsLeftAloneWhenSavingAndRemovalAreDisabled)
{
    auto* playlist = createPlaylist(u"Removed"_s, 1);
    ASSERT_NE(playlist, nullptr);
    flushEvents();
    ASSERT_TRUE(writeExport(u"Removed"_s, "existing"));

    playlists()->removePlaylist(playlist->id());
    ASSERT_NE(createPlaylist(u"Trigger"_s, 2), nullptr);
    flushEvents();
    EXPECT_EQ(exportContents(u"Removed"_s), "existing");
}

TEST_F(PlaylistAutoExportTest, RestoringTrashedPlaylistReexportsIt)
{
    settings()->fileSet(Settings::Core::Internal::AutoExportPlaylistsRemove, true);
    auto* playlist = createPlaylist(u"Restored"_s, 1);
    ASSERT_NE(playlist, nullptr);
    flushEvents();

    const UId id = playlist->id();
    playlists()->removePlaylist(id);
    ASSERT_NE(createPlaylist(u"Trigger"_s, 2), nullptr);
    flushEvents();
    ASSERT_FALSE(QFile::exists(exportPath(u"Restored"_s)));

    ASSERT_NE(playlists()->restorePlaylist(id), nullptr);
    flushEvents();
    EXPECT_TRUE(exportContents(u"Restored"_s).contains("song1.flac"));
}

TEST_F(PlaylistAutoExportTest, RestoringBeforeRemovalKeepsTheExport)
{
    settings()->fileSet(Settings::Core::Internal::AutoExportPlaylistsRemove, true);
    auto* playlist = createPlaylist(u"Restored"_s, 1);
    ASSERT_NE(playlist, nullptr);
    flushEvents();

    const UId id = playlist->id();
    playlists()->removePlaylist(id);
    ASSERT_NE(playlists()->restorePlaylist(id), nullptr);
    flushEvents();

    EXPECT_TRUE(exportContents(u"Restored"_s).contains("song1.flac"));
    EXPECT_TRUE(pendingTrash().isEmpty());
}

TEST_F(PlaylistAutoExportTest, PendingRemovalIsRetriedAtStartup)
{
    settings()->fileSet(Settings::Core::Internal::AutoExportPlaylistsRemove, true);
    ASSERT_NE(createPlaylist(u"Pending"_s, 1), nullptr);
    flushEvents();
    ASSERT_TRUE(QFile::exists(exportPath(u"Pending"_s)));

    seedPendingTrash({exportPath(u"Pending"_s)});
    reopenApplication();
    populatePlaylists();

    EXPECT_FALSE(QFile::exists(exportPath(u"Pending"_s)));
    EXPECT_TRUE(pendingTrash().isEmpty());
}

TEST_F(PlaylistAutoExportTest, ShutdownSavesRemovalsForNextStartup)
{
    settings()->fileSet(Settings::Core::Internal::AutoExportPlaylistsRemove, true);
    auto* playlist = createPlaylist(u"Pending"_s, 1);
    ASSERT_NE(playlist, nullptr);
    flushEvents();
    ASSERT_TRUE(QFile::exists(exportPath(u"Pending"_s)));

    playlists()->removePlaylist(playlist->id());
    m_application->shutdown();
    EXPECT_TRUE(QFile::exists(exportPath(u"Pending"_s)));
    EXPECT_EQ(pendingTrash(), QStringList{exportPath(u"Pending"_s)});

    reopenApplication();
    populatePlaylists();
    EXPECT_FALSE(QFile::exists(exportPath(u"Pending"_s)));
    EXPECT_TRUE(pendingTrash().isEmpty());
}

TEST_F(PlaylistAutoExportTest, MissingPendingFileIsClearedAtStartup)
{
    settings()->fileSet(Settings::Core::Internal::AutoExportPlaylistsRemove, true);
    seedPendingTrash({exportPath(u"Missing"_s)});
    reopenApplication();
    populatePlaylists();
    EXPECT_TRUE(pendingTrash().isEmpty());
}

TEST_F(PlaylistAutoExportTest, PendingRemovalIsCancelledForAnActivePlaylist)
{
    settings()->fileSet(Settings::Core::Internal::AutoExportPlaylistsRemove, true);
    ASSERT_TRUE(writeExport(u"Active"_s, "existing"));
    seedPendingTrash({exportPath(u"Active"_s)});
    reopenApplication();

    ASSERT_NE(createPlaylist(u"Active"_s, 1), nullptr);
    populatePlaylists();

    EXPECT_TRUE(QFile::exists(exportPath(u"Active"_s)));
    EXPECT_TRUE(pendingTrash().isEmpty());
}

TEST_F(PlaylistAutoExportTest, DisablingRemovalClearsPendingTrash)
{
    ASSERT_TRUE(writeExport(u"Pending"_s, "existing"));
    seedPendingTrash({exportPath(u"Pending"_s)});
    reopenApplication();
    populatePlaylists();

    EXPECT_EQ(exportContents(u"Pending"_s), "existing");
    EXPECT_TRUE(pendingTrash().isEmpty());
}

TEST_F(PlaylistAutoExportTest, DisablingAutoExportClearsPendingTrash)
{
    ASSERT_TRUE(writeExport(u"Pending"_s, "existing"));
    settings()->fileSet(Settings::Core::Internal::AutoExportPlaylistsRemove, true);
    settings()->fileSet(Settings::Core::Internal::AutoExportPlaylists, false);
    seedPendingTrash({exportPath(u"Pending"_s)});
    reopenApplication();
    populatePlaylists();

    EXPECT_EQ(exportContents(u"Pending"_s), "existing");
    EXPECT_TRUE(pendingTrash().isEmpty());
}

TEST_F(PlaylistAutoExportTest, FailedTrashPreservesPendingRemoval)
{
    const QString resourcePath = u":/playlists/standardtest.m3u"_s;
    ASSERT_TRUE(QFile::exists(resourcePath));

    settings()->fileSet(Settings::Core::Internal::AutoExportPlaylistsRemove, true);
    settings()->fileSet(Settings::Core::Internal::AutoExportPlaylistsPath, u":/playlists"_s);
    seedPendingTrash({resourcePath});
    reopenApplication();
    populatePlaylists();

    EXPECT_TRUE(QFile::exists(resourcePath));
    EXPECT_EQ(pendingTrash(), QStringList{resourcePath});
}
} // namespace Fooyin::Testing
