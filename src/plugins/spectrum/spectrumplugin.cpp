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

#include "spectrumplugin.h"

#include "spectrumcolours.h"
#include "spectrumwidget.h"

#include <core/engine/enginecontroller.h>
#include <core/player/playercontroller.h>
#include <gui/widgetprovider.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Spectrum {
void SpectrumPlugin::initialise(const CorePluginContext& context)
{
    m_playerController = context.playerController;
    m_engine           = context.engine;
    m_settings         = context.settingsManager;
}

void SpectrumPlugin::initialise(const GuiPluginContext& context)
{
    m_widgetProvider = context.widgetProvider;

    qRegisterMetaType<Fooyin::Spectrum::Colours>("Fooyin::Spectrum::Colours");

    m_widgetProvider->registerWidget(
        u"Spectrum"_s, [this]() { return new SpectrumWidget(m_engine, m_playerController, m_settings); },
        tr("Spectrum"));
    m_widgetProvider->setSubMenus(u"Spectrum"_s, {tr("Visualisations")});
}
} // namespace Fooyin::Spectrum

#include "moc_spectrumplugin.cpp"
