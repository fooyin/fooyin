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

#include "guiplugin.h"

#include "gui/controls/controlwidget.h"
#include "gui/info/infowidget.h"
#include "gui/library/coverwidget.h"
#include "gui/library/statuswidget.h"
#include "gui/playlist/playlistwidget.h"
#include "gui/settings/settingsdialog.h"
#include "gui/widgets/spacer.h"
#include "guisettings.h"
#include "mainwindow.h"
#include "widgetfactory.h"
#include "widgetprovider.h"
#include "widgets/splitterwidget.h"

#include <core/actions/actionmanager.h>
#include <core/app/threadmanager.h>
#include <core/library/librarymanager.h>
#include <core/library/musiclibrary.h>
#include <core/settings/settingsmanager.h>
#include <pluginsystem/pluginmanager.h>

namespace Gui {
GuiPlugin::GuiPlugin() = default;

GuiPlugin::~GuiPlugin() = default;

void GuiPlugin::initialise()
{
    m_guiSettings    = std::make_unique<Settings::GuiSettings>();
    m_widgetFactory  = std::make_unique<Widgets::WidgetFactory>();
    m_widgetProvider = std::make_unique<Widgets::WidgetProvider>(m_widgetFactory.get());
    m_settingsDialog = new Settings::SettingsDialog(PluginSystem::object<Core::Library::LibraryManager>());
    m_mainWindow
        = new MainWindow(PluginSystem::object<Core::ActionManager>(), PluginSystem::object<Core::SettingsManager>(),
                         m_settingsDialog, PluginSystem::object<Core::Library::MusicLibrary>());

    m_mainWindow->setAttribute(Qt::WA_DeleteOnClose);

    connect(m_mainWindow, &MainWindow::closing, PluginSystem::object<Core::ThreadManager>(),
            &Core::ThreadManager::close);

    PluginSystem::addObject(m_widgetFactory.get());
    PluginSystem::addObject(m_widgetProvider.get());
    PluginSystem::addObject(m_mainWindow);
    PluginSystem::addObject(m_settingsDialog);

    m_widgetFactory->registerClass<Widgets::ControlWidget>("Controls");
    m_widgetFactory->registerClass<Widgets::InfoWidget>("Info");
    m_widgetFactory->registerClass<Widgets::CoverWidget>("Artwork");
    m_widgetFactory->registerClass<Widgets::PlaylistWidget>("Playlist");
    m_widgetFactory->registerClass<Widgets::Spacer>("Spacer");
    m_widgetFactory->registerClass<Widgets::VerticalSplitterWidget>("Vertical", {"Splitter"});
    m_widgetFactory->registerClass<Widgets::HorizontalSplitterWidget>("Horiztonal", {"Splitter"});
    m_widgetFactory->registerClass<Widgets::StatusWidget>("Status");
}

void GuiPlugin::pluginsFinalised()
{
    m_mainWindow->setupUi();
    m_mainWindow->show();
}
} // namespace Gui
