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
class SettingsDialogController;
class ActionManager;
} // namespace Utils

namespace Core {
class SettingsManager;
class ThreadManager;

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

namespace Gui::Widgets {
class WidgetFactory;
} // namespace Gui::Widgets

struct WidgetPluginContext
{
    WidgetPluginContext(Utils::ActionManager* actionManager, Core::Player::PlayerManager* playerManager,
                        Core::Library::MusicLibrary* library, Gui::Widgets::WidgetFactory* widgetFactory)
        : actionManager{actionManager}
        , playerManager{playerManager}
        , library{library}
        , widgetFactory{widgetFactory}
    { }

    Utils::ActionManager* actionManager;
    Core::Player::PlayerManager* playerManager;
    Core::Library::MusicLibrary* library;
    Gui::Widgets::WidgetFactory* widgetFactory;
};

struct SettingsPluginContext
{
    SettingsPluginContext(Core::SettingsManager* settingsManager, Utils::SettingsDialogController* settingsController)
        : settingsManager{settingsManager}
        , settingsController{settingsController}
    { }

    Core::SettingsManager* settingsManager;
    Utils::SettingsDialogController* settingsController;
};

struct ThreadPluginContext
{
    explicit ThreadPluginContext(Core::ThreadManager* threadManager)
        : threadManager{threadManager}
    { }

    Core::ThreadManager* threadManager;
};

struct DatabasePluginContext
{
    explicit DatabasePluginContext(Core::DB::Database* database)
        : database{database}
    { }

    Core::DB::Database* database;
};
