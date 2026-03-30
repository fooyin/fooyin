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

#include <core/player/playercontroller.h>

#include <core/coresettings.h>
#include <core/engine/audioloader.h>
#include <core/library/musiclibrary.h>
#include <core/player/playerdefs.h>
#include <core/playlist/playlisthandler.h>
#include <core/track.h>
#include <utils/database/dbconnectionhandler.h>
#include <utils/database/dbconnectionpool.h>
#include <utils/database/dbconnectionprovider.h>
#include <utils/database/dbquery.h>
#include <utils/settings/settingsmanager.h>

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <gtest/gtest.h>

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
    static char appName[]        = "fooyin-playercontroller-test";
    static char* argv[]          = {appName, nullptr};
    static QCoreApplication* app = []() {
        auto* instance = new QCoreApplication(argc, argv);
        QCoreApplication::setApplicationName(QString::fromLatin1(appName));
        return instance;
    }();
    return app;
}

void registerControllerSettings(SettingsManager& settings)
{
    settings.createSetting<Settings::Core::PlayMode>(0, QString::fromLatin1(Settings::Core::PlayModeKey));
    settings.createSetting<Settings::Core::StopAfterCurrent>(false, u"Playback/StopAfterCurrent"_s);
    settings.createSetting<Settings::Core::ResetStopAfterCurrent>(false, u"Playback/ResetStopAfterCurrent"_s);
    settings.createSetting<Settings::Core::PlayedThreshold>(0.5, u"Playback/PlayedThreshold"_s);
    settings.createSetting<Settings::Core::RewindPreviousTrack>(false, u"Playlist/RewindPreviousTrack"_s);
    settings.createSetting<Settings::Core::PlaybackQueueStopWhenFinished>(false,
                                                                          u"Playback/PlaybackQueueStopWhenFinished"_s);
    settings.createSetting<Settings::Core::FollowPlaybackQueue>(false, u"Playback/FollowPlaybackQueue"_s);
    settings.createSetting<Settings::Core::ShuffleAlbumsGroupScript>(u"%album%"_s,
                                                                     u"Playback/ShuffleAlbumsGroupScript"_s);
    settings.createSetting<Settings::Core::ShuffleAlbumsSortScript>(u"%track%"_s,
                                                                    u"Playback/ShuffleAlbumsSortScript"_s);
    settings.createTempSetting<Settings::Core::ActiveTrack>(QVariant{});
    settings.createTempSetting<Settings::Core::ActiveTrackId>(-2);
}

Track makeTrack(const QString& path, int id, uint64_t durationMs)
{
    Track track{path, 0};
    track.setId(id);
    track.setDuration(durationMs);
    track.generateHash();
    return track;
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
                                             "ForceSorted INTEGER DEFAULT 1);"_s};
    if(!createPlaylists.exec()) {
        return false;
    }

    DbQuery createPlaylistTracks{dbProvider.db(), u"CREATE TABLE IF NOT EXISTS PlaylistTracks ("
                                                  "PlaylistID INTEGER NOT NULL, "
                                                  "TrackID INTEGER NOT NULL, "
                                                  "TrackIndex INTEGER NOT NULL);"_s};
    return createPlaylistTracks.exec();
}

class StubMusicLibrary : public MusicLibrary
{
public:
    explicit StubMusicLibrary(QObject* parent = nullptr)
        : MusicLibrary(parent)
    { }

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

    Track trackForId(int id) const override
    {
        const auto it = std::ranges::find_if(m_tracks, [id](const Track& track) { return track.id() == id; });
        return it != m_tracks.cend() ? *it : Track{};
    }

