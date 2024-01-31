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

using namespace Qt::Literals::StringLiterals;

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
    return keys.join("|"_L1);
}

bool sortShortcutItems(const Fooyin::ShortcutItem* lhs, const Fooyin::ShortcutItem* rhs)
{
    if(lhs->parent()->parent() && rhs->parent()->parent()) {
        if(lhs->childCount() != rhs->childCount()) {
            return lhs->childCount() < rhs->childCount();
        }
    }
    const auto cmp = QString::localeAwareCompare(lhs->title(), rhs->title());
    if(cmp == 0) {
        return false;
    }
    return cmp < 0;
}
} // namespace

namespace Fooyin {
ShortcutItem::ShortcutItem()
    : ShortcutItem{QStringLiteral(""), nullptr, nullptr}
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

void ShortcutItem::sortChildren()
{
    std::vector<ShortcutItem*> sortedChildren{m_children};
    std::ranges::sort(sortedChildren, sortShortcutItems);
    m_children = sortedChildren;
    std::ranges::for_each(m_children, &ShortcutItem::sortChildren);
}

ShortcutsModel::ShortcutsModel(QObject* parent)
    : TreeModel{parent}
{ }

void ShortcutsModel::populate(ActionManager* actionManager)
{
    beginResetModel();

    resetRoot();
    m_nodes.clear();

    std::map<QString, ShortcutItem*> sections;

    const auto commands = actionManager->commands();

    for(Command* command : commands) {
        if(command->action() && command->action()->isSeparator()) {
            continue;
        }

        const auto parts      = command->id().name().split('.');
        const auto categories = parts | std::views::take(parts.size() - 1);

        ShortcutItem* parent = rootItem();

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

    endResetModel();
}

void ShortcutsModel::shortcutChanged(Command* command, const ShortcutList& shortcuts)
{
    if(!command) {
        return;
    }

    if(!m_nodes.contains(command->id())) {
        return;
    }

    auto* item = &m_nodes.at(command->id());

    if(item->shortcuts() != shortcuts) {
        item->updateShortcuts(shortcuts);
        item->setStatus(ShortcutItem::Changed);

        emit dataChanged({}, {});
    }
}

void ShortcutsModel::shortcutDeleted(Command* command, const QKeySequence& shortcut)
{
    if(!command) {
        return;
    }

    if(!m_nodes.contains(command->id())) {
        return;
    }

    auto* item = &m_nodes.at(command->id());

    ShortcutList currentShortcuts = item->shortcuts();

    if(!currentShortcuts.contains(shortcut)) {
        return;
    }

    currentShortcuts.removeAll(shortcut);

    item->updateShortcuts(currentShortcuts);
    item->setStatus(ShortcutItem::Changed);

    emit dataChanged({}, {});
}

void ShortcutsModel::processQueue()
{
    for(auto& [id, shortcut] : m_nodes) {
        auto* command = shortcut.command();

        switch(shortcut.status()) {
            case(ShortcutItem::Changed): {
                command->setShortcut(shortcut.shortcuts());
                emit dataChanged({}, {});
                break;
            }
            case(ShortcutItem::Added):
            case(ShortcutItem::Removed):
            case(ShortcutItem::None):
                break;
        }
        shortcut.setStatus(ShortcutItem::None);
    }
}

QVariant ShortcutsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::TextAlignmentRole) {
        return (Qt::AlignHCenter);
    }

    if(role != Qt::DisplayRole || orientation == Qt::Orientation::Vertical) {
        return {};
    }

    switch(section) {
        case(0):
            return "Action";
        case(1):
            return "Id";
        case(2):
            return "Shortcut";
    }
    return {};
}

QVariant ShortcutsModel::data(const QModelIndex& index, int role) const
{
    if(role != Qt::DisplayRole && role != Qt::FontRole && role != ShortcutItem::IsCategory
       && role != ShortcutItem::ActionCommand) {
        return {};
    }

    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    auto* item = static_cast<ShortcutItem*>(index.internalPointer());

    if(role == Qt::FontRole) {
        return item->font();
    }

    if(role == ShortcutItem::IsCategory) {
        return item->isCategory();
    }

    if(role == ShortcutItem::ActionCommand) {
        return QVariant::fromValue(item->command());
    }

    switch(index.column()) {
        case(0): {
            return !item->isCategory() ? item->command()->description() : item->title();
        }
        case(1): {
            return item->isCategory() ? item->title() : item->command()->id().name();
        }
        case(2): {
            if(item->isCategory()) {
                return {};
            }
            return item->shortcut();
        }
        default:
            return {};
    }
}

int ShortcutsModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 3;
}
} // namespace Fooyin
