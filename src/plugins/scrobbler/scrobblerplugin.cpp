/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "scrobblerplugin.h"

#include "scrobbler.h"
#include "scrobblerpage.h"
#include "scrobblersettings.h"

namespace Fooyin::Scrobbler {
void ScrobblerPlugin::initialise(const CorePluginContext& context)
{
    m_playerController = context.playerController;
    m_networkAccess    = context.m_networkAccess;
    m_settings         = context.settingsManager;

    m_scrobbler         = std::make_unique<Scrobbler>(m_playerController, m_networkAccess, m_settings);
    m_scrobblerSettings = std::make_unique<ScrobblerSettings>(m_settings);
}

void ScrobblerPlugin::initialise(const GuiPluginContext& /*context*/)
{
    new ScrobblerPage(m_scrobbler.get(), m_settings, this);
}

void ScrobblerPlugin::shutdown()
{
    m_scrobbler->saveCache();
}
} // namespace Fooyin::Scrobbler

#include "moc_scrobblerplugin.cpp"
