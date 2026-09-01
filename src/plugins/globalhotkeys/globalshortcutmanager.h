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

#pragma once

#include "globalshortcutbackend.h"

#include <QObject>

#include <map>
#include <memory>

namespace Fooyin {
class ActionManager;
class Command;

namespace GlobalHotkeys {
class GlobalShortcutManager : public QObject
{
    Q_OBJECT

public:
    explicit GlobalShortcutManager(ActionManager* actionManager, QObject* parent = nullptr);
    ~GlobalShortcutManager() override;

    [[nodiscard]] GlobalShortcutAvailability availability() const;
    [[nodiscard]] bool configurationAvailable() const;

    void addCommand(Command* command);
    void removeCommand(Command* command);
    void clear();
    void configure();
    void synchronise();

Q_SIGNALS:
    void registrationFailed(const Fooyin::Id& commandId, const QKeySequence& shortcut, const QString& error);

private:
    void activate(const Id& commandId);
    void registrationFailure(const Id& commandId, const QKeySequence& shortcut, const QString& error);
    void scheduleSaveSettings();
    void scheduleSynchronise();

    ActionManager* m_actionManager;
    std::unique_ptr<GlobalShortcutBackend> m_backend;
    std::map<Id, Command*> m_commands;
    bool m_saveScheduled{false};
    bool m_syncScheduled{false};
};
} // namespace GlobalHotkeys
} // namespace Fooyin
