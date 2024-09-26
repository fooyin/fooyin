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

#include <core/plugins/coreplugin.h>
#include <core/plugins/plugin.h>
#include <gui/plugins/guiplugin.h>

namespace Fooyin::Scrobbler {
class Scrobbler;
class ScrobblerSettings;

class ScrobblerPlugin : public QObject,
                        public Plugin,
                        public CorePlugin,
                        public GuiPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.fooyin.fooyin.plugin/1.0" FILE "scrobbler.json")
    Q_INTERFACES(Fooyin::Plugin Fooyin::CorePlugin Fooyin::GuiPlugin)

public:
    void initialise(const CorePluginContext& context) override;
    void initialise(const GuiPluginContext& context) override;
    void shutdown() override;

private:
    PlayerController* m_playerController;
    std::shared_ptr<NetworkAccessManager> m_networkAccess;
    ActionManager* m_actionManager;
    SettingsManager* m_settings;

    std::unique_ptr<Scrobbler> m_scrobbler;
    std::unique_ptr<ScrobblerSettings> m_scrobblerSettings;
};
} // namespace Fooyin::Scrobbler
