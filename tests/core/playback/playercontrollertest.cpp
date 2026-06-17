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
#include <core/internalcoresettings.h>
#include <core/library/musiclibrary.h>
#include <core/player/playerdefs.h>
#include <core/playlist/playlisthandler.h>
#include <core/track.h>
#include <utils/database/dbconnectionhandler.h>
#include <utils/database/dbconnectionpool.h>
#include <utils/database/dbconnectionprovider.h>
#include <utils/database/dbquery.h>
#include <utils/enum.h>
#include <utils/settings/settingsmanager.h>

#include <QCoreApplication>
#include <QDir>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <gtest/gtest.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
namespace {
constexpr auto ActiveTrackIndexKey      = "Playlist/ActiveTrackIndex"_L1;
constexpr auto LastPlaybackPosition     = "Player/LastPosition"_L1;
constexpr auto LastPlaybackTimeListened = "Player/LastTimeListened"_L1;
constexpr auto LastPlaybackState        = "Player/LastState"_L1;

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

Track makeAlbumTrack(const QString& path, int id, uint64_t durationMs, const QString& album, const QString& trackNumber)
{
    Track track = makeTrack(path, id, durationMs);
    track.setAlbum(album);
    track.setTrackNumber(trackNumber);
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

    TrackList libraryTracks() const override
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

    WriteRequest deleteTracks(const TrackList& /*tracks*/) override
    {
        return {};
    }

    void emitTracksUpdatedForTests(const TrackList& tracks)
    {
        for(const auto& updatedTrack : tracks) {
            const auto it = std::ranges::find_if(m_tracks, [&updatedTrack](const Track& libraryTrack) {
                return updatedTrack.id() >= 0 ? libraryTrack.id() == updatedTrack.id()
                                              : libraryTrack.sameIdentityAs(updatedTrack);
            });

            if(it != m_tracks.end()) {
                *it = updatedTrack;
            }
        }

        Q_EMIT tracksUpdated(tracks);
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
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_change_commit_test.ini"_s};
    registerControllerSettings(settings);
    PlayerController controller{&settings, nullptr};

    QSignalSpy requestSpy{&controller, &PlayerController::trackChangeRequested};

    const Track track = makeTrack(u"/tmp/controller.flac"_s, 11, 2000);
    controller.changeCurrentTrack(
        PlaylistTrack{.track = track, .playlistId = UId::create(), .entryId = UId::create(), .indexInPlaylist = 0},
        {.reason = Player::AdvanceReason::ManualSelection, .userInitiated = true});

    ASSERT_EQ(requestSpy.count(), 1);
    const QVariantList requestArgs = requestSpy.takeFirst();
    ASSERT_EQ(requestArgs.size(), 1);
    const auto lastRequest = requestArgs.front().value<Player::TrackChangeRequest>();
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
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_play_pause_test.ini"_s};
    registerControllerSettings(settings);
    PlayerController controller{&settings, nullptr};

    QSignalSpy transportPlaySpy{&controller, &PlayerController::transportPlayRequested};
    QSignalSpy transportPauseSpy{&controller, &PlayerController::transportPauseRequested};
    QSignalSpy playStateSpy{&controller, &PlayerController::playStateChanged};

    controller.changeCurrentTrack(makeTrack(u"/tmp/pending.flac"_s, 12, 1500));
    controller.play();

    EXPECT_EQ(controller.playState(), Player::PlayState::Playing);
    EXPECT_EQ(transportPlaySpy.count(), 1);
    EXPECT_EQ(playStateSpy.count(), 1);

    controller.pause();

    EXPECT_EQ(controller.playState(), Player::PlayState::Paused);
    EXPECT_EQ(transportPauseSpy.count(), 1);
    EXPECT_EQ(playStateSpy.count(), 2);
}

TEST(PlayerControllerTest, TrackPlayedEmitsOnceAfterCrossingThreshold)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_track_played_test.ini"_s};
    registerControllerSettings(settings);
    PlayerController controller{&settings, nullptr};

    QSignalSpy playedSpy{&controller, &PlayerController::trackPlayed};

    controller.commitCurrentTrack(makeTrack(u"/tmp/played.flac"_s, 13, 1000));
    controller.setCurrentPosition(100);
    controller.setCurrentPosition(500);
    controller.setCurrentPosition(800);

    ASSERT_EQ(playedSpy.count(), 1);
    const QVariantList playedArgs = playedSpy.takeFirst();
    ASSERT_EQ(playedArgs.size(), 1);
    EXPECT_EQ(playedArgs.front().value<Track>(), makeTrack(u"/tmp/played.flac"_s, 13, 1000));
}

TEST(PlayerControllerTest, RestartingCurrentTrackAfterStoppedCountsListenedTime)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_stopped_restart_progress_test.ini"_s};
    registerControllerSettings(settings);
    PlayerController controller{&settings, nullptr};

    const QSignalSpy playedSpy{&controller, &PlayerController::trackPlayed};

    controller.commitCurrentTrack(makeTrack(u"/tmp/stopped-restart.flac"_s, 19, 1000));
    controller.syncPlayStateFromEngine(Player::PlayState::Playing);
    controller.setCurrentPosition(100);

    controller.syncPlayStateFromEngine(Player::PlayState::Stopped);
    controller.syncPlayStateFromEngine(Player::PlayState::Playing);
    controller.setCurrentPosition(100);
    controller.setCurrentPosition(500);

    EXPECT_EQ(playedSpy.count(), 1);
}

