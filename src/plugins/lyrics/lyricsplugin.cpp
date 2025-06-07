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

#include "lyricsfinder.h"
#include "lyricssaver.h"
#include "lyricswidget.h"
#include "settings/lyricsgeneralpage.h"
#include "settings/lyricsguipage.h"
#include "settings/lyricssavingpage.h"
#include "settings/lyricssearchingpage.h"
#include "settings/lyricssettings.h"
#include "settings/lyricssourcespage.h"

#include <core/player/playercontroller.h>
#include <gui/theme/themeregistry.h>
#include <gui/widgetprovider.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Lyrics {
void LyricsPlugin::initialise(const CorePluginContext& context)
{
    m_playerController = context.playerController;
    m_engine           = context.engine;
    m_settings         = context.settingsManager;

    m_lyricsSettings = std::make_unique<LyricsSettings>(m_settings);
    m_lyricsFinder   = new LyricsFinder(context.networkAccess, m_settings, this);
    m_lyricsSaver    = new LyricsSaver(context.library, m_settings, this);

    m_lyricsFinder->restoreState();
}

void LyricsPlugin::initialise(const GuiPluginContext& context)
{
    m_actionManager  = context.actionManager;
    m_widgetProvider = context.widgetProvider;

    m_widgetProvider->registerWidget(
        u"Lyrics"_s,
        [this]() { return new LyricsWidget(m_playerController, m_engine, m_lyricsFinder, m_lyricsSaver, m_settings); },
        tr("Lyrics"));
    m_widgetProvider->setLimit(u"Lyrics"_s, 1);
    context.themeRegistry->registerFontEntry(tr("Lyrics"), u"Fooyin::Lyrics::LyricsArea"_s);

    new LyricsGeneralPage(m_settings, this);
    new LyricsGuiPage(m_settings, this);
    new LyricsSearchingPage(m_settings, this);
    new LyricsSourcesPage(m_lyricsFinder, m_settings, this);
    new LyricsSavingPage(m_settings, this);
}

void LyricsPlugin::shutdown()
{
    m_lyricsFinder->saveState();
}
} // namespace Fooyin::Lyrics

#include "moc_lyricsplugin.cpp"
