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

    p->widgetProvider->registerWidget(
        QStringLiteral("WaveBar"), [this]() { return new WaveBarWidget(p->playerManager, p->engine, p->settings); },
        QStringLiteral("Waveform Seekbar"));
}
} // namespace Fooyin::WaveBar

#include "moc_wavebarplugin.cpp"