TEST(PlayerControllerTest, RestartingCurrentTrackWithPendingRequestCountsListenedTime)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_pending_restart_progress_test.ini"_s};
    registerControllerSettings(settings);
    PlayerController controller{&settings, nullptr};

    const QSignalSpy playedSpy{&controller, &PlayerController::trackPlayed};
    const Track track = makeTrack(u"/tmp/pending-restart.flac"_s, 22, 1000);

    controller.commitCurrentTrack(track);
    controller.syncPlayStateFromEngine(Player::PlayState::Playing);
    controller.setCurrentPosition(100);
    controller.syncPlayStateFromEngine(Player::PlayState::Stopped);

    const QSignalSpy currentTrackChangedSpy{&controller, &PlayerController::currentTrackChanged};

    controller.changeCurrentTrack(track);
    controller.play();
    controller.syncPlayStateFromEngine(Player::PlayState::Playing);
    controller.setCurrentPosition(100);
    controller.setCurrentPosition(500);

    EXPECT_EQ(currentTrackChangedSpy.count(), 0);
    EXPECT_EQ(playedSpy.count(), 1);
}

TEST(PlayerControllerTest, PendingTrackChangeDoesNotRestartPreviousTrackProgress)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_pending_change_progress_test.ini"_s};
    registerControllerSettings(settings);
    PlayerController controller{&settings, nullptr};

    const QSignalSpy playedSpy{&controller, &PlayerController::trackPlayed};

    controller.commitCurrentTrack(makeTrack(u"/tmp/previous-current.flac"_s, 20, 1000));
    controller.syncPlayStateFromEngine(Player::PlayState::Playing);
    controller.setCurrentPosition(100);
    controller.syncPlayStateFromEngine(Player::PlayState::Stopped);

    controller.changeCurrentTrack(makeTrack(u"/tmp/pending-current.flac"_s, 21, 1000));
    controller.play();
    controller.syncPlayStateFromEngine(Player::PlayState::Playing);
    controller.setCurrentPosition(500);

    EXPECT_EQ(playedSpy.count(), 0);
}

TEST(PlayerControllerTest, RestoringPastThresholdDoesNotEmitTrackPlayed)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_restore_threshold_test.ini"_s};
    registerControllerSettings(settings);
    PlayerController controller{&settings, nullptr};

    const QSignalSpy playedSpy{&controller, &PlayerController::trackPlayed};

    controller.commitCurrentTrack(makeTrack(u"/tmp/restored-played.flac"_s, 14, 1000));
    controller.restoreCurrentPosition(500);
    controller.setCurrentPosition(550);

    EXPECT_EQ(playedSpy.count(), 0);
}

TEST(PlayerControllerTest, RestoringPartialPlaybackProgressCountsOnlyRemainingListenedTime)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_restore_progress_test.ini"_s};
    registerControllerSettings(settings);
    PlayerController controller{&settings, nullptr};

    QSignalSpy playedSpy{&controller, &PlayerController::trackPlayed};

    controller.commitCurrentTrack(makeTrack(u"/tmp/restored-progress.flac"_s, 18, 1000));
    controller.restorePlaybackProgress(500, 350);
    controller.setCurrentPosition(540);
    EXPECT_EQ(playedSpy.count(), 0);

    controller.setCurrentPosition(560);
    EXPECT_EQ(playedSpy.count(), 1);
}

TEST(PlayerControllerTest, RestoringStartupTrackRequestsEngineLoadWithoutPrecommit)
{
    ensureCoreApplication();
    {
        FyStateSettings stateSettings;
        stateSettings.remove(ActiveTrackIndexKey);
        stateSettings.remove(LastPlaybackPosition);
        stateSettings.remove(LastPlaybackTimeListened);
        stateSettings.remove(LastPlaybackState);
    }

    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_startup_restore_request_test.ini"_s};
    registerControllerSettings(settings);
    settings.fileSet(Settings::Core::Internal::SaveActivePlaylistState, true);

    PlaylistHandlerHarness harness{settings};
    ASSERT_TRUE(harness.dbInitialised);

    auto* playlist
        = harness.handler.createPlaylist(u"StartupRestore"_s, {makeTrack(u"/tmp/startup-a.flac"_s, 201, 1000),
                                                               makeTrack(u"/tmp/startup-b.flac"_s, 202, 1000)});
    ASSERT_NE(playlist, nullptr);
    harness.handler.changeActivePlaylist(playlist);

    {
        FyStateSettings stateSettings;
        stateSettings.setValue(ActiveTrackIndexKey, 1);
        stateSettings.setValue(LastPlaybackPosition, QVariant::fromValue(uint64_t{450}));
        stateSettings.setValue(LastPlaybackTimeListened, QVariant::fromValue(uint64_t{320}));
        stateSettings.setValue(LastPlaybackState, Utils::Enum::toString(Player::PlayState::Paused));
    }

    const PlayerController controller{&settings, &harness.handler};
    QSignalSpy requestSpy{&controller, &PlayerController::trackChangeRequested};

    ASSERT_TRUE(QMetaObject::invokeMethod(&harness.handler, "playlistsPopulated", Qt::DirectConnection));

    ASSERT_EQ(requestSpy.count(), 1);
    const auto request = requestSpy.takeFirst().front().value<Player::TrackChangeRequest>();
    EXPECT_EQ(request.context.reason, Player::AdvanceReason::StartupRestore);
    EXPECT_FALSE(request.context.userInitiated);
    EXPECT_EQ(request.track.indexInPlaylist, 1);
    EXPECT_EQ(request.track.track.id(), 202);
    EXPECT_FALSE(controller.currentTrack().isValid());
    EXPECT_EQ(playlist->currentTrackIndex(), 1);

    {
        FyStateSettings stateSettings;
        stateSettings.remove(ActiveTrackIndexKey);
        stateSettings.remove(LastPlaybackPosition);
        stateSettings.remove(LastPlaybackTimeListened);
        stateSettings.remove(LastPlaybackState);
    }
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

TEST(PlayerControllerTest, PreviousCapabilityIncludesRewindingCurrentTrack)
{
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_prev_capability_rewind_test.ini"_s};
    registerControllerSettings(settings);
    settings.set<Settings::Core::RewindPreviousTrack>(true);

    PlayerController controller{&settings, nullptr};
    controller.commitCurrentTrack(makeTrack(u"/tmp/prev-capability.flac"_s, 17, 10000));

    EXPECT_FALSE(controller.hasPreviousTrack());

    controller.setCurrentPosition(6000);

    EXPECT_TRUE(controller.hasPreviousTrack());
}

TEST(PlayerControllerTest, RandomTrackRequestsDifferentTrackWhenPossible)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_random_track_test.ini"_s};
    registerControllerSettings(settings);
    PlaylistHandlerHarness harness{settings};
    ASSERT_TRUE(harness.dbInitialised);

    auto* playlist
        = harness.handler.createPlaylist(u"RandomTrack"_s, {makeTrack(u"/tmp/random-track-a.flac"_s, 31, 1000),
                                                            makeTrack(u"/tmp/random-track-b.flac"_s, 32, 1000)});
    ASSERT_NE(playlist, nullptr);

    harness.handler.changeActivePlaylist(playlist);
    playlist->changeCurrentIndex(0);

    PlayerController controller{&settings, &harness.handler};
    const auto committedTrack = playlist->playlistTrack(0);
    ASSERT_TRUE(committedTrack.has_value());
    controller.commitCurrentTrack(*committedTrack);

    QSignalSpy requestSpy{&controller, &PlayerController::trackChangeRequested};

    controller.randomTrack();

    ASSERT_EQ(requestSpy.count(), 1);
    const auto request = requestSpy.takeFirst().front().value<Player::TrackChangeRequest>();
    EXPECT_EQ(request.track.indexInPlaylist, 1);
    EXPECT_EQ(request.context.reason, Player::AdvanceReason::ManualSelection);
    EXPECT_TRUE(request.context.userInitiated);
}

