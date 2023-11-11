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

#include "fycore_export.h"

namespace Fy {
namespace Plugins {
class PluginManager;
}

namespace Utils {
class SettingsManager;
} // namespace Utils

namespace Core {
namespace Engine {
class EngineHandler;
}

namespace Player {
class PlayerManager;
}

namespace Playlist {
class PlaylistManager;
}

namespace Library {
class LibraryManager;
class MusicLibrary;
} // namespace Library

namespace Settings {
class CoreSettings;
}

struct FYCORE_EXPORT CorePluginContext
{
    CorePluginContext(Plugins::PluginManager* pluginManager, Core::Engine::EngineHandler* engineHandler,
                      Core::Player::PlayerManager* playerManager, Core::Library::LibraryManager* libraryManager,
                      Core::Library::MusicLibrary* library, Core::Playlist::PlaylistManager* playlistHandler,
                      Utils::SettingsManager* settingsManager, Core::Settings::CoreSettings* coreSettings)
        : pluginManager{pluginManager}
        , playerManager{playerManager}
        , libraryManager{libraryManager}
        , library{library}
        , playlistHandler{playlistHandler}
        , settingsManager{settingsManager}
        , engineHandler{engineHandler}
        , coreSettings{coreSettings}
    { }

    Plugins::PluginManager* pluginManager;
    Core::Player::PlayerManager* playerManager;
    Core::Library::LibraryManager* libraryManager;
    Core::Library::MusicLibrary* library;
    Core::Playlist::PlaylistManager* playlistHandler;
    Utils::SettingsManager* settingsManager;
    Core::Engine::EngineHandler* engineHandler;
    Core::Settings::CoreSettings* coreSettings;
};
} // namespace Core
} // namespace Fy
