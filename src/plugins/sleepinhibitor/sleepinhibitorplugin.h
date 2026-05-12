/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
 * Copyright © 2026, Gustav Oechler <gustavoechler@gmail.com>
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

#include <core/coresettings.h>
#include <core/plugins/coreplugin.h>
#include <core/plugins/plugin.h>
#include <gui/plugins/pluginconfigguiplugin.h>
#include <gui/plugins/pluginsettingsprovider.h>

#include <QBasicTimer>
#include <QLoggingCategory>
#include <QObject>
#include <QTimerEvent>

inline Q_LOGGING_CATEGORY(SLEEPINHIBITOR, "fy.sleepinhibitor");

namespace Fooyin::SleepInhibitor {
class Inhibitor;

class SleepInhibitorPlugin : public QObject,
                             public Plugin,
                             public CorePlugin,
                             public PluginConfigGuiPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.fooyin.fooyin.plugin/1.0" FILE "sleepinhibitor.json")
    Q_INTERFACES(Fooyin::Plugin Fooyin::CorePlugin Fooyin::PluginConfigGuiPlugin)

public:
    SleepInhibitorPlugin();

    void initialise(const CorePluginContext& context) override;

    [[nodiscard]] std::unique_ptr<PluginSettingsProvider> settingsProvider() const override;

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    PlayerController* m_playerController;
    FySettings m_settings;
    Inhibitor* m_inhibitor;
    QBasicTimer m_inhibitTimer;
};
} // namespace Fooyin::SleepInhibitor
