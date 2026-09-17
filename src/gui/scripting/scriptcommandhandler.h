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

#include <gui/trackselectioncontroller.h>

#include <QString>
#include <QStringView>
#include <optional>

#include <vector>

class QAction;

namespace Fooyin {
class ActionManager;
class CurrentPlaylistController;
class PlayerController;
class PropertiesDialog;

enum class ScriptCommandTarget : uint8_t
{
    FollowActiveContext = 0,
    NowPlaying,
    CurrentPlaylist,
    CurrentPlaylistSelection,
    ActiveSelection,
};

enum class CommandSelectionScope : uint8_t
{
    ContextDefault = 0,
    WholeContext,
    Selection,
};

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

struct CommandInvocation
{
    QAction* action{nullptr};
    ScriptCommandTarget target{ScriptCommandTarget::FollowActiveContext};
    std::optional<TrackSelectionTarget> selectionTarget;
    CommandSelectionScope selectionScope{CommandSelectionScope::ContextDefault};
};

class ScriptCommandHandler
{
public:
    ScriptCommandHandler(ActionManager* actionManager, PlayerController* playerController,
                         PropertiesDialog* propertiesDialog, TrackSelectionController* selectionController,
                         CurrentPlaylistController* currentPlaylistController);

    static const ScriptCommandAliasList& scriptCommandAliases();
    static std::optional<ResolvedScriptCommand> resolveCommand(const QString& commandId);
    static const CommandInvocation* currentInvocation(const QAction* action = nullptr);

    [[nodiscard]] bool canExecute(const QString& commandId,
                                  ScriptCommandTarget target = ScriptCommandTarget::FollowActiveContext) const;
    bool execute(const QString& commandId, ScriptCommandTarget target = ScriptCommandTarget::FollowActiveContext) const;

private:
    ActionManager* m_actionManager;
    PlayerController* m_playerController;
    PropertiesDialog* m_propertiesDialog;
    TrackSelectionController* m_selectionController;
    CurrentPlaylistController* m_currentPlaylistController;
};
} // namespace Fooyin
