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
class EngineController;
class PlayerController;
class PlaylistHandler;
class PlaylistParserRegistry;
class PluginManager;
class LibraryManager;
class MusicLibrary;
class SettingsManager;

/*!
 * Passed to core plugins in CorePlugin::initialise.
 */
struct CorePluginContext
{
    CorePluginContext(PluginManager* pluginManager_, EngineController* engine_, PlayerController* playerController_,
                      LibraryManager* libraryManager_, MusicLibrary* library_, PlaylistHandler* playlistHandler_,
                      SettingsManager* settingsManager_, PlaylistParserRegistry* playlistParsers_)
        : pluginManager{pluginManager_}
        , playerController{playerController_}
        , libraryManager{libraryManager_}
        , library{library_}
        , playlistHandler{playlistHandler_}
        , settingsManager{settingsManager_}
        , engine{engine_}
        , playlistParsers{playlistParsers_}
    { }

    PluginManager* pluginManager;
    PlayerController* playerController;
    LibraryManager* libraryManager;
    MusicLibrary* library;
    PlaylistHandler* playlistHandler;
    SettingsManager* settingsManager;
    EngineController* engine;
    PlaylistParserRegistry* playlistParsers;
};
} // namespace Fooyin
