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

#include <core/coreplugincontext.h>
#include <core/coresettings.h>
#include <core/database/database.h>
#include <core/engine/enginehandler.h>
#include <core/playlist/libraryplaylistinterface.h>

#include <gui/guiplugincontext.h>
#include <gui/guisettings.h>
#include <gui/layoutprovider.h>
#include <gui/settings/guigeneralpage.h>
#include <gui/settings/librarygeneralpage.h>
#include <gui/settings/playlistguipage.h>
#include <gui/widgetfactory.h>
#include <gui/widgetprovider.h>

#include <utils/settings/settingsdialogcontroller.h>

#include <QApplication>

namespace Fy {

namespace Utils {
class ActionManager;
class SettingsManager;
class ThreadManager;
} // namespace Utils

namespace Plugins {
class PluginManager;
}

namespace Core {
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
} // namespace Core

namespace Gui {
class MainWindow;

namespace Widgets {
class EditableLayout;
}
} // namespace Gui

class Application : public QApplication
{
    Q_OBJECT

public:
    explicit Application(int& argc, char** argv, int flags = ApplicationFlags);
    ~Application() override;

    void startup();
    void shutdown();

private:
    void registerWidgets();
    void registerActions();
    void loadPlugins();

    Utils::ActionManager* m_actionManager;
    Utils::SettingsManager* m_settingsManager;
    Core::Settings::CoreSettings m_coreSettings;
    Utils::ThreadManager* m_threadManager;
    Core::DB::Database m_database;
    Core::Player::PlayerManager* m_playerManager;
    Core::Engine::EngineHandler m_engine;
    Core::Playlist::PlaylistManager* m_playlistHandler;
    Core::Library::LibraryManager* m_libraryManager;
    Core::Library::MusicLibrary* m_library;
    std::unique_ptr<Core::Playlist::LibraryPlaylistInterface> m_playlistInterface;

    Gui::Widgets::WidgetFactory m_widgetFactory;
    Gui::Widgets::WidgetProvider m_widgetProvider;
    Gui::Settings::GuiSettings m_guiSettings;
    Gui::Widgets::EditableLayout* m_editableLayout;
    Gui::LayoutProvider m_layoutProvider;
    Gui::MainWindow* m_mainWindow;

    Utils::SettingsDialogController m_settingsDialogController;

    //    Gui::Settings::GeneralPage m_generalPage;
    Gui::Settings::LibraryGeneralPage m_libraryGeneralPage;
    Gui::Settings::GuiGeneralPage m_guiGeneralPage;
    Gui::Settings::PlaylistGuiPage m_playlistGuiPage;

    Plugins::PluginManager* m_pluginManager;
    Core::CorePluginContext m_corePluginContext;
    Gui::GuiPluginContext m_guiPluginContext;
};
} // namespace Fy
