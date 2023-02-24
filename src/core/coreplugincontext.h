/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

namespace Utils {
class SettingsManager;
class SettingsDialogController;
class ActionManager;
class ThreadManager;
} // namespace Utils

namespace Core {
namespace DB {
class Database;
}

namespace Player {
class PlayerManager;
}

namespace Library {
class MusicLibrary;
}
} // namespace Core

struct CorePluginContext
{
    CorePluginContext(Utils::ActionManager* actionManager, Core::Player::PlayerManager* playerManager,
                      Core::Library::MusicLibrary* library, Utils::SettingsManager* settingsManager,
                      Utils::SettingsDialogController* settingsController, Utils::ThreadManager* threadManager,
                      Core::DB::Database* database)
        : actionManager{actionManager}
        , playerManager{playerManager}
        , library{library}
        , settingsManager{settingsManager}
        , settingsController{settingsController}
        , threadManager{threadManager}
        , database{database}
    { }

    Utils::ActionManager* actionManager;
    Core::Player::PlayerManager* playerManager;
    Core::Library::MusicLibrary* library;
    Utils::SettingsManager* settingsManager;
    Utils::SettingsDialogController* settingsController;
    Utils::ThreadManager* threadManager;
    Core::DB::Database* database;
};
