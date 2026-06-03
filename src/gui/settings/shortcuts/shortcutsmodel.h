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

#pragma once

#include <utils/actions/command.h>
#include <utils/id.h>
#include <utils/treemodel.h>
#include <utils/treestatusitem.h>

namespace Fooyin {
class ActionManager;
class Command;

struct ShortcutConflict
{
    QKeySequence shortcut;
    Id otherCommandId;
    QString otherCommandDescription;
};
using ShortcutConflictList = std::vector<ShortcutConflict>;

class ShortcutItem : public TreeStatusItem<ShortcutItem>
{
public:
    enum Role
    {
        IsCategory = Qt::UserRole,
        ActionCommand
    };

    ShortcutItem();
    explicit ShortcutItem(QString title, Command* command, ShortcutItem* parent);

    bool operator<(const ShortcutItem& other) const;

    [[nodiscard]] QString title() const;
    [[nodiscard]] QString shortcut() const;
    [[nodiscard]] ShortcutList shortcuts() const;

    [[nodiscard]] Command* command() const;

    [[nodiscard]] bool isCategory() const;

    void updateShortcuts(const ShortcutList& shortcuts);

private:
    QString m_title;
    QString m_shortcut;
    ShortcutList m_shortcuts;
    Command* m_command;
};

class ShortcutsModel : public TreeModel<ShortcutItem>
{
    Q_OBJECT

public:
    explicit ShortcutsModel(QObject* parent = nullptr);

    void populate(ActionManager* actionManager);
    void shortcutChanged(Command* command, const ShortcutList& shortcuts);
    void shortcutDeleted(Command* command, const QKeySequence& shortcut);
    void processQueue();
    void reassignConflicts(Command* command);

    [[nodiscard]] ShortcutList shortcuts(Command* command) const;
    [[nodiscard]] ShortcutConflictList conflicts(Command* command) const;
    [[nodiscard]] QString conflictDescription(Command* command) const;
    [[nodiscard]] QString firstConflictError() const;
    [[nodiscard]] bool hasConflicts() const;

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

private:
    using ConflictMap = std::map<Id, ShortcutConflictList>;

    [[nodiscard]] ShortcutItem* itemForCommand(Command* command);
    [[nodiscard]] const ShortcutItem* itemForCommand(Command* command) const;
    void rebuildConflicts();
    void emitDataChanged(const IdSet& ids);

    std::map<Id, ShortcutItem> m_nodes;
    ConflictMap m_conflicts;
};
} // namespace Fooyin
