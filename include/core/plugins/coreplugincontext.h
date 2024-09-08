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

#include <memory>

namespace Fooyin {
class AudioLoader;
class EngineController;
class LibraryManager;
class MusicLibrary;
class NetworkAccessManager;
class PlayerController;
class PlaylistHandler;
class SettingsManager;
class SortingRegistry;

/*!
 * Passed to core plugins in CorePlugin::initialise.
 */
struct CorePluginContext
{
    CorePluginContext(EngineController* engine_, PlayerController* playerController_, LibraryManager* libraryManager_,
                      MusicLibrary* library_, PlaylistHandler* playlistHandler_, SettingsManager* settingsManager_,
                      std::shared_ptr<AudioLoader> audioLoader_, SortingRegistry* sortingRegistry_,
                      std::shared_ptr<NetworkAccessManager> networkAccess)
        : playerController{playerController_}
        , libraryManager{libraryManager_}
        , library{library_}
        , playlistHandler{playlistHandler_}
        , settingsManager{settingsManager_}
        , engine{engine_}
        , audioLoader{std::move(audioLoader_)}
        , sortingRegistry{sortingRegistry_}
        , m_networkAccess{std::move(networkAccess)}
    { }

    PlayerController* playerController;
    LibraryManager* libraryManager;
    MusicLibrary* library;
    PlaylistHandler* playlistHandler;
    SettingsManager* settingsManager;
    EngineController* engine;
    std::shared_ptr<AudioLoader> audioLoader;
    SortingRegistry* sortingRegistry;
    std::shared_ptr<NetworkAccessManager> m_networkAccess;
};
} // namespace Fooyin
