/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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

#include <QString>

namespace Fooyin {
class ActionManager;
class PlayerController;
class PropertiesDialog;

class ScriptCommandHandler
{
public:
    ScriptCommandHandler(ActionManager* actionManager, PlayerController* playerController,
                         PropertiesDialog* propertiesDialog);

    bool execute(const QString& commandId) const;

private:
    ActionManager* m_actionManager;
    PlayerController* m_playerController;
    PropertiesDialog* m_propertiesDialog;
};
} // namespace Fooyin