TEST(PlayerControllerTest, RandomAlbumRequestsDifferentAlbumWhenPossible)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_random_album_test.ini"_s};
    registerControllerSettings(settings);
    PlaylistHandlerHarness harness{settings};
    ASSERT_TRUE(harness.dbInitialised);

    auto* playlist = harness.handler.createPlaylist(
        u"RandomAlbum"_s, {makeAlbumTrack(u"/tmp/random-album-a1.flac"_s, 41, 1000, u"Album A"_s, u"1"_s),
                           makeAlbumTrack(u"/tmp/random-album-a2.flac"_s, 42, 1000, u"Album A"_s, u"2"_s),
                           makeAlbumTrack(u"/tmp/random-album-b1.flac"_s, 43, 1000, u"Album B"_s, u"1"_s)});
    ASSERT_NE(playlist, nullptr);

    harness.handler.changeActivePlaylist(playlist);
    playlist->changeCurrentIndex(1);

    PlayerController controller{&settings, &harness.handler};
    const auto committedTrack = playlist->playlistTrack(1);
    ASSERT_TRUE(committedTrack.has_value());
    controller.commitCurrentTrack(*committedTrack);

    QSignalSpy requestSpy{&controller, &PlayerController::trackChangeRequested};

    controller.randomAlbum();

    ASSERT_EQ(requestSpy.count(), 1);
    const auto request = requestSpy.takeFirst().front().value<Player::TrackChangeRequest>();
    EXPECT_EQ(request.track.indexInPlaylist, 2);
    EXPECT_EQ(request.context.reason, Player::AdvanceReason::ManualSelection);
    EXPECT_TRUE(request.context.userInitiated);
}

TEST(PlayerControllerTest, PreviousAlbumRequestsFirstTrackOfPreviousAlbum)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_previous_album_test.ini"_s};
    registerControllerSettings(settings);
    PlaylistHandlerHarness harness{settings};
    ASSERT_TRUE(harness.dbInitialised);

    auto* playlist = harness.handler.createPlaylist(
        u"PreviousAlbum"_s, {makeAlbumTrack(u"/tmp/previous-album-a1.flac"_s, 51, 1000, u"Album A"_s, u"1"_s),
                             makeAlbumTrack(u"/tmp/previous-album-a2.flac"_s, 52, 1000, u"Album A"_s, u"2"_s),
                             makeAlbumTrack(u"/tmp/previous-album-b1.flac"_s, 53, 1000, u"Album B"_s, u"1"_s),
                             makeAlbumTrack(u"/tmp/previous-album-b2.flac"_s, 54, 1000, u"Album B"_s, u"2"_s)});
    ASSERT_NE(playlist, nullptr);

    harness.handler.changeActivePlaylist(playlist);
    playlist->changeCurrentIndex(3);

    PlayerController controller{&settings, &harness.handler};
    const auto committedTrack = playlist->playlistTrack(3);
    ASSERT_TRUE(committedTrack.has_value());
    controller.commitCurrentTrack(*committedTrack);

    QSignalSpy requestSpy{&controller, &PlayerController::trackChangeRequested};

    controller.previousAlbum();

    ASSERT_EQ(requestSpy.count(), 1);
    const auto request = requestSpy.takeFirst().front().value<Player::TrackChangeRequest>();
    EXPECT_EQ(request.track.indexInPlaylist, 0);
    EXPECT_EQ(request.context.reason, Player::AdvanceReason::ManualPrevious);
    EXPECT_TRUE(request.context.userInitiated);
}

