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

#include "sleepinhibitorplugin.h"
#include "inhibitor.h"
#include "sleepinhibitorsettings.h"

#include <core/coresettings.h>
#include <core/player/playercontroller.h>
#include <utils/enum.h>

namespace Fooyin::SleepInhibitor {
Q_LOGGING_CATEGORY(SLEEPINHIBITOR, "fy.sleepinhibitor")

namespace {
constexpr auto SleepInhibitIntervalMs = 30000;

class SleepInhibitorPluginSettingsProvider : public PluginSettingsProvider
{
public:
    void showSettings(QWidget* parent) override
    {
        auto* dialog = new SleepInhibitorSettings(parent);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    }
};
} // namespace

SleepInhibitorPlugin::SleepInhibitorPlugin()
    : m_inhibitor{new Inhibitor(this)}
{
    m_inhibitTimer.start(SleepInhibitIntervalMs, this);
}

void SleepInhibitorPlugin::initialise(const CorePluginContext& context)
{
    m_playerController = context.playerController;
}

std::unique_ptr<PluginSettingsProvider> SleepInhibitorPlugin::settingsProvider() const
{
    return std::make_unique<SleepInhibitorPluginSettingsProvider>();
}

void SleepInhibitorPlugin::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_inhibitTimer.timerId()) {
        const auto enabled            = m_settings.value(Settings::Enabled, true).toBool();
        const auto onlyDuringPlayback = m_settings.value(Settings::OnlyDuringPlayback, true).toBool();
        if(enabled && (!onlyDuringPlayback || m_playerController->playState() == Player::PlayState::Playing)) {
            m_inhibitor->inhibitSleep();
        }
        else {
            m_inhibitor->uninhibitSleep();
        }
    }

    QObject::timerEvent(event);
}

} // namespace Fooyin::SleepInhibitor
