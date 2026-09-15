/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

namespace Fooyin::SleepInhibitor {
class SleepInhibitorPluginSettingsProvider : public QObject,
                                             public PluginSettingsProvider
{
    Q_OBJECT

Q_SIGNALS:
    void settingsChanged();

protected:
    QDialog* createSettings(QWidget* parent) override
    {
        auto* dialog = new SleepInhibitorSettings(parent);
        QObject::connect(dialog, &SleepInhibitorSettings::settingsChanged, this,
                         &SleepInhibitorPluginSettingsProvider::settingsChanged);
        return dialog;
    }
};

SleepInhibitorPlugin::SleepInhibitorPlugin()
    : m_inhibitor{new Inhibitor(this)}
{
    m_inhibitionType = m_settings.value(Settings::PreventDisplaySleep, false).toBool()
                         ? InhibitionType::DisplayAndSystem
                         : InhibitionType::System;
}

void SleepInhibitorPlugin::initialise(const CorePluginContext& context)
{
    m_playerController = context.playerController;
    QObject::connect(m_playerController, &PlayerController::playStateChanged, this,
                     &SleepInhibitorPlugin::updateInhibition);
    updateInhibition();
}

void SleepInhibitorPlugin::shutdown()
{
    m_inhibitor->uninhibitSleep();
}

std::unique_ptr<PluginSettingsProvider> SleepInhibitorPlugin::settingsProvider() const
{
    auto provider = std::make_unique<SleepInhibitorPluginSettingsProvider>();
    QObject::connect(provider.get(), &SleepInhibitorPluginSettingsProvider::settingsChanged, this,
                     &SleepInhibitorPlugin::updateInhibition);
    return provider;
}

void SleepInhibitorPlugin::updateInhibition()
{
    const auto enabled             = m_settings.value(Settings::Enabled, true).toBool();
    const auto onlyDuringPlayback  = m_settings.value(Settings::OnlyDuringPlayback, true).toBool();
    const auto preventDisplaySleep = m_settings.value(Settings::PreventDisplaySleep, false).toBool();
    const auto inhibitionType      = preventDisplaySleep ? InhibitionType::DisplayAndSystem : InhibitionType::System;

    if(enabled && m_inhibitionType != inhibitionType) {
        m_inhibitionType = inhibitionType;
        // Release the previous inhibitor so the next inhibitSleep call uses the new inhibition type.
        m_inhibitor->uninhibitSleep();
    }

    if(enabled && (!onlyDuringPlayback || m_playerController->playState() == Player::PlayState::Playing)) {
        m_inhibitor->inhibitSleep(m_inhibitionType);
    }
    else {
        m_inhibitor->uninhibitSleep();
    }
}

} // namespace Fooyin::SleepInhibitor

#include "moc_sleepinhibitorplugin.cpp"
#include "sleepinhibitorplugin.moc"