TEST(PlayerControllerTest, NextAlbumRequestsFirstTrackOfNextAlbum)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_next_album_test.ini"_s};
    registerControllerSettings(settings);
    PlaylistHandlerHarness harness{settings};
    ASSERT_TRUE(harness.dbInitialised);

    auto* playlist = harness.handler.createPlaylist(
        u"NextAlbum"_s, {makeAlbumTrack(u"/tmp/next-album-a1.flac"_s, 61, 1000, u"Album A"_s, u"1"_s),
                         makeAlbumTrack(u"/tmp/next-album-a2.flac"_s, 62, 1000, u"Album A"_s, u"2"_s),
                         makeAlbumTrack(u"/tmp/next-album-b1.flac"_s, 63, 1000, u"Album B"_s, u"1"_s),
                         makeAlbumTrack(u"/tmp/next-album-b2.flac"_s, 64, 1000, u"Album B"_s, u"2"_s)});
    ASSERT_NE(playlist, nullptr);

    harness.handler.changeActivePlaylist(playlist);
    playlist->changeCurrentIndex(1);

    PlayerController controller{&settings, &harness.handler};
    const auto committedTrack = playlist->playlistTrack(1);
    ASSERT_TRUE(committedTrack.has_value());
    controller.commitCurrentTrack(*committedTrack);

    QSignalSpy requestSpy{&controller, &PlayerController::trackChangeRequested};

    controller.nextAlbum();

    ASSERT_EQ(requestSpy.count(), 1);
    const auto request = requestSpy.takeFirst().front().value<Player::TrackChangeRequest>();
    EXPECT_EQ(request.track.indexInPlaylist, 2);
    EXPECT_EQ(request.context.reason, Player::AdvanceReason::ManualNext);
    EXPECT_TRUE(request.context.userInitiated);
}

TEST(PlayerControllerTest, CommittingQueueTrackOnlyRemovesFirstDuplicateQueueEntry)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_queue_duplicate_commit_test.ini"_s};
    registerControllerSettings(settings);
    PlayerController controller{&settings, nullptr};

    const Track track = makeTrack(u"/tmp/queue-duplicate.flac"_s, 16, 1000);
    const PlaylistTrack queuedTrack{.track = track, .playlistId = {}, .entryId = {}};

    QSignalSpy dequeuedSpy{&controller, &PlayerController::tracksDequeued};

    controller.queueTracks({queuedTrack, queuedTrack});
    controller.commitCurrentTrack(Player::TrackChangeRequest{
        .track        = queuedTrack,
        .context      = {.reason = Player::AdvanceReason::NaturalEnd, .userInitiated = false},
        .isQueueTrack = true,
        .itemId       = 102,
    });

    ASSERT_EQ(dequeuedSpy.count(), 1);
    const QVariantList dequeuedArgs = dequeuedSpy.takeFirst();
    ASSERT_EQ(dequeuedArgs.size(), 1);
    const auto dequeuedTracks = dequeuedArgs.front().value<QueueTracks>();
    ASSERT_EQ(dequeuedTracks.size(), 1U);
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

TEST(PlayerControllerTest, StopAfterCurrentNaturalEndAdvancesPlaylistPositionBeforeStopping)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_stop_current_advances_test.ini"_s};
    registerControllerSettings(settings);
    PlaylistHandlerHarness harness{settings};
    ASSERT_TRUE(harness.dbInitialised);

    auto* playlist = harness.handler.createPlaylist(u"StopAfterCurrent"_s,
                                                    {makeTrack(u"/tmp/stop-after-current-a.flac"_s, 71, 1000),
                                                     makeTrack(u"/tmp/stop-after-current-b.flac"_s, 72, 1000)});
    ASSERT_NE(playlist, nullptr);

    harness.handler.changeActivePlaylist(playlist);
    playlist->changeCurrentIndex(0);

    PlayerController controller{&settings, &harness.handler};
    const auto committedTrack = playlist->playlistTrack(0);
    ASSERT_TRUE(committedTrack.has_value());
    controller.commitCurrentTrack(*committedTrack);

    settings.set<Settings::Core::StopAfterCurrent>(true);
    settings.set<Settings::Core::ResetStopAfterCurrent>(true);

    controller.play();
    controller.advance(Player::AdvanceReason::NaturalEnd);
    controller.syncPlayStateFromEngine(Player::PlayState::Stopped);

    EXPECT_EQ(playlist->currentTrackIndex(), 1);
    EXPECT_FALSE(settings.value<Settings::Core::StopAfterCurrent>());
    EXPECT_FALSE(controller.currentPlaylistTrack().track.isValid());

    QSignalSpy requestSpy{&controller, &PlayerController::trackChangeRequested};

    controller.play();

    ASSERT_EQ(requestSpy.count(), 1);
    const auto request = requestSpy.takeFirst().front().value<Player::TrackChangeRequest>();
    EXPECT_EQ(request.track.indexInPlaylist, 1);
    EXPECT_EQ(request.track.track.id(), 72);
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

TEST(PlayerControllerTest, SyncPlaylistTrackStateEmitsCurrentTrackUpdatedForMetadataChanges)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_sync_metadata_update_test.ini"_s};
    registerControllerSettings(settings);
    PlaylistHandlerHarness harness{settings};
    ASSERT_TRUE(harness.dbInitialised);

    auto* playlist = harness.handler.createPlaylist(u"Metadata"_s, {makeTrack(u"/tmp/metadata-a.flac"_s, 121, 1000),
                                                                    makeTrack(u"/tmp/metadata-b.flac"_s, 122, 1000)});
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

    QSignalSpy currentTrackUpdatedSpy{&controller, &PlayerController::currentTrackUpdated};
    QSignalSpy playlistTrackUpdatedSpy{&controller, &PlayerController::playlistTrackUpdated};

    PlaylistTrackList updatedTracks = playlist->playlistTracks();
    ASSERT_EQ(updatedTracks.size(), 2);
    updatedTracks[1].track.setTitle(u"Updated Title"_s);

    harness.handler.replacePlaylistTracks(playlist->id(), updatedTracks);

    ASSERT_EQ(currentTrackUpdatedSpy.count(), 1);
    EXPECT_EQ(currentTrackUpdatedSpy.at(0).at(0).value<Track>().title(), u"Updated Title"_s);
    EXPECT_EQ(controller.currentTrack().title(), u"Updated Title"_s);
    EXPECT_EQ(playlistTrackUpdatedSpy.count(), 0);
}

