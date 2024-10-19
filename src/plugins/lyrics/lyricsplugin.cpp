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

#include "lyricsplugin.h"

#include "lyricswidget.h"
#include "settings/lyricsgeneralpage.h"
#include "settings/lyricsguipage.h"
#include "settings/lyricssettings.h"

#include <core/player/playercontroller.h>
#include <gui/theme/themeregistry.h>
#include <gui/widgetprovider.h>

namespace Fooyin::Lyrics {
void LyricsPlugin::initialise(const CorePluginContext& context)
{
    m_playerController = context.playerController;
    m_settings         = context.settingsManager;

    m_lyricsSettings = std::make_unique<LyricsSettings>(m_settings);
}

void LyricsPlugin::initialise(const GuiPluginContext& context)
{
    m_actionManager  = context.actionManager;
    m_widgetProvider = context.widgetProvider;

    m_widgetProvider->registerWidget(
        QStringLiteral("Lyrics"), [this]() { return new LyricsWidget(m_playerController, m_settings); }, tr("Lyrics"));
    context.themeRegistry->registerFontEntry(tr("Lyrics"), QStringLiteral("Fooyin::Lyrics::LyricsArea"));

    new LyricsGeneralPage(m_settings, this);
    new LyricsGuiPage(m_settings, this);
}
} // namespace Fooyin::Lyrics

#include "moc_lyricsplugin.cpp"
