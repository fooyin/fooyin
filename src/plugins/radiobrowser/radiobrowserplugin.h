/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include <core/plugins/coreplugin.h>
#include <core/plugins/plugin.h>
#include <gui/plugins/guiplugin.h>
#include <utils/database/dbconnectionpool.h>

#include <QPointer>

namespace Fooyin {
class ActionManager;
class NetworkAccessManager;
class PlaylistLoader;
class TrackSelectionController;

namespace RadioBrowser {
class RadioBrowserConnectionManager;
class RadioBrowserController;
class RadioBrowserDialog;
class RadioStationStore;
class RadioBrowserWidget;

class RadioBrowserPlugin : public QObject,
                           public Plugin,
                           public CorePlugin,
                           public GuiPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.fooyin.fooyin.plugin/1.0" FILE "radiobrowser.json")
    Q_INTERFACES(Fooyin::Plugin Fooyin::CorePlugin Fooyin::GuiPlugin)

public:
    void initialise(const CorePluginContext& context) override;
    void initialise(const GuiPluginContext& context) override;

private:
    void registerLayouts(LayoutProvider* layoutProvider) const;
    void showRadioBrowserDialog();

    std::shared_ptr<NetworkAccessManager> m_network;
    std::shared_ptr<PlaylistLoader> m_playlistLoader;
    DbConnectionPoolPtr m_dbPool;
    PlayerController* m_playerController;
    SettingsManager* m_settings;
    WidgetProvider* m_widgetProvider;
    ActionManager* m_actionManager;
    TrackSelectionController* m_trackSelection;

    RadioStationStore* m_store;
    RadioBrowserController* m_controller;
    RadioBrowserConnectionManager* m_connectionManager;
};
} // namespace RadioBrowser
} // namespace Fooyin
