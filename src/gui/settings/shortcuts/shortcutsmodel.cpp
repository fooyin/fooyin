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

#include "shortcutsmodel.h"

#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>

using namespace Qt::Literals::StringLiterals;

namespace {
Fy::Utils::ShortcutList removeEmptyKeys(const Fy::Utils::ShortcutList& shortcuts)
{
    return Fy::Utils::filter(shortcuts, [](const QKeySequence& shortcut) { return !shortcut.isEmpty(); });
}

QString shortcutsToString(const Fy::Utils::ShortcutList& sequence)
{
    QStringList keys;
    std::ranges::transform(removeEmptyKeys(sequence), std::back_inserter(keys),
                           [](const QKeySequence& k) { return k.toString(QKeySequence::NativeText); });
    return keys.join("|"_L1);
}
} // namespace

namespace Fy::Gui::Settings {
ShortcutItem::ShortcutItem()
    : ShortcutItem{QStringLiteral(""), nullptr, nullptr}
{ }

ShortcutItem::ShortcutItem(QString title, Utils::Command* command, ShortcutItem* parent)
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

Utils::ShortcutList ShortcutItem::shortcuts() const
{
    return m_shortcuts;
}

Utils::Command* ShortcutItem::command() const
{
    return m_command;
}

bool ShortcutItem::isCategory() const
{
    return !m_command;
}

void ShortcutItem::updateShortcuts(const Utils::ShortcutList& shortcuts)
{
    m_shortcuts = shortcuts;
    m_shortcut  = shortcutsToString(m_shortcuts);
}

ShortcutsModel::ShortcutsModel(QObject* parent)
    : TreeModel{parent}
{ }

void ShortcutsModel::populate(Utils::ActionManager* actionManager)
{
    beginResetModel();

    resetRoot();
    m_nodes.clear();

    std::map<QString, ShortcutItem*> sections;

    const auto commands = actionManager->commands();

    for(Utils::Command* command : commands) {
        if(command->action() && command->action()->isSeparator()) {
            continue;
        }

        const QString identifier = command->id().name();
        const int pos            = identifier.indexOf(QLatin1Char('.'));
        const QString section    = identifier.left(pos);

        if(!sections.contains(section)) {
            ShortcutItem* categoryItem
                = &m_nodes.emplace(section, ShortcutItem{section, nullptr, rootItem()}).first->second;
            sections.emplace(section, categoryItem);
            rootItem()->appendChild(categoryItem);
        }

        ShortcutItem* parent = sections.at(section);
        ShortcutItem* child
            = &m_nodes.emplace(command->id(), ShortcutItem{command->description(), command, parent}).first->second;
        parent->appendChild(child);
    }

    endResetModel();
}

void ShortcutsModel::shortcutChanged(Utils::Command* command, const Utils::ShortcutList& shortcuts)
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

void ShortcutsModel::shortcutDeleted(Utils::Command* command, const QKeySequence& shortcut)
{
    if(!command) {
        return;
    }

    if(!m_nodes.contains(command->id())) {
        return;
    }

    auto* item = &m_nodes.at(command->id());

    Utils::ShortcutList currentShortcuts = item->shortcuts();

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
       && role != ShortcutItem::Command) {
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

    if(role == ShortcutItem::Command) {
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
} // namespace Fy::Gui::Settings
