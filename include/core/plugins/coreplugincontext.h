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

namespace Fooyin {
class PluginManager;
class SettingsManager;
class EngineController;
class PlayerManager;
class PlaylistManager;
class LibraryManager;
class MusicLibrary;

/*!
 * Passed to core plugins in CorePlugin::initialise.
 */
struct CorePluginContext
{
    CorePluginContext(PluginManager* pluginManager_, EngineController* engine_, PlayerManager* playerManager_,
                      LibraryManager* libraryManager_, MusicLibrary* library_, PlaylistManager* playlistHandler_,
                      SettingsManager* settingsManager_)
        : pluginManager{pluginManager_}
        , playerManager{playerManager_}
        , libraryManager{libraryManager_}
        , library{library_}
        , playlistHandler{playlistHandler_}
        , settingsManager{settingsManager_}
        , engine{engine_}
    { }

    PluginManager* pluginManager;
    PlayerManager* playerManager;
    LibraryManager* libraryManager;
    MusicLibrary* library;
    PlaylistManager* playlistHandler;
    SettingsManager* settingsManager;
    EngineController* engine;
};
} // namespace Fooyin
