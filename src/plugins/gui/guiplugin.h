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

#include <pluginsystem/plugin.h>

namespace Gui {
class MainWindow;

namespace Widgets {
class WidgetFactory;
class WidgetProvider;
} // namespace Widgets

namespace Settings {
class SettingsDialog;
class GuiSettings;
} // namespace Settings

class GuiPlugin : public QObject,
                  public PluginSystem::Plugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.fooyin.plugin" FILE "metadata.json")
    Q_INTERFACES(PluginSystem::Plugin)

public:
    GuiPlugin();
    ~GuiPlugin() override;

    void initialise() override;
    void pluginsFinalised() override;

private:
    MainWindow* m_mainWindow;
    std::unique_ptr<Settings::GuiSettings> m_guiSettings;
    std::unique_ptr<Widgets::WidgetFactory> m_widgetFactory;
    std::unique_ptr<Widgets::WidgetProvider> m_widgetProvider;
    Settings::SettingsDialog* m_settingsDialog;
};
} // namespace Gui