TEST(PlayerControllerTest, PlayFromIdleUsesActivePlaylistCurrentTrack)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_play_from_idle_current_track_test.ini"_s};
    registerControllerSettings(settings);
    PlaylistHandlerHarness harness{settings};
    ASSERT_TRUE(harness.dbInitialised);

    auto* playlist = harness.handler.createPlaylist(
        u"Idle"_s, {makeTrack(u"/tmp/idle-a.flac"_s, 41, 1000), makeTrack(u"/tmp/idle-b.flac"_s, 42, 1000)});
    ASSERT_NE(playlist, nullptr);

    harness.handler.changeActivePlaylist(playlist);
    playlist->changeCurrentIndex(0);

    PlayerController controller{&settings, &harness.handler};
    QSignalSpy requestSpy{&controller, &PlayerController::trackChangeRequested};

    controller.play();

    ASSERT_EQ(requestSpy.count(), 1);
    const QVariantList requestArgs = requestSpy.takeFirst();
    ASSERT_EQ(requestArgs.size(), 1);
    const auto request = requestArgs.front().value<Player::TrackChangeRequest>();

    EXPECT_EQ(request.track.indexInPlaylist, 0);
    EXPECT_EQ(request.track.track.id(), 41);
    EXPECT_EQ(playlist->currentTrackIndex(), 0);
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

TEST(PlayerControllerTest, FollowPlaybackQueueContinuesFromQueuedTrackPlaylist)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_follow_queue_playlist_test.ini"_s};
    registerControllerSettings(settings);
    settings.set<Settings::Core::FollowPlaybackQueue>(true);

    PlaylistHandlerHarness harness{settings};
    ASSERT_TRUE(harness.dbInitialised);

    auto* firstPlaylist  = harness.handler.createPlaylist(u"First"_s, {makeTrack(u"/tmp/first-a.flac"_s, 61, 1000),
                                                                       makeTrack(u"/tmp/first-b.flac"_s, 62, 1000),
                                                                       makeTrack(u"/tmp/first-c.flac"_s, 63, 1000)});
    auto* secondPlaylist = harness.handler.createPlaylist(u"Second"_s, {makeTrack(u"/tmp/second-a.flac"_s, 71, 1000),
                                                                        makeTrack(u"/tmp/second-b.flac"_s, 72, 1000),
                                                                        makeTrack(u"/tmp/second-c.flac"_s, 73, 1000)});
    ASSERT_NE(firstPlaylist, nullptr);
    ASSERT_NE(secondPlaylist, nullptr);

    harness.handler.changeActivePlaylist(firstPlaylist);
    firstPlaylist->changeCurrentIndex(0);
    secondPlaylist->changeCurrentIndex(0);

    PlayerController controller{&settings, &harness.handler};

    const auto firstTrack    = firstPlaylist->playlistTrack(0);
    const auto queuedTrack   = secondPlaylist->playlistTrack(1);
    const auto followedTrack = secondPlaylist->playlistTrack(2);
    ASSERT_TRUE(firstTrack.has_value());
    ASSERT_TRUE(queuedTrack.has_value());
    ASSERT_TRUE(followedTrack.has_value());

    QSignalSpy requestSpy{&controller, &PlayerController::trackChangeRequested};

    controller.commitCurrentTrack(Player::TrackChangeRequest{
        .track        = *firstTrack,
        .context      = {.reason = Player::AdvanceReason::ManualSelection, .userInitiated = true},
        .isQueueTrack = false,
        .itemId       = 201,
    });
    controller.queueTrack(*queuedTrack);

    controller.advance(Player::AdvanceReason::NaturalEnd);

    ASSERT_EQ(requestSpy.count(), 1);
    const auto queuedRequest = requestSpy.at(0).at(0).value<Player::TrackChangeRequest>();
    EXPECT_EQ(queuedRequest.track, *queuedTrack);
    EXPECT_TRUE(queuedRequest.isQueueTrack);

    controller.commitCurrentTrack(queuedRequest);

    EXPECT_EQ(controller.currentPlaylistTrack(), *queuedTrack);
    EXPECT_TRUE(controller.currentIsQueueTrack());
    EXPECT_EQ(controller.upcomingPlaylistTrack(), *followedTrack);

    controller.advance(Player::AdvanceReason::NaturalEnd);

    ASSERT_EQ(requestSpy.count(), 2);
    const auto followedRequest = requestSpy.at(1).at(0).value<Player::TrackChangeRequest>();
    EXPECT_EQ(followedRequest.track, *followedTrack);
    EXPECT_FALSE(followedRequest.isQueueTrack);
    EXPECT_EQ(harness.handler.activePlaylist(), secondPlaylist);
    EXPECT_EQ(secondPlaylist->currentTrackIndex(), 2);
}

