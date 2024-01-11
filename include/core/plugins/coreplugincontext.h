/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

namespace Fooyin {
class PluginManager;
class SettingsManager;
class EngineHandler;
class PlayerManager;
class PlaylistManager;
class LibraryManager;
class MusicLibrary;

/*!
 * Passed to core plugins in CorePlugin::initialise.
 */
struct FYCORE_EXPORT CorePluginContext
{
    CorePluginContext(PluginManager* pluginManager, EngineHandler* engineHandler, PlayerManager* playerManager,
                      LibraryManager* libraryManager, MusicLibrary* library, PlaylistManager* playlistHandler,
                      SettingsManager* settingsManager)
        : pluginManager{pluginManager}
        , playerManager{playerManager}
        , libraryManager{libraryManager}
        , library{library}
        , playlistHandler{playlistHandler}
        , settingsManager{settingsManager}
        , engineHandler{engineHandler}
    { }

    PluginManager* pluginManager;
    PlayerManager* playerManager;
    LibraryManager* libraryManager;
    MusicLibrary* library;
    PlaylistManager* playlistHandler;
    SettingsManager* settingsManager;
    EngineHandler* engineHandler;
};
} // namespace Fooyin
