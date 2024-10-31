/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "playlist/playlistloader.h"

#include <core/engine/audioloader.h>
#include <utils/database/dbconnectionpool.h>

#include <QObject>

namespace Fooyin {
class ApplicationPrivate;
class Database;
class EngineController;
class LibraryManager;
class MusicLibrary;
class NetworkAccessManager;
class PlayerController;
class PlaylistHandler;
class PluginManager;
class SettingsManager;
class SortingRegistry;

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
    [[nodiscard]] PlaylistHandler* playlistHandler() const;
    [[nodiscard]] SettingsManager* settingsManager() const;
    [[nodiscard]] EngineController* engine() const;
    [[nodiscard]] std::shared_ptr<PlaylistLoader> playlistLoader() const;
    [[nodiscard]] std::shared_ptr<AudioLoader> audioLoader() const;
    [[nodiscard]] std::shared_ptr<NetworkAccessManager> networkManager() const;
    [[nodiscard]] SortingRegistry* sortingRegistry() const;

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    std::unique_ptr<ApplicationPrivate> p;
};
} // namespace Fooyin