TEST(PlayerControllerTest, FollowPlaybackQueuePreviousUsesQueuedTrackPlaylist)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_follow_queue_previous_test.ini"_s};
    registerControllerSettings(settings);
    settings.set<Settings::Core::FollowPlaybackQueue>(true);

    PlaylistHandlerHarness harness{settings};
    ASSERT_TRUE(harness.dbInitialised);

    auto* firstPlaylist
        = harness.handler.createPlaylist(u"FirstPrev"_s, {makeTrack(u"/tmp/first-prev-a.flac"_s, 81, 1000),
                                                          makeTrack(u"/tmp/first-prev-b.flac"_s, 82, 1000),
                                                          makeTrack(u"/tmp/first-prev-c.flac"_s, 83, 1000)});
    auto* secondPlaylist
        = harness.handler.createPlaylist(u"SecondPrev"_s, {makeTrack(u"/tmp/second-prev-a.flac"_s, 91, 1000),
                                                           makeTrack(u"/tmp/second-prev-b.flac"_s, 92, 1000),
                                                           makeTrack(u"/tmp/second-prev-c.flac"_s, 93, 1000)});
    ASSERT_NE(firstPlaylist, nullptr);
    ASSERT_NE(secondPlaylist, nullptr);

    harness.handler.changeActivePlaylist(firstPlaylist);
    firstPlaylist->changeCurrentIndex(1);
    secondPlaylist->changeCurrentIndex(0);

    PlayerController controller{&settings, &harness.handler};

    const auto firstTrack       = firstPlaylist->playlistTrack(1);
    const auto queuedTrack      = secondPlaylist->playlistTrack(1);
    const auto previousToQueued = secondPlaylist->playlistTrack(0);
    ASSERT_TRUE(firstTrack.has_value());
    ASSERT_TRUE(queuedTrack.has_value());
    ASSERT_TRUE(previousToQueued.has_value());

    QSignalSpy requestSpy{&controller, &PlayerController::trackChangeRequested};

    controller.commitCurrentTrack(Player::TrackChangeRequest{
        .track        = *firstTrack,
        .context      = {.reason = Player::AdvanceReason::ManualSelection, .userInitiated = true},
        .isQueueTrack = false,
        .itemId       = 301,
    });
    controller.queueTrack(*queuedTrack);

    controller.advance(Player::AdvanceReason::NaturalEnd);

    ASSERT_EQ(requestSpy.count(), 1);
    const auto queuedRequest = requestSpy.takeFirst().front().value<Player::TrackChangeRequest>();
    EXPECT_EQ(queuedRequest.track, *queuedTrack);
    EXPECT_TRUE(queuedRequest.isQueueTrack);

    controller.commitCurrentTrack(queuedRequest);

    EXPECT_TRUE(controller.currentIsQueueTrack());
    EXPECT_TRUE(controller.hasPreviousTrack());

    controller.previous();

    ASSERT_EQ(requestSpy.count(), 1);
    const auto previousRequest = requestSpy.takeFirst().front().value<Player::TrackChangeRequest>();
    EXPECT_EQ(previousRequest.track, *previousToQueued);
    EXPECT_FALSE(previousRequest.isQueueTrack);
    EXPECT_EQ(previousRequest.context.reason, Player::AdvanceReason::ManualPrevious);
    EXPECT_TRUE(previousRequest.context.userInitiated);
    EXPECT_EQ(harness.handler.activePlaylist(), secondPlaylist);
    EXPECT_EQ(secondPlaylist->currentTrackIndex(), 0);
}

TEST(PlayerControllerTest, FollowPlaybackQueueDoesNotFallBackAtQueuedTrackBoundaries)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_follow_queue_boundary_test.ini"_s};
    registerControllerSettings(settings);
    settings.set<Settings::Core::FollowPlaybackQueue>(true);

    PlaylistHandlerHarness harness{settings};
    ASSERT_TRUE(harness.dbInitialised);

    auto* firstPlaylist
        = harness.handler.createPlaylist(u"FirstBoundary"_s, {makeTrack(u"/tmp/first-boundary-a.flac"_s, 101, 1000),
                                                              makeTrack(u"/tmp/first-boundary-b.flac"_s, 102, 1000),
                                                              makeTrack(u"/tmp/first-boundary-c.flac"_s, 103, 1000)});
    auto* secondPlaylist
        = harness.handler.createPlaylist(u"SecondBoundary"_s, {makeTrack(u"/tmp/second-boundary-a.flac"_s, 111, 1000),
                                                               makeTrack(u"/tmp/second-boundary-b.flac"_s, 112, 1000),
                                                               makeTrack(u"/tmp/second-boundary-c.flac"_s, 113, 1000)});
    ASSERT_NE(firstPlaylist, nullptr);
    ASSERT_NE(secondPlaylist, nullptr);

    harness.handler.changeActivePlaylist(firstPlaylist);
    firstPlaylist->changeCurrentIndex(1);
    secondPlaylist->changeCurrentIndex(1);

    PlayerController controller{&settings, &harness.handler};

    const auto activeTrack     = firstPlaylist->playlistTrack(1);
    const auto firstQueueTrack = secondPlaylist->playlistTrack(0);
    const auto lastQueueTrack  = secondPlaylist->playlistTrack(2);
    ASSERT_TRUE(activeTrack.has_value());
    ASSERT_TRUE(firstQueueTrack.has_value());
    ASSERT_TRUE(lastQueueTrack.has_value());

    QSignalSpy requestSpy{&controller, &PlayerController::trackChangeRequested};

    controller.commitCurrentTrack(Player::TrackChangeRequest{
        .track        = *activeTrack,
        .context      = {.reason = Player::AdvanceReason::ManualSelection, .userInitiated = true},
        .isQueueTrack = false,
        .itemId       = 401,
    });
    controller.queueTrack(*firstQueueTrack);
    controller.advance(Player::AdvanceReason::NaturalEnd);

    ASSERT_EQ(requestSpy.count(), 1);
    controller.commitCurrentTrack(requestSpy.takeFirst().front().value<Player::TrackChangeRequest>());

    EXPECT_FALSE(controller.hasPreviousTrack());

    controller.previous();

    EXPECT_EQ(requestSpy.count(), 0);
    EXPECT_EQ(firstPlaylist->currentTrackIndex(), 1);

    controller.commitCurrentTrack(Player::TrackChangeRequest{
        .track        = *activeTrack,
        .context      = {.reason = Player::AdvanceReason::ManualSelection, .userInitiated = true},
        .isQueueTrack = false,
        .itemId       = 402,
    });
    controller.queueTrack(*lastQueueTrack);
    controller.advance(Player::AdvanceReason::NaturalEnd);

    ASSERT_EQ(requestSpy.count(), 1);
    controller.commitCurrentTrack(requestSpy.takeFirst().front().value<Player::TrackChangeRequest>());

    EXPECT_FALSE(controller.hasNextTrack());

    controller.next();

    EXPECT_EQ(requestSpy.count(), 0);
    EXPECT_EQ(firstPlaylist->currentTrackIndex(), 1);
}

