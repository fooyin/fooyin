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
#include <QStringView>
#include <optional>

#include <vector>

namespace Fooyin {
class ActionManager;
class PlayerController;
class PropertiesDialog;

enum class ScriptCommandAliasType : uint8_t
{
    Action = 0,
    PlayingProperties,
    PlayingFolder,
};

struct ScriptCommandAlias
{
    QStringView alias;
    const char* actionId{nullptr};
    ScriptCommandAliasType type{ScriptCommandAliasType::Action};
    const char* category{nullptr};
    const char* description{nullptr};
};
using ScriptCommandAliasList = std::vector<ScriptCommandAlias>;

struct ResolvedScriptCommand
{
    QString id;
    QString category;
    QString description;
    ScriptCommandAliasType type{ScriptCommandAliasType::Action};
};

class ScriptCommandHandler
{
public:
    ScriptCommandHandler(ActionManager* actionManager, PlayerController* playerController,
                         PropertiesDialog* propertiesDialog);

    static const ScriptCommandAliasList& scriptCommandAliases();
    [[nodiscard]] static std::optional<ResolvedScriptCommand> resolveCommand(const QString& commandId);

    [[nodiscard]] bool canExecute(const QString& commandId) const;
    bool execute(const QString& commandId) const;

private:
    ActionManager* m_actionManager;
    PlayerController* m_playerController;
    PropertiesDialog* m_propertiesDialog;
};
} // namespace Fooyin
