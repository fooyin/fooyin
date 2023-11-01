/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

namespace Fy {

namespace Utils {
class ActionManager;
class Command;
} // namespace Utils

namespace Gui::Settings {
class ShortcutItem : public Utils::TreeStatusItem<ShortcutItem>
{
public:
    enum Role
    {
        IsCategory = Qt::UserRole,
        Command
    };

    ShortcutItem();
    explicit ShortcutItem(QString title, Utils::Command* command, ShortcutItem* parent);

    [[nodiscard]] QString title() const;
    [[nodiscard]] QString shortcut() const;
    [[nodiscard]] Utils::ShortcutList shortcuts() const;

    [[nodiscard]] Utils::Command* command() const;

    [[nodiscard]] bool isCategory() const;

    void updateShortcuts(const Utils::ShortcutList& shortcuts);

    void sortChildren();

private:
    QString m_title;
    QString m_shortcut;
    Utils::ShortcutList m_shortcuts;
    Utils::Command* m_command;
};

class ShortcutsModel : public Utils::TreeModel<ShortcutItem>
{
public:
    explicit ShortcutsModel(QObject* parent = nullptr);

    void populate(Utils::ActionManager* actionManager);
    void shortcutChanged(Utils::Command* command, const Utils::ShortcutList& shortcuts);
    void shortcutDeleted(Utils::Command* command, const QKeySequence& shortcut);
    void processQueue();

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent) const override;

private:
    std::map<Utils::Id, ShortcutItem> m_nodes;
};
} // namespace Gui::Settings
} // namespace Fy