TEST(PlayerControllerTest, LibraryTrackUpdatesPreserveShuffleHistory)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_preserve_shuffle_history_test.ini"_s};
    registerControllerSettings(settings);

    PlaylistHandlerHarness harness{settings};
    ASSERT_TRUE(harness.dbInitialised);

    const TrackList tracks{
        makeTrack(u"/tmp/shuffle-a.flac"_s, 81, 1000), makeTrack(u"/tmp/shuffle-b.flac"_s, 82, 1000),
        makeTrack(u"/tmp/shuffle-c.flac"_s, 83, 1000), makeTrack(u"/tmp/shuffle-d.flac"_s, 84, 1000),
        makeTrack(u"/tmp/shuffle-e.flac"_s, 85, 1000),
    };

    auto* playlist = harness.handler.createPlaylist(u"Shuffle"_s, tracks);
    ASSERT_NE(playlist, nullptr);

    harness.handler.changeActivePlaylist(playlist);
    playlist->changeCurrentIndex(0);

    const auto mode = Playlist::PlayModes(Playlist::ShuffleTracks);

    const Track advancedTrack = playlist->nextTrackChange(1, mode);
    ASSERT_TRUE(advancedTrack.isValid());

    const int currentIndex = playlist->currentTrackIndex();
    ASSERT_GT(currentIndex, 0);

    const int previousIndex = playlist->nextIndexFrom(currentIndex, -1, mode);
    ASSERT_EQ(previousIndex, 0);

    Track updatedTrack = playlist->currentTrack();
    updatedTrack.setPlayCount(updatedTrack.playCount() + 1);

    harness.library.emitTracksUpdatedForTests({updatedTrack});

    EXPECT_EQ(playlist->currentTrackIndex(), currentIndex);
    EXPECT_EQ(playlist->nextIndexFrom(currentIndex, -1, mode), previousIndex);
}

TEST(PlayerControllerTest, ReplacingPlayingPlaylistTracksPreservesCurrentEntryAcrossInsertedRows)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_replace_tracks_preserves_entry_test.ini"_s};
    registerControllerSettings(settings);
    PlaylistHandlerHarness harness{settings};
    ASSERT_TRUE(harness.dbInitialised);

    const TrackList tracks{
        makeTrack(u"/tmp/replace-a.flac"_s, 91, 1000), makeTrack(u"/tmp/replace-b.flac"_s, 92, 1000),
        makeTrack(u"/tmp/replace-c.flac"_s, 93, 1000), makeTrack(u"/tmp/replace-d.flac"_s, 94, 1000),
        makeTrack(u"/tmp/replace-e.flac"_s, 95, 1000), makeTrack(u"/tmp/replace-f.flac"_s, 96, 1000),
        makeTrack(u"/tmp/replace-g.flac"_s, 97, 1000),
    };

    auto* playlist = harness.handler.createPlaylist(u"Replace"_s, tracks);
    ASSERT_NE(playlist, nullptr);

    harness.handler.changeActivePlaylist(playlist);

    PlayerController controller{&settings, &harness.handler};
    const auto currentTrack = playlist->playlistTrack(3);
    ASSERT_TRUE(currentTrack.has_value());

    controller.commitCurrentTrack(Player::TrackChangeRequest{
        .track        = *currentTrack,
        .context      = {.reason = Player::AdvanceReason::ManualSelection, .userInitiated = true},
        .isQueueTrack = false,
        .itemId       = 301,
    });

    PlaylistTrackList updatedTracks = playlist->playlistTracks();
    PlaylistTrack duplicatedTrack   = updatedTracks.at(4);
    duplicatedTrack.entryId         = UId::create();
    updatedTracks.insert(updatedTracks.begin() + 2, duplicatedTrack);

    harness.handler.replacePlaylistTracks(playlist->id(), updatedTracks);

    const auto remappedTrack = controller.currentPlaylistTrack();
    EXPECT_EQ(remappedTrack.entryId, currentTrack->entryId);
    EXPECT_EQ(remappedTrack.track.id(), currentTrack->track.id());
    EXPECT_EQ(remappedTrack.indexInPlaylist, 4);
    EXPECT_EQ(playlist->currentTrackIndex(), 4);
}

TEST(PlayerControllerTest, ReplacingOtherPlaylistTracksDoesNotAffectCurrentPlaybackEntry)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_other_playlist_replace_test.ini"_s};
    registerControllerSettings(settings);
    PlaylistHandlerHarness harness{settings};
    ASSERT_TRUE(harness.dbInitialised);

    auto* firstPlaylist = harness.handler.createPlaylist(
        u"First"_s,
        {makeTrack(u"/tmp/other-first-a.flac"_s, 101, 1000), makeTrack(u"/tmp/other-first-b.flac"_s, 102, 1000),
         makeTrack(u"/tmp/other-first-c.flac"_s, 103, 1000), makeTrack(u"/tmp/other-first-d.flac"_s, 104, 1000),
         makeTrack(u"/tmp/other-first-e.flac"_s, 105, 1000), makeTrack(u"/tmp/other-first-f.flac"_s, 106, 1000),
         makeTrack(u"/tmp/other-first-g.flac"_s, 107, 1000)});
    auto* secondPlaylist = harness.handler.createPlaylist(u"Second"_s, {});
    ASSERT_NE(firstPlaylist, nullptr);
    ASSERT_NE(secondPlaylist, nullptr);

    harness.handler.changeActivePlaylist(firstPlaylist);

    PlayerController controller{&settings, &harness.handler};
    const auto currentTrack = firstPlaylist->playlistTrack(3);
    const auto nextTrack    = firstPlaylist->playlistTrack(4);
    ASSERT_TRUE(currentTrack.has_value());
    ASSERT_TRUE(nextTrack.has_value());

    controller.commitCurrentTrack(Player::TrackChangeRequest{
        .track        = *currentTrack,
        .context      = {.reason = Player::AdvanceReason::ManualSelection, .userInitiated = true},
        .isQueueTrack = false,
        .itemId       = 302,
    });

    const TrackList pastedTracks{nextTrack->track, nextTrack->track};
    harness.handler.replacePlaylistTracks(secondPlaylist->id(),
                                          PlaylistTrack::fromTracks(pastedTracks, secondPlaylist->id()));

    EXPECT_EQ(controller.currentPlaylistTrack().entryId, currentTrack->entryId);
    EXPECT_EQ(controller.currentPlaylistTrack().indexInPlaylist, 3);
    EXPECT_EQ(firstPlaylist->currentTrackIndex(), 3);

    QSignalSpy requestSpy{&controller, &PlayerController::trackChangeRequested};
    controller.next();

    ASSERT_EQ(requestSpy.count(), 1);
    const auto nextRequest = requestSpy.takeFirst().front().value<Player::TrackChangeRequest>();
    EXPECT_EQ(nextRequest.track.entryId, nextTrack->entryId);
    EXPECT_EQ(nextRequest.track.playlistId, firstPlaylist->id());
    EXPECT_EQ(nextRequest.track.indexInPlaylist, 4);
}

