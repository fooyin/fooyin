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

#include <core/playlist/playlisthandler.h>

#include <core/coresettings.h>
#include <core/engine/audioloader.h>
#include <core/internalcoresettings.h>
#include <core/library/musiclibrary.h>
#include <core/track.h>
#include <utils/database/dbconnectionhandler.h>
#include <utils/database/dbconnectionpool.h>
#include <utils/database/dbconnectionprovider.h>
#include <utils/database/dbquery.h>
#include <utils/settings/settingsmanager.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <optional>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
namespace {
QCoreApplication* ensureCoreApplication()
{
    QStandardPaths::setTestModeEnabled(true);

    if(auto* app = QCoreApplication::instance()) {
        return app;
    }

    static int argc{1};
    static char appName[]        = "fooyin-playlisthandler-test";
    static char* argv[]          = {appName, nullptr};
    static QCoreApplication* app = []() {
        auto* instance = new QCoreApplication(argc, argv);
        QCoreApplication::setApplicationName(QString::fromLatin1(appName));
        return instance;
    }();
    return app;
}

void registerCoreSettings(SettingsManager& settings)
{
    settings.createSetting<Settings::Core::ShuffleAlbumsGroupScript>(u"%album%"_s,
                                                                     u"Playback/ShuffleAlbumsGroupScript"_s);
    settings.createSetting<Settings::Core::ShuffleAlbumsSortScript>(u"%track%"_s,
                                                                    u"Playback/ShuffleAlbumsSortScript"_s);
}

bool createPlaylistTables(const DbConnectionPoolPtr& dbPool)
{
    const DbConnectionProvider dbProvider{dbPool};

    DbQuery createPlaylists{dbProvider.db(), u"CREATE TABLE IF NOT EXISTS Playlists ("
                                             "PlaylistID INTEGER PRIMARY KEY AUTOINCREMENT, "
                                             "Name TEXT NOT NULL UNIQUE, "
                                             "PlaylistIndex INTEGER, "
                                             "IsAutoPlaylist INTEGER DEFAULT 0, "
                                             "Query TEXT, "
                                             "SortQuery TEXT, "
                                             "ForceSorted INTEGER DEFAULT 1, "
                                             "ExtraProperties BLOB);"_s};
    if(!createPlaylists.exec()) {
        return false;
    }

    DbQuery createPlaylistTracks{dbProvider.db(), u"CREATE TABLE IF NOT EXISTS PlaylistTracks ("
                                                  "PlaylistID INTEGER NOT NULL, "
                                                  "TrackID INTEGER NOT NULL, "
                                                  "TrackIndex INTEGER NOT NULL);"_s};
    return createPlaylistTracks.exec();
}

Track makeTrack(const QString& path, int id)
{
    Track track{path, 0};
    track.setId(id);
    track.setTitle(QFileInfo{path}.completeBaseName());
    track.generateHash();
    return track;
}

class StubMusicLibrary : public MusicLibrary
{
public:
    explicit StubMusicLibrary(QObject* parent = nullptr)
        : MusicLibrary(parent)
    { }

    void setTracks(TrackList tracks)
    {
        m_tracks = std::move(tracks);
    }

    void emitTracksLoaded()
    {
        Q_EMIT tracksLoaded(m_tracks);
    }

    bool hasLibrary() const override
    {
        return false;
    }

    std::optional<LibraryInfo> libraryInfo(int) const override
    {
        return std::nullopt;
    }

    std::optional<LibraryInfo> libraryForPath(const QString&) const override
    {
        return std::nullopt;
    }

    void loadAllTracks() override { }
    bool isEmpty() const override
    {
        return m_tracks.empty();
    }
    void refreshAll() override { }
    void rescanAll() override { }

    ScanRequest refresh(const LibraryInfo&) override
    {
        return {.type = ScanRequest::Library, .cancel = []() { }};
    }

    ScanRequest rescan(const LibraryInfo&) override
    {
        return {.type = ScanRequest::Library, .cancel = []() { }};
    }

    void cancelScan(int) override { }

    ScanRequest scanTracks(const TrackList&) override
    {
        return {.type = ScanRequest::Tracks, .cancel = []() { }};
    }

    ScanRequest scanModifiedTracks(const TrackList&) override
    {
        return {.type = ScanRequest::Tracks, .cancel = []() { }};
    }

    ScanRequest scanFiles(const QList<QUrl>&) override
    {
        return {.type = ScanRequest::Files, .cancel = []() { }};
    }

    ScanRequest loadPlaylist(const QList<QUrl>&) override
    {
        return {.type = ScanRequest::Playlist, .cancel = []() { }};
    }

