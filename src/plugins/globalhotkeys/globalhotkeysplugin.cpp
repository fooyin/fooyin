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

#include "globalhotkeysplugin.h"

#include "globalshortcutmanager.h"

#include <utils/actions/actionmanager.h>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(GLOBAL_HOTKEYS, "fy.globalhotkeys")

namespace Fooyin::GlobalHotkeys {
GlobalHotkeysPlugin::GlobalHotkeysPlugin() = default;

GlobalHotkeysPlugin::~GlobalHotkeysPlugin() = default;

void GlobalHotkeysPlugin::initialise(const GuiPluginContext& context)
{
    m_actionManager = context.actionManager;

    m_manager = std::make_unique<GlobalShortcutManager>(m_actionManager, this);

    QObject::connect(m_actionManager, &ActionManager::commandsChanged, this, &GlobalHotkeysPlugin::addCommands);
    QObject::connect(m_manager.get(), &GlobalShortcutManager::registrationFailed, this,
                     [](const Id& commandId, const QKeySequence& shortcut, const QString& error) {
                         qCWarning(GLOBAL_HOTKEYS)
                             << "Failed to register" << shortcut.toString(QKeySequence::NativeText) << "for"
                             << commandId.name() << ':' << error;
                     });
    QObject::connect(m_actionManager, &ActionManager::globalShortcutConfigurationRequested, m_manager.get(),
                     &GlobalShortcutManager::configure);

    auto management{GlobalShortcutManagement::Unavailable};
    if(m_manager->availability() == GlobalShortcutAvailability::PortalManaged) {
        management = GlobalShortcutManagement::SystemManaged;
    }
    else if(m_manager->availability() == GlobalShortcutAvailability::Available) {
        management = GlobalShortcutManagement::ApplicationManaged;
    }
    m_actionManager->setGlobalShortcutManagement(management);
    m_actionManager->setGlobalShortcutConfigurationAvailable(m_manager->configurationAvailable());

    addCommands();
}

void GlobalHotkeysPlugin::shutdown()
{
    m_actionManager->setGlobalShortcutManagement(GlobalShortcutManagement::Unavailable);
    m_actionManager->setGlobalShortcutConfigurationAvailable(false);
    QObject::disconnect(m_actionManager, nullptr, this, nullptr);

    m_manager.reset();
    m_actionManager = nullptr;
}

void GlobalHotkeysPlugin::addCommands()
{
    const auto commands = m_actionManager->commands();
    for(Command* command : commands) {
        m_manager->addCommand(command);
    }
}
} // namespace Fooyin::GlobalHotkeys