TEST(PlayerControllerTest, ReplacingPlaylistTracksBackToOriginalRestoresCurrentEntryIndex)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_replace_tracks_restore_index_test.ini"_s};
    registerControllerSettings(settings);
    PlaylistHandlerHarness harness{settings};
    ASSERT_TRUE(harness.dbInitialised);

    const TrackList tracks{
        makeTrack(u"/tmp/restore-a.flac"_s, 111, 1000), makeTrack(u"/tmp/restore-b.flac"_s, 112, 1000),
        makeTrack(u"/tmp/restore-c.flac"_s, 113, 1000), makeTrack(u"/tmp/restore-d.flac"_s, 114, 1000),
        makeTrack(u"/tmp/restore-e.flac"_s, 115, 1000),
    };

    auto* playlist = harness.handler.createPlaylist(u"Restore"_s, tracks);
    ASSERT_NE(playlist, nullptr);

    harness.handler.changeActivePlaylist(playlist);

    PlayerController controller{&settings, &harness.handler};
    const auto currentTrack = playlist->playlistTrack(3);
    ASSERT_TRUE(currentTrack.has_value());

    controller.commitCurrentTrack(Player::TrackChangeRequest{
        .track        = *currentTrack,
        .context      = {.reason = Player::AdvanceReason::ManualSelection, .userInitiated = true},
        .isQueueTrack = false,
        .itemId       = 303,
    });

    const PlaylistTrackList originalTracks = playlist->playlistTracks();
    PlaylistTrackList updatedTracks        = originalTracks;
    PlaylistTrack insertedTrack            = updatedTracks.at(4);
    insertedTrack.entryId                  = UId::create();
    updatedTracks.insert(updatedTracks.begin() + 2, insertedTrack);

    harness.handler.replacePlaylistTracks(playlist->id(), updatedTracks);
    ASSERT_EQ(controller.currentPlaylistTrack().indexInPlaylist, 4);

    harness.handler.replacePlaylistTracks(playlist->id(), originalTracks);

    EXPECT_EQ(controller.currentPlaylistTrack().entryId, currentTrack->entryId);
    EXPECT_EQ(controller.currentPlaylistTrack().indexInPlaylist, 3);
    EXPECT_EQ(playlist->currentTrackIndex(), 3);
}

TEST(PlayerControllerTest, RestoringRemovedPlayingEntryReattachesPlaylistTrackState)
{
    ensureCoreApplication();
    SettingsManager settings{QDir::tempPath() + u"/fooyin_playercontroller_restore_removed_playing_entry_test.ini"_s};
    registerControllerSettings(settings);
    PlaylistHandlerHarness harness{settings};
    ASSERT_TRUE(harness.dbInitialised);

    const TrackList tracks{
        makeTrack(u"/tmp/cut-a.flac"_s, 121, 1000), makeTrack(u"/tmp/cut-b.flac"_s, 122, 1000),
        makeTrack(u"/tmp/cut-c.flac"_s, 123, 1000), makeTrack(u"/tmp/cut-d.flac"_s, 124, 1000),
        makeTrack(u"/tmp/cut-e.flac"_s, 125, 1000),
    };

    auto* playlist = harness.handler.createPlaylist(u"CutUndo"_s, tracks);
    ASSERT_NE(playlist, nullptr);

    harness.handler.changeActivePlaylist(playlist);

    PlayerController controller{&settings, &harness.handler};
    const PlaylistTrackList originalTracks = playlist->playlistTracks();
    const auto currentTrack                = playlist->playlistTrack(3);
    ASSERT_TRUE(currentTrack.has_value());

    controller.commitCurrentTrack(Player::TrackChangeRequest{
        .track        = *currentTrack,
        .context      = {.reason = Player::AdvanceReason::ManualSelection, .userInitiated = true},
        .isQueueTrack = false,
        .itemId       = 304,
    });

    PlaylistTrackList removedTracks = originalTracks;
    removedTracks.erase(removedTracks.begin() + 3);
    harness.handler.replacePlaylistTracks(playlist->id(), removedTracks);

    EXPECT_FALSE(controller.currentPlaylistTrack().isInPlaylist());
    EXPECT_FALSE(controller.currentPlaylistTrack().entryId.isValid());
    EXPECT_EQ(controller.currentPlaylistTrack().track.id(), currentTrack->track.id());

    harness.handler.replacePlaylistTracks(playlist->id(), originalTracks);

    EXPECT_EQ(controller.currentPlaylistTrack().entryId, currentTrack->entryId);
    EXPECT_EQ(controller.currentPlaylistTrack().playlistId, playlist->id());
    EXPECT_EQ(controller.currentPlaylistTrack().indexInPlaylist, 3);
    EXPECT_EQ(controller.currentPlaylistTrack().track.id(), currentTrack->track.id());
    EXPECT_EQ(playlist->currentTrackIndex(), 3);
}
} // namespace Fooyin::Testing