    TrackList tracks() const override
    {
        return m_tracks;
    }

    TrackList libraryTracks() const override
    {
        return m_tracks;
    }

    Track trackForId(int id) const override
    {
        for(const auto& track : m_tracks) {
            if(track.id() == id) {
                return track;
            }
        }
        return {};
    }

    TrackList tracksForIds(const TrackIds& ids) const override
    {
        TrackList result;
        result.reserve(ids.size());
        for(const int id : ids) {
            if(const Track track = trackForId(id); track.isValid()) {
                result.emplace_back(track);
            }
        }
        return result;
    }

    std::shared_ptr<TrackMetadataStore> metadataStore() const override
    {
        return {};
    }

    void updateTrack(const Track&) override { }
    void updateTracks(const TrackList&) override { }
    void updateTrackMetadata(const TrackList&) override { }

    WriteRequest writeTrackMetadata(const TrackList&) override
    {
        return {};
    }

    WriteRequest writeTrackCovers(const TrackCoverData&) override
    {
        return {};
    }

    PendingTrackCoverProvider* pendingTrackCoverProvider() const override
    {
        return nullptr;
    }

    void updateTrackStats(const TrackList&) override { }
    void updateTrackStats(const Track&) override { }

    WriteRequest removeUnavailbleTracks() override
    {
        return {};
    }

    WriteRequest deleteTracks(const TrackList&) override
    {
        return {};
    }

private:
    TrackList m_tracks;
};

struct PlaylistHandlerHarness
{
    explicit PlaylistHandlerHarness(SettingsManager& settings, QString dbFilePath = {})
        : m_dbFilePath{std::move(dbFilePath)}
        , dbPool{[this]() {
            EXPECT_TRUE(dbDir.isValid());

            DbConnection::DbParams params;
            params.type     = u"QSQLITE"_s;
            params.filePath = !m_dbFilePath.isEmpty() ? m_dbFilePath : dbDir.filePath(u"playlisthandler.sqlite"_s);

            auto pool = DbConnectionPool::create(params, u"playlisthandler_test"_s);
            EXPECT_TRUE(pool);
            return pool;
        }()}
        , dbConnectionHandler{dbPool}
        , dbInitialised{dbConnectionHandler.hasConnection() && createPlaylistTables(dbPool)}
        , audioLoader{std::make_shared<AudioLoader>()}
        , handler{dbPool, audioLoader, &library, &settings}
    { }

    QTemporaryDir dbDir;
    QString m_dbFilePath;
    DbConnectionPoolPtr dbPool;
    DbConnectionHandler dbConnectionHandler;
    bool dbInitialised;
    std::shared_ptr<AudioLoader> audioLoader;
    StubMusicLibrary library;
    PlaylistHandler handler;
};
} // namespace

TEST(PlaylistHandlerTest, RecreatedDefaultCancelsPendingRemovalButRetainsArchivedPointer)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playlisthandler_default_recreate_test.ini"_s};
    registerCoreSettings(settings);
    PlaylistHandlerHarness harness{settings};
    ASSERT_TRUE(harness.dbInitialised);

    auto* originalDefault = harness.handler.playlistByName(u"Default"_s);
    ASSERT_NE(originalDefault, nullptr);
    const UId originalId = originalDefault->id();

    harness.handler.removePlaylist(originalId);

    auto* recreatedDefault = harness.handler.playlistByName(u"Default"_s);
    ASSERT_NE(recreatedDefault, nullptr);
    EXPECT_NE(recreatedDefault, originalDefault);
    EXPECT_NE(recreatedDefault->id(), originalId);

    const auto archived = harness.handler.removedPlaylists();
    ASSERT_EQ(archived.size(), 1U);
    EXPECT_EQ(archived.front(), originalDefault);
    EXPECT_EQ(originalDefault->name(), u"Default"_s);
    EXPECT_EQ(originalDefault->id(), originalId);

    EXPECT_TRUE(harness.handler.pendingRemovedPlaylists().empty());
}

