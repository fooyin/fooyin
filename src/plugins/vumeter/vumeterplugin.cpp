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

#include "vumeterplugin.h"

#include "vumetersettings.h"
#include "vumetersettingspage.h"
#include "vumeterwidget.h"

#include <core/engine/enginecontroller.h>
#include <core/player/playercontroller.h>
#include <gui/widgetprovider.h>

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

    m_vuMeterSettings = std::make_unique<VuMeterSettings>(m_settings);
    new VuMeterSettingsPage(m_settings, this);

    m_widgetProvider->registerWidget(
        u"VUMeter"_s,
        [this]() {
            auto* meter = new VuMeterWidget(VuMeterWidget::Type::Rms, m_playerController, m_settings);
            QObject::connect(m_engine, &EngineController::bufferPlayed, meter, &VuMeterWidget::renderBuffer);
            return meter;
        },
        u"VU Meter"_s);
    m_widgetProvider->setSubMenus(u"VUMeter"_s, {tr("Visualisations")});

    m_widgetProvider->registerWidget(
        u"PeakMeter"_s,
        [this]() {
            auto* meter = new VuMeterWidget(VuMeterWidget::Type::Peak, m_playerController, m_settings);
            QObject::connect(m_engine, &EngineController::bufferPlayed, meter, &VuMeterWidget::renderBuffer);
            return meter;
        },
        u"Peak Meter"_s);
    m_widgetProvider->setSubMenus(u"PeakMeter"_s, {tr("Visualisations")});
}
} // namespace Fooyin::VuMeter

#include "moc_vumeterplugin.cpp"
