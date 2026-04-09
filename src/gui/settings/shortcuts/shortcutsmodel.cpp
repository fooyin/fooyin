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

#include "shortcutsmodel.h"

#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>

#include <QBrush>

using namespace Qt::StringLiterals;

namespace {
Fooyin::ShortcutList removeEmptyKeys(const Fooyin::ShortcutList& shortcuts)
{
    return Fooyin::Utils::filter(shortcuts, [](const QKeySequence& shortcut) { return !shortcut.isEmpty(); });
}

QString shortcutsToString(const Fooyin::ShortcutList& sequence)
{
    QStringList keys;
    std::ranges::transform(removeEmptyKeys(sequence), std::back_inserter(keys),
                           [](const QKeySequence& k) { return k.toString(QKeySequence::NativeText); });
    return keys.join(" | "_L1);
}

QString shortcutKey(const QKeySequence& shortcut)
{
    return shortcut.toString(QKeySequence::PortableText);
}

QString conflictText(const Fooyin::ShortcutConflictList& conflicts)
{
    QStringList descriptions;

    for(const auto& conflict : conflicts) {
        descriptions.append(
            //: %1 is the shortcut text. %2 is the action currently using that shortcut.
            Fooyin::ShortcutsModel::tr("%1 is already assigned to %2")
                .arg(conflict.shortcut.toString(QKeySequence::NativeText), conflict.otherCommandDescription));
    }

    descriptions.removeDuplicates();
    return descriptions.join(u'\n');
}

QBrush conflictBrush()
{
    return {QColor{176, 0, 32, 40}};
}
} // namespace