TEST(PlaylistHandlerTest, ReaddingSameNameCancelsPendingRemovedExportOnly)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playlisthandler_pending_removed_test.ini"_s};
    registerCoreSettings(settings);
    PlaylistHandlerHarness harness{settings};
    ASSERT_TRUE(harness.dbInitialised);

    auto* firstPlaylist = harness.handler.createPlaylist(u"Session A"_s, {makeTrack(u"/tmp/a.flac"_s, 1)});
    auto* otherPlaylist = harness.handler.createPlaylist(u"Session B"_s, {makeTrack(u"/tmp/b.flac"_s, 2)});
    ASSERT_NE(firstPlaylist, nullptr);
    ASSERT_NE(otherPlaylist, nullptr);

    harness.handler.removePlaylist(firstPlaylist->id());

    const auto pendingAfterRemove = harness.handler.pendingRemovedPlaylists();
    ASSERT_EQ(pendingAfterRemove.size(), 1U);
    EXPECT_EQ(pendingAfterRemove.front(), firstPlaylist);

    auto* recreatedPlaylist = harness.handler.createPlaylist(u"Session A"_s, {});
    ASSERT_NE(recreatedPlaylist, nullptr);
    EXPECT_NE(recreatedPlaylist, firstPlaylist);

    const auto archived = harness.handler.removedPlaylists();
    ASSERT_EQ(archived.size(), 1U);
    EXPECT_EQ(archived.front(), firstPlaylist);
    EXPECT_EQ(firstPlaylist->name(), u"Session A"_s);
    EXPECT_TRUE(harness.handler.pendingRemovedPlaylists().empty());
}

TEST(PlaylistHandlerTest, TracksMetadataChangedUpdatesPlaylistTrackWhenFilepathChanges)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playlisthandler_filepath_update_test.ini"_s};
    registerCoreSettings(settings);
    PlaylistHandlerHarness harness{settings};
    ASSERT_TRUE(harness.dbInitialised);

    auto* playlist = harness.handler.createPlaylist(u"Renamed Track"_s, {makeTrack(u"/tmp/original.flac"_s, 1)});
    ASSERT_NE(playlist, nullptr);
    ASSERT_EQ(playlist->trackCount(), 1);

    std::vector<int> updatedIndexes;
    QObject::connect(&harness.handler, &PlaylistHandler::tracksChanged, &harness.handler,
                     [&updatedIndexes, playlist](Playlist* changedPlaylist, const std::vector<int>& indexes,
                                                 PlaylistTrackChangeSource) {
                         if(changedPlaylist == playlist) {
                             updatedIndexes = indexes;
                         }
                     });

    const Track renamedTrack = makeTrack(u"/tmp/renamed.flac"_s, 1);
    Q_EMIT harness.library.tracksMetadataChanged({renamedTrack});

    ASSERT_EQ(updatedIndexes, std::vector<int>({0}));

    const auto updatedTrack = playlist->playlistTrack(0);
    ASSERT_TRUE(updatedTrack.has_value());
    EXPECT_EQ(updatedTrack->track.id(), renamedTrack.id());
    EXPECT_EQ(updatedTrack->track.filepath(), renamedTrack.filepath());
}

TEST(PlaylistHandlerTest, TracksMetadataChangedUpdatesAutoPlaylistTrackCustomTags)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playlisthandler_auto_custom_tag_update_test.ini"_s};
    registerCoreSettings(settings);
    PlaylistHandlerHarness harness{settings};
    ASSERT_TRUE(harness.dbInitialised);

    Track originalTrack = makeTrack(u"/tmp/custom-tag.flac"_s, 1);
    originalTrack.replaceExtraTag(u"CUSTOM"_s, QStringList{u"Before"_s});
    harness.library.setTracks({originalTrack});

    auto* playlist = harness.handler.createNewAutoPlaylist(u"Custom Tag Auto"_s, u"title PRESENT"_s);
    ASSERT_NE(playlist, nullptr);
    ASSERT_EQ(playlist->trackCount(), 1);

    PlaylistChangeset changeSet;
    QObject::connect(
        &harness.handler, &PlaylistHandler::tracksPatched, &harness.handler,
        [&changeSet, playlist](Playlist* changedPlaylist, const PlaylistChangeset& changed, PlaylistTrackChangeSource) {
            if(changedPlaylist == playlist) {
                changeSet = changed;
            }
        });

    Track updatedTrack = makeTrack(u"/tmp/custom-tag.flac"_s, 1);
    updatedTrack.replaceExtraTag(u"CUSTOM"_s, QStringList{u"After"_s});
    ASSERT_EQ(updatedTrack.metaValue(u"custom"_s), u"After"_s);
    harness.library.setTracks({updatedTrack});
    Q_EMIT harness.library.tracksMetadataChanged({updatedTrack});

    const auto playlistTrack = playlist->playlistTrack(0);
    ASSERT_TRUE(playlistTrack.has_value());
    ASSERT_EQ(changeSet.updatedEntries.size(), 1);
    EXPECT_EQ(playlistTrack->track.metaValue(u"custom"_s), u"After"_s);
    EXPECT_EQ(changeSet.updatedEntries.front(), playlistTrack->entryId);
}
} // namespace Fooyin::Testing
