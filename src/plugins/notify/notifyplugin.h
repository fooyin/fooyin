/*
 * Fooyin
 * Copyright 2025, ripdog <https://github.com/ripdog>
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
#include <gui/coverprovider.h>
#include <gui/plugins/guiplugin.h>

namespace Fooyin {
struct PlaylistTrack;

namespace Notify {
class NotifyPage;
class NotifySettings;

class NotifyPlugin : public QObject,
                     public Plugin,
                     public CorePlugin,
                     public GuiPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.fooyin.fooyin.plugin/1.0" FILE "notify.json")
    Q_INTERFACES(Fooyin::Plugin Fooyin::CorePlugin Fooyin::GuiPlugin)

public:
    NotifyPlugin();

    void initialise(const CorePluginContext& context) override;
    void initialise(const GuiPluginContext& context) override;
    void shutdown() override;

private slots:
    void notificationClosed(uint id, uint reason);

private:
    void trackChanged(const PlaylistTrack& playlistTrack);
    void showNotification(const Track& track, const QPixmap& cover);
    void sendDbusNotification(const QString& title, const QString& body, const QPixmap& cover);

    PlayerController* m_playerController{nullptr};
    std::shared_ptr<AudioLoader> m_audioLoader;
    SettingsManager* m_settings{nullptr};
    CoverProvider* m_coverProvider{nullptr};

    std::unique_ptr<NotifySettings> m_notifySettings;
    NotifyPage* m_notifyPage{nullptr};

    ScriptParser m_scriptParser;
    uint32_t m_lastNotificationId{0};
};
} // namespace Notify
} // namespace Fooyin