namespace Fooyin {
ShortcutItem::ShortcutItem()
    : ShortcutItem{{}, nullptr, nullptr}
{ }

ShortcutItem::ShortcutItem(QString title, Command* command, ShortcutItem* parent)
    : TreeStatusItem{parent}
    , m_title{std::move(title)}
    , m_command{command}
{
    if(command) {
        m_shortcuts = command->shortcuts();
        m_shortcut  = shortcutsToString(m_shortcuts);
    }
}

bool ShortcutItem::operator<(const ShortcutItem& other) const
{
    if(parent()->parent() && other.parent()->parent()) {
        if(childCount() != other.childCount()) {
            return childCount() < other.childCount();
        }
    }
    const auto cmp = QString::localeAwareCompare(title(), other.title());
    if(cmp == 0) {
        return false;
    }
    return cmp < 0;
}

QString ShortcutItem::title() const
{
    return m_title;
}

QString ShortcutItem::shortcut() const
{
    return m_shortcut;
}

ShortcutList ShortcutItem::shortcuts() const
{
    return m_shortcuts;
}

Command* ShortcutItem::command() const
{
    return m_command;
}

bool ShortcutItem::isCategory() const
{
    return !m_command;
}

void ShortcutItem::updateShortcuts(const ShortcutList& shortcuts)
{
    m_shortcuts = shortcuts;
    m_shortcut  = shortcutsToString(m_shortcuts);
}

ShortcutsModel::ShortcutsModel(QObject* parent)
    : TreeModel{parent}
{ }

void ShortcutsModel::populate(ActionManager* actionManager)
{
    beginResetModel();

    resetRoot();
    m_nodes.clear();
    m_conflicts.clear();

    std::map<QString, ShortcutItem*> sections;

    const auto commands = actionManager->commands();

    for(Command* command : commands) {
        if(command->action() && command->action()->isSeparator()) {
            continue;
        }

        ShortcutItem* parent = rootItem();

        const auto categories = command->categories();
        for(const QString& category : categories) {
            if(!sections.contains(category)) {
                auto* categoryItem
                    = &m_nodes.emplace(category, ShortcutItem{category, nullptr, rootItem()}).first->second;
                sections.emplace(category, categoryItem);
                parent->appendChild(categoryItem);
            }
            parent = sections.at(category);
        }

        ShortcutItem* child
            = &m_nodes.emplace(command->id(), ShortcutItem{command->description(), command, parent}).first->second;
        parent->appendChild(child);
    }

    rootItem()->sortChildren();
    rebuildConflicts();

    endResetModel();
}

void ShortcutsModel::shortcutChanged(Command* command, const ShortcutList& shortcuts)
{
    auto* item = itemForCommand(command);
    if(!item) {
        return;
    }

    IdSet changedIds{command->id()};
    const auto previousConflicts{m_conflicts};

    if(item->shortcuts() != shortcuts) {
        item->updateShortcuts(shortcuts);
        item->setStatus(ShortcutItem::Changed);
    }

    rebuildConflicts();

    for(const auto& id : previousConflicts | std::views::keys) {
        changedIds.emplace(id);
    }
    for(const auto& id : m_conflicts | std::views::keys) {
        changedIds.emplace(id);
    }

    emitDataChanged(changedIds);
}

void ShortcutsModel::shortcutDeleted(Command* command, const QKeySequence& shortcut)
{
    auto* item = itemForCommand(command);
    if(!item) {
        return;
    }

    ShortcutList currentShortcuts = item->shortcuts();
    if(!currentShortcuts.contains(shortcut)) {
        return;
    }

    IdSet changedIds{command->id()};
    const auto previousConflicts{m_conflicts};

    currentShortcuts.removeAll(shortcut);
    item->updateShortcuts(currentShortcuts);
    item->setStatus(ShortcutItem::Changed);

    rebuildConflicts();

    for(const auto& id : previousConflicts | std::views::keys) {
        changedIds.emplace(id);
    }
    for(const auto& id : m_conflicts | std::views::keys) {
        changedIds.emplace(id);
    }

    emitDataChanged(changedIds);
}

void ShortcutsModel::processQueue()
{
    for(auto& [id, shortcut] : m_nodes) {
        auto* command = shortcut.command();

        switch(shortcut.status()) {
            case(ShortcutItem::Changed): {
                command->setShortcut(shortcut.shortcuts());
                break;
            }
            case(ShortcutItem::Added):
            case(ShortcutItem::Removed):
            case(ShortcutItem::None):
                break;
        }
        shortcut.setStatus(ShortcutItem::None);
    }

    rebuildConflicts();
    invalidateData();
}

void ShortcutsModel::reassignConflicts(Command* command)
{
    if(!command || !m_conflicts.contains(command->id())) {
        return;
    }

    IdSet changedIds{command->id()};
    const auto previousConflicts{m_conflicts};
    const auto conflictsForCommand = m_conflicts.at(command->id());

    for(const auto& conflict : conflictsForCommand) {
        auto* item = itemForCommand(m_nodes.at(conflict.otherCommandId).command());
        if(!item) {
            continue;
        }

        ShortcutList shortcuts = item->shortcuts();
        if(!shortcuts.contains(conflict.shortcut)) {
            continue;
        }

        shortcuts.removeAll(conflict.shortcut);
        item->updateShortcuts(shortcuts);
        item->setStatus(ShortcutItem::Changed);
        changedIds.emplace(conflict.otherCommandId);
    }

    rebuildConflicts();

    for(const auto& id : previousConflicts | std::views::keys) {
        changedIds.emplace(id);
    }
    for(const auto& id : m_conflicts | std::views::keys) {
        changedIds.emplace(id);
    }

    emitDataChanged(changedIds);
}

ShortcutList ShortcutsModel::shortcuts(Command* command) const
{
    if(const auto* item = itemForCommand(command)) {
        return item->shortcuts();
    }
    return command ? command->shortcuts() : ShortcutList{};
}

ShortcutConflictList ShortcutsModel::conflicts(Command* command) const
{
    if(!command || !m_conflicts.contains(command->id())) {
        return {};
    }
    return m_conflicts.at(command->id());
}

QString ShortcutsModel::conflictDescription(Command* command) const
{
    return conflictText(conflicts(command));
}

QString ShortcutsModel::firstConflictError() const
{
    if(m_conflicts.empty()) {
        return {};
    }

    const auto& [commandId, conflictsForCommand] = *m_conflicts.begin();
    const auto commandIt                         = m_nodes.find(commandId);
    if(commandIt == m_nodes.end() || conflictsForCommand.empty() || !commandIt->second.command()) {
        return {};
    }

    return tr("Resolve duplicate shortcuts before applying changes.")
         + u"\n\n%1: %2"_s.arg(commandIt->second.command()->description(), conflictText(conflictsForCommand));
}

bool ShortcutsModel::hasConflicts() const
{
    return !m_conflicts.empty();
}

QVariant ShortcutsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return Qt::AlignCenter;
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    switch(section) {
        case 0:
            return tr("Action");
        case 1:
            return tr("Id");
        case 2:
            return tr("Shortcut");
        default:
            break;
    }

