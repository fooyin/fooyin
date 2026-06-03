/*
 * Fooyin
 * Copyright © 2025, Luke Taylor <luket@pm.me>
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
#include <core/scripting/scriptparser.h>
#include <gui/plugins/guiplugin.h>

#include <QCoro/QCoroTask>

#include <optional>

namespace Fooyin {
class SignalThrottler;

namespace Discord {
class DiscordIPCClient;
class DiscordPage;
class DiscordSettings;

class DiscordPlugin : public QObject,
                      public Plugin,
                      public CorePlugin,
                      public GuiPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.fooyin.fooyin.plugin/1.0" FILE "discord.json")
    Q_INTERFACES(Fooyin::Plugin Fooyin::CorePlugin Fooyin::GuiPlugin)

public:
    DiscordPlugin();

    void initialise(const CorePluginContext& context) override;
    void initialise(const GuiPluginContext& context) override;
    void shutdown() override;

private:
    void startConnectTask();
    void startDisconnectTask();
    void startClientIdChangeTask(const QString& clientId);
    void startUpdateActivityTask();
    void startClearActivityTask();

    void toggleEnabled(bool enable);
    void updateActivity();
    void positionChanged(uint64_t ms);

    PlayerController* m_player;
    SettingsManager* m_settings;

    std::unique_ptr<DiscordSettings> m_discordSettings;
    DiscordPage* m_discordPage;

    DiscordIPCClient* m_discordClient;
    SignalThrottler* m_throttler;
    ScriptParser m_scriptParser;
    int m_positionSyncedTrackId;
    bool m_clearOnPause;

    std::optional<QCoro::Task<>> m_clientIdTask;
    std::optional<QCoro::Task<>> m_connectTask;
    std::optional<QCoro::Task<>> m_disconnectTask;
    std::optional<QCoro::Task<>> m_activityTask;
};
} // namespace Discord
} // namespace Fooyin
