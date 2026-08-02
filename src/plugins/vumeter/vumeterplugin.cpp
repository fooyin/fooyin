/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include "vumeterplugin.h"

#include "vumetercolours.h"
#include "vumeterwidget.h"

#include <core/engine/enginecontroller.h>
#include <core/player/playercontroller.h>
#include <gui/guiconstants.h>
#include <gui/widgetprovider.h>
#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>

#include <QAction>
#include <QMenu>

using namespace Qt::StringLiterals;

namespace Fooyin::VuMeter {
void VuMeterPlugin::initialise(const CorePluginContext& context)
{
    m_playerController = context.playerController;
    m_engine           = context.engine;
    m_settings         = context.settingsManager;
}

void VuMeterPlugin::initialise(const GuiPluginContext& context)
{
    m_widgetProvider = context.widgetProvider;

    qRegisterMetaType<Colours>("Fooyin::VuMeter::Colours");

    const auto addWindowAction = [this, actionManager = context.actionManager](
                                     const QString& text, const QString& statusTip, const char* actionId,
                                     VuMeterWidget::Type type, const QString& title, const QString& stateKey) {
        auto* action = new QAction(text, this);
        action->setStatusTip(statusTip);
        auto* command = actionManager->registerAction(action, actionId);
        command->setCategories({tr("View"), tr("Visualisations")});
        actionManager->actionContainer(Constants::Menus::Visualisations)->addAction(command);
        QObject::connect(action, &QAction::triggered, this, [this, type, title, stateKey]() {
            auto* window = new VuMeterWidget(type, m_playerController, m_settings);
            QObject::connect(m_engine, &EngineController::levelReady, window, &VuMeterWidget::renderLevel);
            window->showStandaloneWindow(title, stateKey);
        });
    };

    addWindowAction(tr("&VU Meter"), tr("Open a VU meter in a separate window"), "VUMeter.ShowWindow",
                    VuMeterWidget::Type::Rms, tr("VU Meter"), u"VUMeter/WindowState"_s);
    addWindowAction(tr("&Peak Meter"), tr("Open a peak meter in a separate window"), "PeakMeter.ShowWindow",
                    VuMeterWidget::Type::Peak, tr("Peak Meter"), u"PeakMeter/WindowState"_s);

    m_widgetProvider->registerWidget(
        u"VUMeter"_s,
        [this]() {
            auto* meter = new VuMeterWidget(VuMeterWidget::Type::Rms, m_playerController, m_settings);
            QObject::connect(m_engine, &EngineController::levelReady, meter, &VuMeterWidget::renderLevel);
            return meter;
        },
        u"VU Meter"_s);
    m_widgetProvider->setSubMenus(u"VUMeter"_s, {tr("Visualisations")});

    m_widgetProvider->registerWidget(
        u"PeakMeter"_s,
        [this]() {
            auto* meter = new VuMeterWidget(VuMeterWidget::Type::Peak, m_playerController, m_settings);
            QObject::connect(m_engine, &EngineController::levelReady, meter, &VuMeterWidget::renderLevel);
            return meter;
        },
        u"Peak Meter"_s);
    m_widgetProvider->setSubMenus(u"PeakMeter"_s, {tr("Visualisations")});
}
} // namespace Fooyin::VuMeter

#include "moc_vumeterplugin.cpp"