    TrackList tracksForIds(const TrackIds& ids) const override
    {
        TrackList result;
        for(const int id : ids) {
            if(const auto track = trackForId(id); track.isValid()) {
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

    void updateTrackStats(const TrackList&) override { }
    void updateTrackStats(const Track&) override { }

    WriteRequest removeUnavailbleTracks() override
    {
        return {};
    }

    WriteRequest deleteTracks(const TrackList& /*tracks*/) override
    {
        return {};
    }

private:
    TrackList m_tracks;
};

struct PlaylistHandlerHarness
{
    explicit PlaylistHandlerHarness(SettingsManager& settings)
        : dbPool{[this]() {
            EXPECT_TRUE(dbDir.isValid());

            DbConnection::DbParams params;
            params.type     = u"QSQLITE"_s;
            params.filePath = dbDir.filePath(u"playercontroller.sqlite"_s);

            auto pool = DbConnectionPool::create(params, u"playercontroller_test"_s);
            EXPECT_TRUE(pool);
            return pool;
        }()}
        , dbConnectionHandler{dbPool}
        , dbInitialised{dbConnectionHandler.hasConnection() && createPlaylistTables(dbPool)}
        , audioLoader{std::make_shared<AudioLoader>()}
        , handler{dbPool, audioLoader, &library, &settings}
    { }

    QTemporaryDir dbDir;
    DbConnectionPoolPtr dbPool;
    DbConnectionHandler dbConnectionHandler;
    bool dbInitialised;
    std::shared_ptr<AudioLoader> audioLoader;
    StubMusicLibrary library;
    PlaylistHandler handler;
};
} // namespace

TEST(PlayerControllerTest, ChangeAndCommitTrackUpdatePublicState)
{
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_change_commit_test.ini"_s};
    registerControllerSettings(settings);
    PlayerController controller{&settings, nullptr};

    int requestCount{0};
    Player::TrackChangeRequest lastRequest;
    QObject::connect(&controller, &PlayerController::trackChangeRequested, &controller,
                     [&requestCount, &lastRequest](const Player::TrackChangeRequest& request) {
                         ++requestCount;
                         lastRequest = request;
                     });

    const Track track = makeTrack(u"/tmp/controller.flac"_s, 11, 2000);
    controller.changeCurrentTrack(PlaylistTrack{.track = track, .playlistId = UId::create(), .indexInPlaylist = 0},
                                  {.reason = Player::AdvanceReason::ManualSelection, .userInitiated = true});

    ASSERT_EQ(requestCount, 1);
    EXPECT_EQ(lastRequest.track.track, track);
    EXPECT_GT(lastRequest.itemId, 0);
    EXPECT_EQ(lastRequest.context.reason, Player::AdvanceReason::ManualSelection);
    EXPECT_TRUE(lastRequest.context.userInitiated);

    controller.commitCurrentTrack(lastRequest);

    EXPECT_EQ(controller.currentTrack(), track);
    EXPECT_EQ(controller.currentTrackId(), track.id());
    EXPECT_EQ(controller.currentPlaylistTrack().track, track);
    EXPECT_EQ(controller.lastTrackChangeContext().reason, Player::AdvanceReason::ManualSelection);
    EXPECT_TRUE(controller.lastTrackChangeContext().userInitiated);
}

TEST(PlayerControllerTest, PlayAndPauseEmitTransportStateChanges)
{
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_play_pause_test.ini"_s};
    registerControllerSettings(settings);
    PlayerController controller{&settings, nullptr};

    int transportPlayCount{0};
    int transportPauseCount{0};
    int playStateChanges{0};

    QObject::connect(&controller, &PlayerController::transportPlayRequested, &controller,
                     [&transportPlayCount]() { ++transportPlayCount; });
    QObject::connect(&controller, &PlayerController::transportPauseRequested, &controller,
                     [&transportPauseCount]() { ++transportPauseCount; });
    QObject::connect(&controller, &PlayerController::playStateChanged, &controller,
                     [&playStateChanges]() { ++playStateChanges; });

    controller.changeCurrentTrack(makeTrack(u"/tmp/pending.flac"_s, 12, 1500));
    controller.play();

    EXPECT_EQ(controller.playState(), Player::PlayState::Playing);
    EXPECT_EQ(transportPlayCount, 1);
    EXPECT_EQ(playStateChanges, 1);

    controller.pause();

    EXPECT_EQ(controller.playState(), Player::PlayState::Paused);
    EXPECT_EQ(transportPauseCount, 1);
    EXPECT_EQ(playStateChanges, 2);
}

TEST(PlayerControllerTest, TrackPlayedEmitsOnceAfterCrossingThreshold)
{
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_track_played_test.ini"_s};
    registerControllerSettings(settings);
    PlayerController controller{&settings, nullptr};

    int playedCount{0};
    QObject::connect(&controller, &PlayerController::trackPlayed, &controller,
                     [&playedCount](const Track&) { ++playedCount; });

    controller.commitCurrentTrack(makeTrack(u"/tmp/played.flac"_s, 13, 1000));
    controller.setCurrentPosition(100);
    controller.setCurrentPosition(500);
    controller.setCurrentPosition(800);

    EXPECT_EQ(playedCount, 1);
}

TEST(PlayerControllerTest, NextClearsStopAfterCurrentWhenResetEnabled)
{
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_next_clears_stop_current_test.ini"_s};
    registerControllerSettings(settings);
    PlayerController controller{&settings, nullptr};

    settings.set<Settings::Core::StopAfterCurrent>(true);
    settings.set<Settings::Core::ResetStopAfterCurrent>(true);

    controller.next();

    EXPECT_FALSE(settings.value<Settings::Core::StopAfterCurrent>());
    EXPECT_TRUE(controller.trackEndAutoTransitionsEnabled());
}

TEST(PlayerControllerTest, PreviousClearsStopAfterCurrentWhenResetEnabled)
{
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_prev_clears_stop_current_test.ini"_s};
    registerControllerSettings(settings);
    PlayerController controller{&settings, nullptr};

    settings.set<Settings::Core::StopAfterCurrent>(true);
    settings.set<Settings::Core::ResetStopAfterCurrent>(true);
    settings.set<Settings::Core::RewindPreviousTrack>(true);

    controller.commitCurrentTrack(makeTrack(u"/tmp/prev-stop-current.flac"_s, 15, 10000));
    controller.setCurrentPosition(6000);
    controller.previous();

    EXPECT_FALSE(settings.value<Settings::Core::StopAfterCurrent>());
    EXPECT_TRUE(controller.trackEndAutoTransitionsEnabled());
}

TEST(PlayerControllerTest, CommittingQueueTrackOnlyRemovesFirstDuplicateQueueEntry)
{
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_queue_duplicate_commit_test.ini"_s};
    registerControllerSettings(settings);
    PlayerController controller{&settings, nullptr};

    const Track track = makeTrack(u"/tmp/queue-duplicate.flac"_s, 16, 1000);
    const PlaylistTrack queuedTrack{.track = track, .playlistId = {}};

    QueueTracks dequeuedTracks;
    QObject::connect(&controller, &PlayerController::tracksDequeued, &controller,
                     [&dequeuedTracks](const QueueTracks& tracks) {
                         dequeuedTracks.insert(dequeuedTracks.end(), tracks.cbegin(), tracks.cend());
                     });

    controller.queueTracks({queuedTrack, queuedTrack});
    controller.commitCurrentTrack(Player::TrackChangeRequest{
        .track        = queuedTrack,
        .context      = {.reason = Player::AdvanceReason::NaturalEnd, .userInitiated = false},
        .isQueueTrack = true,
        .itemId       = 102,
    });

    ASSERT_EQ(dequeuedTracks.size(), 1);
    EXPECT_EQ(dequeuedTracks.front(), queuedTrack);
    EXPECT_EQ(controller.queuedTracksCount(), 1);
    EXPECT_EQ(controller.playbackQueue().track(0), queuedTrack);
}

TEST(PlayerControllerTest, StopAfterCurrentResetWaitsForEngineStoppedState)
{
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_deferred_stop_current_reset_test.ini"_s};
    registerControllerSettings(settings);
    PlayerController controller{&settings, nullptr};

    settings.set<Settings::Core::StopAfterCurrent>(true);
    settings.set<Settings::Core::ResetStopAfterCurrent>(true);

    EXPECT_TRUE(settings.value<Settings::Core::StopAfterCurrent>());
    EXPECT_FALSE(controller.trackEndAutoTransitionsEnabled());

    controller.commitCurrentTrack(makeTrack(u"/tmp/stop-current.flac"_s, 14, 1000));
    controller.play();
    controller.advance(Player::AdvanceReason::NaturalEnd);

    EXPECT_TRUE(settings.value<Settings::Core::StopAfterCurrent>());
    EXPECT_FALSE(controller.trackEndAutoTransitionsEnabled());

    controller.syncPlayStateFromEngine(Player::PlayState::Stopped);

    EXPECT_FALSE(settings.value<Settings::Core::StopAfterCurrent>());
    EXPECT_TRUE(controller.trackEndAutoTransitionsEnabled());
}

TEST(PlayerControllerTest, CommitingPlaylistTrackSyncsActivePlaylistIndex)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_sync_active_playlist_test.ini"_s};
    registerControllerSettings(settings);
    PlaylistHandlerHarness harness{settings};
    ASSERT_TRUE(harness.dbInitialised);

    auto* playlist = harness.handler.createPlaylist(
        u"Active"_s, {makeTrack(u"/tmp/active-a.flac"_s, 21, 1000), makeTrack(u"/tmp/active-b.flac"_s, 22, 1000)});
    ASSERT_NE(playlist, nullptr);

    harness.handler.changeActivePlaylist(playlist);
    playlist->changeCurrentIndex(0);

    PlayerController controller{&settings, &harness.handler};
    const auto committedTrack = playlist->playlistTrack(1);
    ASSERT_TRUE(committedTrack.has_value());

    controller.commitCurrentTrack(Player::TrackChangeRequest{
        .track        = *committedTrack,
        .context      = {.reason = Player::AdvanceReason::NaturalEnd, .userInitiated = false},
        .isQueueTrack = false,
        .itemId       = 99,
    });

    EXPECT_EQ(controller.currentPlaylistTrack(), *committedTrack);
    EXPECT_EQ(playlist->currentTrackIndex(), 1);
}

TEST(PlayerControllerTest, CommitingQueueTrackDoesNotSyncActivePlaylistIndex)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_queue_sync_test.ini"_s};
    registerControllerSettings(settings);
    PlaylistHandlerHarness harness{settings};
    ASSERT_TRUE(harness.dbInitialised);

