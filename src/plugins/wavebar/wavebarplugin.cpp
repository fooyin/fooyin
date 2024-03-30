/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "wavebarplugin.h"

#include "settings/wavebarguisettingspage.h"
#include "settings/wavebarsettings.h"
#include "settings/wavebarsettingspage.h"
#include "wavebarwidget.h"

#include <core/engine/enginecontroller.h>
#include <gui/widgetprovider.h>

namespace Fooyin::WaveBar {
struct WaveBarPlugin::Private
{
    PlayerController* playerManager;
    EngineController* engine;
    WidgetProvider* widgetProvider;
    SettingsManager* settings;

    std::unique_ptr<WaveBarSettings> waveBarSettings;
    std::unique_ptr<WaveBarSettingsPage> waveBarSettingsPage;
    std::unique_ptr<WaveBarGuiSettingsPage> waveBarGuiSettingsPage;
};

WaveBarPlugin::WaveBarPlugin()
    : p{std::make_unique<Private>()}
{ }

WaveBarPlugin::~WaveBarPlugin() = default;

void WaveBarPlugin::initialise(const CorePluginContext& context)
{
    p->playerManager = context.playerController;
    p->engine        = context.engine;
    p->settings      = context.settingsManager;
}

void WaveBarPlugin::initialise(const GuiPluginContext& context)
{
    p->widgetProvider = context.widgetProvider;

    p->waveBarSettings        = std::make_unique<WaveBarSettings>(p->settings);
    p->waveBarSettingsPage    = std::make_unique<WaveBarSettingsPage>(p->settings);
    p->waveBarGuiSettingsPage = std::make_unique<WaveBarGuiSettingsPage>(p->settings);

    p->widgetProvider->registerWidget(
        QStringLiteral("WaveBar"), [this]() { return new WaveBarWidget(p->playerManager, p->engine, p->settings); },
        QStringLiteral("Waveform Seekbar"));
    p->widgetProvider->setSubMenus(QStringLiteral("WaveBar"), {QStringLiteral("Controls")});
}
} // namespace Fooyin::WaveBar

#include "moc_wavebarplugin.cpp"