    return {};
}

QVariant ShortcutsModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = static_cast<ShortcutItem*>(index.internalPointer());

    switch(role) {
        case Qt::DisplayRole: {
            switch(index.column()) {
                case 0:
                    return !item->isCategory() ? item->command()->description() : item->title();
                case 1:
                    return item->isCategory() ? item->title() : item->command()->id().name();
                case 2:
                    return item->isCategory() ? QVariant{} : QVariant{item->shortcut()};
                default:
                    break;
            }
            break;
        }
        case Qt::FontRole:
            return item->font();
        case Qt::BackgroundRole:
            if(item->command() && m_conflicts.contains(item->command()->id())) {
                return conflictBrush();
            }
            break;
        case Qt::ToolTipRole:
            if(item->command()) {
                return conflictDescription(item->command());
            }
            break;
        case ShortcutItem::IsCategory:
            return item->isCategory();
        case ShortcutItem::ActionCommand:
            return QVariant::fromValue(item->command());
        default:
            break;
    }

    return {};
}

int ShortcutsModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 3;
}

ShortcutItem* ShortcutsModel::itemForCommand(Command* command)
{
    if(!command || !m_nodes.contains(command->id())) {
        return nullptr;
    }
    return &m_nodes.at(command->id());
}

const ShortcutItem* ShortcutsModel::itemForCommand(Command* command) const
{
    if(!command || !m_nodes.contains(command->id())) {
        return nullptr;
    }
    return &m_nodes.at(command->id());
}

void ShortcutsModel::rebuildConflicts()
{
    std::map<QString, std::vector<Id>> commandsByShortcut;
    std::map<QString, QKeySequence> shortcutsByKey;

    for(const auto& [id, item] : m_nodes) {
        if(!item.command()) {
            continue;
        }

        std::set<QString> uniqueShortcuts;
        for(const auto& shortcut : removeEmptyKeys(item.shortcuts())) {
            const QString key = shortcutKey(shortcut);
            if(key.isEmpty() || !uniqueShortcuts.emplace(key).second) {
                continue;
            }

            commandsByShortcut[key].push_back(id);
            shortcutsByKey.try_emplace(key, shortcut);
        }
    }

    ConflictMap conflicts;

    for(const auto& [key, commands] : commandsByShortcut) {
        if(commands.size() < 2) {
            continue;
        }

        for(const Id& commandId : commands) {
            auto& commandConflicts = conflicts[commandId];
            for(const Id& otherCommandId : commands) {
                if(otherCommandId == commandId) {
                    continue;
                }

                const auto otherIt = m_nodes.find(otherCommandId);
                if(otherIt == m_nodes.end() || !otherIt->second.command()) {
                    continue;
                }

                commandConflicts.push_back({.shortcut                = shortcutsByKey.at(key),
                                            .otherCommandId          = otherCommandId,
                                            .otherCommandDescription = otherIt->second.command()->description()});
            }
        }
    }

    for(auto& conflictsForCommand : conflicts | std::views::values) {
        std::ranges::sort(conflictsForCommand, [](const ShortcutConflict& lhs, const ShortcutConflict& rhs) {
            const auto shortcutCmp = QString::localeAwareCompare(lhs.shortcut.toString(QKeySequence::NativeText),
                                                                 rhs.shortcut.toString(QKeySequence::NativeText));
            if(shortcutCmp != 0) {
                return shortcutCmp < 0;
            }
            return QString::localeAwareCompare(lhs.otherCommandDescription, rhs.otherCommandDescription) < 0;
        });
    }

    m_conflicts = std::move(conflicts);
}

void ShortcutsModel::emitDataChanged(const IdSet& ids)
{
    for(const Id& id : ids) {
        const auto it = m_nodes.find(id);
        if(it == m_nodes.end() || !it->second.command()) {
            continue;
        }

        const auto index = indexOfItem(&it->second);
        emit dataChanged(index, index.siblingAtColumn(columnCount({}) - 1));
    }
}
} // namespace Fooyin
