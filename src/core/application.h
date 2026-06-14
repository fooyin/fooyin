/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#pragma once

#include "fycore_export.h"

#include "internalcoresettings.h"
#include "playback/playbackqueuestore.h"
#include "translationloader.h"

#include <core/plugins/coreplugincontext.h>
#include <core/track.h>
#include <utils/database/dbconnectionprovider.h>

#include <QBasicTimer>
#include <QObject>

#include <memory>

namespace Fooyin {
class AudioLoader;
class Database;
class DspChainStore;
class DspRegistry;
class EngineController;
class EngineHandler;
class LibraryManager;
class MusicLibrary;
class NetworkAccessManager;
class PendingTrackCoverProvider;
class PlayerController;
class PlaylistHandler;
class PlaylistLoader;
class PluginManager;
class RemoteIoService;
class SettingsManager;
class SortingRegistry;
class UnifiedMusicLibrary;

class FYCORE_EXPORT Application : public QObject
{
    Q_OBJECT

public:
    explicit Application(QObject* parent = nullptr);
    ~Application() override;

    void startup();
    void shutdown();
    static void quit();
    static void restart();

    [[nodiscard]] Database* database() const;
    [[nodiscard]] DbConnectionPoolPtr databasePool() const;
    [[nodiscard]] PluginManager* pluginManager() const;
    [[nodiscard]] PlayerController* playerController() const;
    [[nodiscard]] LibraryManager* libraryManager() const;
    [[nodiscard]] MusicLibrary* library() const;
    [[nodiscard]] PendingTrackCoverProvider* pendingTrackCoverProvider() const;
    [[nodiscard]] PlaylistHandler* playlistHandler() const;
    [[nodiscard]] SettingsManager* settingsManager() const;
    [[nodiscard]] EngineController* engine() const;
    [[nodiscard]] std::shared_ptr<PlaylistLoader> playlistLoader() const;
    [[nodiscard]] std::shared_ptr<AudioLoader> audioLoader() const;
    [[nodiscard]] std::shared_ptr<NetworkAccessManager> networkManager() const;
    [[nodiscard]] std::shared_ptr<RemoteIoService> remoteIoService() const;
    [[nodiscard]] SortingRegistry* sortingRegistry() const;
    [[nodiscard]] DspRegistry* dspRegistry() const;
    [[nodiscard]] DspChainStore* dspChainStore() const;

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    void initialise();
    void registerPlaylistParsers();
    void registerInputs();

    void setupConnections();
    void markTrack(const Track& track) const;
    void tracksWereUpdated(const TrackList& tracks) const;
    void loadPlugins();

    void startSaveTimer();
    void exportAllPlaylists(bool shutdown);

    void loadDatabaseSettings() const;
    void saveDatabaseSettings() const;

    SettingsManager* m_settings;
    CoreSettings m_coreSettings;
    TranslationLoader m_translations;
    Database* m_database;
    std::shared_ptr<AudioLoader> m_audioLoader;
    std::unique_ptr<DspRegistry> m_dspRegistry;
    std::unique_ptr<DspChainStore> m_dspChainStore;
    std::shared_ptr<NetworkAccessManager> m_networkManager;
    std::shared_ptr<RemoteIoService> m_remoteIo;
    LibraryManager* m_libraryManager;
    std::shared_ptr<PlaylistLoader> m_playlistLoader;
    UnifiedMusicLibrary* m_library;
    PlaylistHandler* m_playlistHandler;
    PlayerController* m_playerController;
    PlaybackQueueStore m_playbackQueueStore;
    std::unique_ptr<EngineHandler> m_engine;
    SortingRegistry* m_sortingRegistry;

    std::unique_ptr<PluginManager> m_pluginManager;
    CorePluginContext m_corePluginContext;

    QBasicTimer m_playlistSaveTimer;
    QBasicTimer m_settingsSaveTimer;
};
} // namespace Fooyin
