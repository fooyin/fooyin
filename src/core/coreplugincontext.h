/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

namespace Fy {

namespace Plugins {
class PluginManager;
}

namespace Utils {
class SettingsManager;
class ActionManager;
} // namespace Utils

namespace Core {
namespace DB {
class Database;
}

namespace Player {
class PlayerManager;
}

namespace Playlist {
class PlaylistHandler;
}

namespace Library {
class LibraryManager;
class MusicLibrary;
class SortingRegistry;
} // namespace Library

struct CorePluginContext
{
    CorePluginContext(Plugins::PluginManager* pluginManager, Utils::ActionManager* actionManager,
                      Core::Player::PlayerManager* playerManager, Core::Library::LibraryManager* libraryManager,
                      Core::Library::MusicLibrary* library, Core::Playlist::PlaylistHandler* playlistHandler,
                      Utils::SettingsManager* settingsManager, Core::DB::Database* database,
                      Core::Library::SortingRegistry* sortingRegistry)
        : pluginManager{pluginManager}
        , actionManager{actionManager}
        , playerManager{playerManager}
        , libraryManager{libraryManager}
        , library{library}
        , playlistHandler{playlistHandler}
        , settingsManager{settingsManager}
        , database{database}
        , sortingRegistry{sortingRegistry}
    { }

    Plugins::PluginManager* pluginManager;
    Utils::ActionManager* actionManager;
    Core::Player::PlayerManager* playerManager;
    Core::Library::LibraryManager* libraryManager;
    Core::Library::MusicLibrary* library;
    Core::Playlist::PlaylistHandler* playlistHandler;
    Utils::SettingsManager* settingsManager;
    Core::DB::Database* database;
    Core::Library::SortingRegistry* sortingRegistry;
};
} // namespace Core
} // namespace Fy
