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

#include "spectrogramplugin.h"

#include "spectrogramwidget.h"

#include <core/engine/enginecontroller.h>
#include <core/player/playercontroller.h>
#include <gui/widgetprovider.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Spectrogram {
void SpectrogramPlugin::initialise(const CorePluginContext& context)
{
    m_playerController = context.playerController;
    m_engine           = context.engine;
    m_settings         = context.settingsManager;
}

void SpectrogramPlugin::initialise(const GuiPluginContext& context)
{
    context.widgetProvider->registerWidget(
        u"Spectrogram"_s, [this]() { return new SpectrogramWidget(m_playerController, m_engine, m_settings); },
        tr("Spectrogram"));
    context.widgetProvider->setSubMenus(u"Spectrogram"_s, {tr("Visualisations")});
}
} // namespace Fooyin::Spectrogram

#include "moc_spectrogramplugin.cpp"