    auto* playlist = harness.handler.createPlaylist(
        u"QueueSource"_s, {makeTrack(u"/tmp/queue-a.flac"_s, 31, 1000), makeTrack(u"/tmp/queue-b.flac"_s, 32, 1000)});
    ASSERT_NE(playlist, nullptr);

    harness.handler.changeActivePlaylist(playlist);
    playlist->changeCurrentIndex(0);

    PlayerController controller{&settings, &harness.handler};
    const auto queuedTrack = playlist->playlistTrack(1);
    ASSERT_TRUE(queuedTrack.has_value());

    controller.commitCurrentTrack(Player::TrackChangeRequest{
        .track        = *queuedTrack,
        .context      = {.reason = Player::AdvanceReason::NaturalEnd, .userInitiated = false},
        .isQueueTrack = true,
        .itemId       = 100,
    });

    EXPECT_EQ(controller.currentPlaylistTrack(), *queuedTrack);
    EXPECT_EQ(playlist->currentTrackIndex(), 0);
}

TEST(PlayerControllerTest, CommitingNonActivePlaylistTrackDoesNotSyncPlaylistIndex)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_non_active_sync_test.ini"_s};
    registerControllerSettings(settings);
    PlaylistHandlerHarness harness{settings};
    ASSERT_TRUE(harness.dbInitialised);

    auto* activePlaylist
        = harness.handler.createPlaylist(u"Active"_s, {makeTrack(u"/tmp/nonactive-a.flac"_s, 41, 1000),
                                                       makeTrack(u"/tmp/nonactive-b.flac"_s, 42, 1000)});
    auto* otherPlaylist = harness.handler.createPlaylist(
        u"Other"_s, {makeTrack(u"/tmp/other-a.flac"_s, 51, 1000), makeTrack(u"/tmp/other-b.flac"_s, 52, 1000)});
    ASSERT_NE(activePlaylist, nullptr);
    ASSERT_NE(otherPlaylist, nullptr);

    harness.handler.changeActivePlaylist(activePlaylist);
    activePlaylist->changeCurrentIndex(0);
    otherPlaylist->changeCurrentIndex(0);

    PlayerController controller{&settings, &harness.handler};
    const auto otherTrack = otherPlaylist->playlistTrack(1);
    ASSERT_TRUE(otherTrack.has_value());

    controller.commitCurrentTrack(Player::TrackChangeRequest{
        .track        = *otherTrack,
        .context      = {.reason = Player::AdvanceReason::NaturalEnd, .userInitiated = false},
        .isQueueTrack = false,
        .itemId       = 101,
    });

    EXPECT_EQ(controller.currentPlaylistTrack(), *otherTrack);
    EXPECT_EQ(activePlaylist->currentTrackIndex(), 0);
    EXPECT_EQ(otherPlaylist->currentTrackIndex(), 0);
}
} // namespace Fooyin::Testing
