/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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
#include "scrobblertoggle.h"
#include "settings/scrobblerpage.h"
#include "settings/scrobblerservicespage.h"
#include "settings/scrobblersettings.h"

#include <gui/iconloader.h>
#include <gui/widgetprovider.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>

using namespace Qt::StringLiterals;

constexpr auto ScrobbleIcon = "scrobble";

namespace Fooyin::Scrobbler {
void ScrobblerPlugin::initialise(const CorePluginContext& context)
{
    m_playerController = context.playerController;
    m_networkAccess    = context.networkAccess;
    m_settings         = context.settingsManager;

    m_scrobblerSettings = std::make_unique<ScrobblerSettings>(m_settings);
    m_scrobbler         = std::make_unique<Scrobbler>(m_playerController, m_networkAccess, m_settings);
}

void ScrobblerPlugin::initialise(const GuiPluginContext& context)
{
    m_actionManager = context.actionManager;

    auto* toggleScrobble = new QAction(tr("Toggle scrobbling"), this);
    Gui::setThemeIcon(toggleScrobble, ScrobbleIcon);
    toggleScrobble->setCheckable(true);
    toggleScrobble->setChecked(m_settings->value<Settings::Scrobbler::ScrobblingEnabled>());
    QObject::connect(toggleScrobble, &QAction::triggered, this,
                     [this](bool enabled) { m_settings->set<Settings::Scrobbler::ScrobblingEnabled>(enabled); });
    m_settings->subscribe<Settings::Scrobbler::ScrobblingEnabled>(toggleScrobble, &QAction::setChecked);
    auto* toggleCmd = m_actionManager->registerAction(toggleScrobble, "Scrobbler.Toggle");
    toggleCmd->setCategories({tr("Scrobbler")});

    context.widgetProvider->registerWidget(
        u"ScrobbleToggle"_s, [this]() { return new ScrobblerToggle(m_actionManager, m_settings); },
        tr("Scrobble Toggle"));
    context.widgetProvider->setSubMenus(u"ScrobbleToggle"_s, {tr("Controls")});

    new ScrobblerPage(m_settings, this);
    new ScrobblerServicesPage(m_scrobbler.get(), m_settings, this);
}

void ScrobblerPlugin::shutdown()
{
    m_scrobbler->saveCache();
}
} // namespace Fooyin::Scrobbler

#include "moc_scrobblerplugin.cpp"
