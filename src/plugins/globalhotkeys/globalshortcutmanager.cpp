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

#include "globalshortcutmanager.h"

#include "globalshortcutbackend.h"

#ifdef FOOYIN_HAVE_GLOBAL_SHORTCUT_PORTAL
#include "globalshortcutportalbackend.h"
#endif
#ifdef FOOYIN_HAVE_MACOS_GLOBAL_SHORTCUTS
#include "globalshortcutmacosbackend.h"
#endif
#ifdef FOOYIN_HAVE_WINDOWS_GLOBAL_SHORTCUTS
#include "globalshortcutwindowsbackend.h"
#endif
#ifdef FOOYIN_HAVE_X11_GLOBAL_SHORTCUTS
#include "globalshortcutx11backend.h"
#endif

#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>

#include <QAction>
#include <QGuiApplication>

#include <ranges>

using namespace Qt::StringLiterals;

namespace Fooyin::GlobalHotkeys {
namespace {
class UnavailableGlobalShortcutBackend : public GlobalShortcutBackend
{
public:
    using GlobalShortcutBackend::GlobalShortcutBackend;

    [[nodiscard]] GlobalShortcutAvailability availability() const override
    {
        return GlobalShortcutAvailability::Unavailable;
    }

    void applyBindings(const GlobalShortcutDescriptorList& /*bindings*/) override { }
    void clearBindings() override { }
};

std::unique_ptr<GlobalShortcutBackend> createGlobalShortcutBackend()
{
#ifdef FOOYIN_HAVE_WINDOWS_GLOBAL_SHORTCUTS
    return std::make_unique<GlobalShortcutWindowsBackend>();
#elifdef FOOYIN_HAVE_MACOS_GLOBAL_SHORTCUTS
    return std::make_unique<GlobalShortcutMacosBackend>();
#else
#ifdef FOOYIN_HAVE_GLOBAL_SHORTCUT_PORTAL
    if(QGuiApplication::platformName().startsWith("wayland"_L1)) {
        return std::make_unique<GlobalShortcutPortalBackend>();
    }
#endif
#ifdef FOOYIN_HAVE_X11_GLOBAL_SHORTCUTS
    if(QGuiApplication::platformName() == "xcb"_L1) {
        return std::make_unique<GlobalShortcutX11Backend>();
    }
#endif
    return std::make_unique<UnavailableGlobalShortcutBackend>();
#endif
}
} // namespace

GlobalShortcutManager::GlobalShortcutManager(ActionManager* actionManager, QObject* parent)
    : QObject{parent}
    , m_actionManager{actionManager}
    , m_backend{createGlobalShortcutBackend()}
{
    QObject::connect(m_backend.get(), &GlobalShortcutBackend::activated, this, &GlobalShortcutManager::activate);
    QObject::connect(m_backend.get(), &GlobalShortcutBackend::registrationFailed, this,
                     &GlobalShortcutManager::registrationFailure);
}

GlobalShortcutManager::~GlobalShortcutManager()
{
    clear();
}

GlobalShortcutAvailability GlobalShortcutManager::availability() const
{
    return m_backend->availability();
}

bool GlobalShortcutManager::configurationAvailable() const
{
    return m_backend->configurationAvailable();
}

void GlobalShortcutManager::addCommand(Command* command)
{
    if(!command) {
        return;
    }

    if(!m_commands.contains(command->id())) {
        const Id commandId = command->id();
        m_commands.emplace(commandId, command);

        QObject::connect(command, &Command::globalShortcutsChanged, this, &GlobalShortcutManager::scheduleSynchronise);
        QObject::connect(command, &Command::globalShortcutRegistrationChanged, this,
                         &GlobalShortcutManager::scheduleSynchronise);
        QObject::connect(command, &QObject::destroyed, this, [this, commandId] {
            m_commands.erase(commandId);
            scheduleSynchronise();
        });
    }

    scheduleSynchronise();
}

void GlobalShortcutManager::removeCommand(Command* command)
{
    if(!command || m_commands.erase(command->id()) == 0) {
        return;
    }

    QObject::disconnect(command, &Command::globalShortcutsChanged, this, &GlobalShortcutManager::scheduleSynchronise);
    QObject::disconnect(command, &Command::globalShortcutRegistrationChanged, this,
                        &GlobalShortcutManager::scheduleSynchronise);
    scheduleSynchronise();
}

void GlobalShortcutManager::clear()
{
    for(auto* command : m_commands | std::views::values) {
        QObject::disconnect(command, &Command::globalShortcutsChanged, this,
                            &GlobalShortcutManager::scheduleSynchronise);
        QObject::disconnect(command, &Command::globalShortcutRegistrationChanged, this,
                            &GlobalShortcutManager::scheduleSynchronise);
    }

    m_commands.clear();
    m_saveScheduled = false;
    m_syncScheduled = false;
    m_backend->clearBindings();
}

void GlobalShortcutManager::configure()
{
    m_backend->configure();
}

void GlobalShortcutManager::synchronise()
{
    m_syncScheduled = false;

    GlobalShortcutDescriptorList bindings;

    for(const auto& [id, command] : m_commands) {
        if(!command || !command->actionForContext(Constants::Context::Global)) {
            continue;
        }

        if(availability() == GlobalShortcutAvailability::PortalManaged) {
            if(command->isGlobalShortcutRegistered()) {
                bindings.emplace_back(id, command->description());
            }
        }
        else {
            const auto globalShortcuts = command->globalShortcuts();
            for(const auto& shortcut : globalShortcuts) {
                if(!shortcut.isEmpty()) {
                    bindings.emplace_back(id, command->description(), shortcut);
                }
            }
        }
    }

    std::ranges::sort(bindings, [](const auto& lhs, const auto& rhs) {
        if(lhs.commandId != rhs.commandId) {
            return lhs.commandId.name() < rhs.commandId.name();
        }
        return lhs.shortcut.toString(QKeySequence::PortableText) < rhs.shortcut.toString(QKeySequence::PortableText);
    });

    m_backend->applyBindings(bindings);
}

void GlobalShortcutManager::scheduleSynchronise()
{
    if(std::exchange(m_syncScheduled, true)) {
        return;
    }
    QMetaObject::invokeMethod(this, &GlobalShortcutManager::synchronise, Qt::QueuedConnection);
}

void GlobalShortcutManager::registrationFailure(const Id& commandId, const QKeySequence& shortcut, const QString& error)
{
    const auto commandIt = m_commands.find(commandId);
    if(commandIt != m_commands.end() && commandIt->second) {
        Command* command = commandIt->second;

        if(availability() == GlobalShortcutAvailability::PortalManaged && shortcut.isEmpty()) {
            command->setGlobalShortcutRegistered(false);
            scheduleSaveSettings();
            Q_EMIT registrationFailed(commandId, shortcut, error);
            return;
        }

        ShortcutList globalShortcuts = command->globalShortcuts();
        if(globalShortcuts.removeAll(shortcut) > 0) {
            ShortcutList appShortcuts = command->shortcuts();
            if(!appShortcuts.contains(shortcut)) {
                appShortcuts.append(shortcut);
            }

            command->setGlobalShortcuts(globalShortcuts);
            command->setGlobalShortcutRegistered(!globalShortcuts.empty());
            command->setShortcut(appShortcuts);
            scheduleSaveSettings();
        }
    }

    Q_EMIT registrationFailed(commandId, shortcut, error);
}

void GlobalShortcutManager::scheduleSaveSettings()
{
    if(std::exchange(m_saveScheduled, true)) {
        return;
    }

    QMetaObject::invokeMethod(
        this,
        [this] {
            m_saveScheduled = false;
            m_actionManager->saveSettings();
        },
        Qt::QueuedConnection);
}

void GlobalShortcutManager::activate(const Id& commandId)
{
    if(!m_commands.contains(commandId)) {
        return;
    }

    const Command* command = m_commands.at(commandId);
    if(QAction* action = command->actionForContext(Constants::Context::Global);
       action && action->isEnabled() && action->isVisible()) {
        action->trigger();
    }
}
} // namespace Fooyin::GlobalHotkeys
