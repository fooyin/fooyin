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

#include "oscilloscopeplugin.h"

#include "oscilloscopecolours.h"
#include "oscilloscopewidget.h"

#include <core/engine/enginecontroller.h>
#include <core/player/playercontroller.h>
#include <gui/guiconstants.h>
#include <gui/widgetprovider.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>

#include <QAction>

using namespace Qt::StringLiterals;

namespace Fooyin::Oscilloscope {
void OscilloscopePlugin::initialise(const CorePluginContext& context)
{
    m_playerController = context.playerController;
    m_engine           = context.engine;
    m_settings         = context.settingsManager;
}

void OscilloscopePlugin::initialise(const GuiPluginContext& context)
{
    qRegisterMetaType<Colours>("Fooyin::Oscilloscope::Colours");

    auto* showOscilloscope = new QAction(tr("&Oscilloscope"), this);
    showOscilloscope->setStatusTip(tr("Open an oscilloscope in a separate window"));
    auto* showOscilloscopeCmd = context.actionManager->registerAction(showOscilloscope, "Oscilloscope.ShowWindow");
    showOscilloscopeCmd->setCategories({tr("View"), tr("Visualisations")});
    context.actionManager->actionContainer(Constants::Menus::Visualisations)->addAction(showOscilloscopeCmd);
    QObject::connect(showOscilloscope, &QAction::triggered, this, [this]() {
        auto* window = new OscilloscopeWidget(m_engine, m_playerController, m_settings);
        window->showStandaloneWindow(tr("Oscilloscope"), u"Oscilloscope/WindowState"_s, true);
    });

    context.widgetProvider->registerWidget(
        u"Oscilloscope"_s, [this]() { return new OscilloscopeWidget(m_engine, m_playerController, m_settings); },
        tr("Oscilloscope"));
    context.widgetProvider->setSubMenus(u"Oscilloscope"_s, {tr("Visualisations")});
}
} // namespace Fooyin::Oscilloscope

#include "moc_oscilloscopeplugin.cpp"
