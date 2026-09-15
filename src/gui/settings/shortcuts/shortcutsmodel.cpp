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

#include "shortcutsmodel.h"

#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>

#include <QBrush>
#include <QPointer>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
ShortcutBindingList removeEmptyBindings(const ShortcutBindingList& bindings)
{
    return Utils::filter(bindings, [](const Fooyin::ShortcutBinding& binding) { return !binding.shortcut.isEmpty(); });
}

QString bindingsToString(const ShortcutBindingList& bindings, bool globalShortcutRegistered)
{
    QStringList keys;

    std::ranges::transform(removeEmptyBindings(bindings), std::back_inserter(keys), [](const auto& binding) {
        QString text = binding.shortcut.toString(QKeySequence::NativeText);
        if(binding.scope == ShortcutScope::Global) {
            text += u" ("_s + ShortcutsModel::tr("Global") + u')';
        }
        return text;
    });

    if(globalShortcutRegistered) {
        keys.append(ShortcutsModel::tr("Global"));
    }

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

bool isModifierOnly(Qt::Key key)
{
    return key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Alt || key == Qt::Key_Meta
        || key == Qt::Key_AltGr;
}

bool isUnmodifiedPrintable(const QKeyCombination& combination)
{
    const auto modifiers = combination.keyboardModifiers();
    if(modifiers.testAnyFlags(Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
        return false;
    }

    const int key = combination.key();
    return key >= 0x20 && key < Qt::Key_Escape;
}
} // namespace

ShortcutItem::ShortcutItem()
    : ShortcutItem{{}, nullptr, nullptr}
{ }

ShortcutItem::ShortcutItem(QString title, Command* command, ShortcutItem* parent, bool systemManaged)
    : TreeStatusItem{parent}
    , m_title{std::move(title)}
    , m_command{command}
{
    if(command) {
        m_globalShortcutRegistered = systemManaged && command->isGlobalShortcutRegistered();

        const auto shortcuts = command->shortcuts();
        for(const auto& shortcut : shortcuts) {
            m_bindings.emplace_back(shortcut, ShortcutScope::Application);
        }

        if(!systemManaged) {
            const auto globalShortcuts = command->globalShortcuts();
            for(const auto& shortcut : globalShortcuts) {
                m_bindings.emplace_back(shortcut, ShortcutScope::Global);
            }
        }

        m_shortcut = bindingsToString(m_bindings, m_globalShortcutRegistered);
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

ShortcutBindingList ShortcutItem::bindings() const
{
    return m_bindings;
}

bool ShortcutItem::isGlobalShortcutRegistered() const
{
    return m_globalShortcutRegistered;
}

Command* ShortcutItem::command() const
{
    return m_command;
}

bool ShortcutItem::isCategory() const
{
    return !m_command;
}

void ShortcutItem::updateBindings(const ShortcutBindingList& bindings)
{
    m_bindings = bindings;
    m_shortcut = bindingsToString(m_bindings, m_globalShortcutRegistered);
}

void ShortcutItem::updateGlobalShortcutRegistration(bool registered)
{
    m_globalShortcutRegistered = registered;
    m_shortcut                 = bindingsToString(m_bindings, m_globalShortcutRegistered);
}

ShortcutsModel::ShortcutsModel(QObject* parent)
    : TreeModel{parent}
{ }

void ShortcutsModel::populate(ActionManager* actionManager)
{
    beginResetModel();

    m_systemManaged = actionManager->globalShortcutManagement() == GlobalShortcutManagement::SystemManaged;

    for(const auto& connection : m_commandConnections) {
        QObject::disconnect(connection);
    }
    m_commandConnections.clear();

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
            = &m_nodes.emplace(command->id(), ShortcutItem{command->description(), command, parent, m_systemManaged})
                   .first->second;
        parent->appendChild(child);

        const QPointer commandGuard{command};
        const auto synchronise = [this, commandGuard] {
            if(commandGuard) {
                synchroniseBindings(commandGuard);
            }
        };

        m_commandConnections.push_back(
            QObject::connect(command, &Command::shortcutChanged, this, synchronise, Qt::QueuedConnection));
        m_commandConnections.push_back(
            QObject::connect(command, &Command::globalShortcutsChanged, this, synchronise, Qt::QueuedConnection));
        m_commandConnections.push_back(QObject::connect(command, &Command::globalShortcutRegistrationChanged, this,
                                                        synchronise, Qt::QueuedConnection));
    }

    rootItem()->sortChildren();
    rebuildConflicts();

    endResetModel();
}

void ShortcutsModel::bindingsChanged(Command* command, const ShortcutBindingList& bindings)
{
    auto* item = itemForCommand(command);
    if(!item) {
        return;
    }

    IdSet changedIds{command->id()};
    const auto previousConflicts{m_conflicts};

    if(item->bindings() != bindings) {
        item->updateBindings(bindings);
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

void ShortcutsModel::globalShortcutRegistrationChanged(Command* command, bool registered)
{
    auto* item = itemForCommand(command);
    if(!item || item->isGlobalShortcutRegistered() == registered) {
        return;
    }

    item->updateGlobalShortcutRegistration(registered);
    item->setStatus(ShortcutItem::Changed);
    emitDataChanged({command->id()});
}

void ShortcutsModel::processQueue()
{
    IdSet changedIds;

    for(auto& shortcut : m_nodes | std::views::values) {
        auto* command = shortcut.command();

        switch(shortcut.status()) {
            case ShortcutItem::Changed: {
                changedIds.emplace(command->id());

                ShortcutList appShortcuts;
                ShortcutList globalShortcuts;

                for(const auto& binding : shortcut.bindings()) {
                    auto& destination = binding.scope == ShortcutScope::Global ? globalShortcuts : appShortcuts;
                    destination.append(binding.shortcut);
                }

                command->setShortcut(appShortcuts);
                if(m_systemManaged) {
                    command->setGlobalShortcutRegistered(shortcut.isGlobalShortcutRegistered());
                }
                else {
                    command->setGlobalShortcuts(globalShortcuts);
                    command->setGlobalShortcutRegistered(!globalShortcuts.empty());
                }
                break;
            }
            case ShortcutItem::Added:
            case ShortcutItem::Removed:
            case ShortcutItem::None:
                break;
        }
        shortcut.setStatus(ShortcutItem::None);
    }

    rebuildConflicts();
    emitDataChanged(changedIds);
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

        ShortcutBindingList bindings = item->bindings();
        std::erase_if(bindings, [&conflict](const auto& binding) { return binding.shortcut == conflict.shortcut; });
        item->updateBindings(bindings);
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

ShortcutBindingList ShortcutsModel::bindings(Command* command) const
{
    if(const auto* item = itemForCommand(command)) {
        return item->bindings();
    }

    if(!command) {
        return {};
    }

    ShortcutBindingList result;

    const auto shortcuts = command->shortcuts();
    for(const auto& shortcut : shortcuts) {
        result.emplace_back(shortcut, ShortcutScope::Application);
    }
    if(!m_systemManaged) {
        const auto globalShortcuts = command->globalShortcuts();
        for(const auto& shortcut : globalShortcuts) {
            result.emplace_back(shortcut, ShortcutScope::Global);
        }
    }

    return result;
}

bool ShortcutsModel::isGlobalShortcutRegistered(Command* command) const
{
    if(const auto* item = itemForCommand(command)) {
        return item->isGlobalShortcutRegistered();
    }
    return command && command->isGlobalShortcutRegistered();
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
    for(const auto& item : m_nodes | std::views::values) {
        if(!item.command()) {
            continue;
        }

        const auto bindings = item.bindings();
        if(std::ranges::count(bindings, ShortcutScope::Global, &ShortcutBinding::scope) > 1) {
            return tr("Only one global shortcut can be assigned to an action.")
                 + u"\n\n%1"_s.arg(item.command()->description());
        }

        for(const auto& binding : bindings) {
            if(binding.scope != ShortcutScope::Global) {
                continue;
            }

            if(!item.command()->actionForContext(Constants::Context::Global)) {
                return tr("%1 is not available as a global action.").arg(item.command()->description());
            }
            if(binding.shortcut.count() != 1) {
                return tr("Global shortcuts must contain exactly one key combination.")
                     + u"\n\n%1: %2"_s.arg(item.command()->description(),
                                           binding.shortcut.toString(QKeySequence::NativeText));
            }

            const QKeyCombination combination = binding.shortcut[0];
            if(isModifierOnly(combination.key()) || isUnmodifiedPrintable(combination)) {
                return tr("Global shortcuts using printable keys must include Ctrl, Alt, or Meta.")
                     + u"\n\n%1: %2"_s.arg(item.command()->description(),
                                           binding.shortcut.toString(QKeySequence::NativeText));
            }
        }
    }

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

void ShortcutsModel::synchroniseBindings(Command* command)
{
    auto* item = itemForCommand(command);
    if(!item || item->status() != ShortcutItem::None) {
        return;
    }

    ShortcutBindingList bindings;
    for(const auto& shortcut : command->shortcuts()) {
        bindings.emplace_back(shortcut, ShortcutScope::Application);
    }
    if(!m_systemManaged) {
        for(const auto& shortcut : command->globalShortcuts()) {
            bindings.emplace_back(shortcut, ShortcutScope::Global);
        }
    }

    const bool globalShortcutRegistered = m_systemManaged && command->isGlobalShortcutRegistered();
    if(item->bindings() == bindings && item->isGlobalShortcutRegistered() == globalShortcutRegistered) {
        return;
    }

    const auto previousConflicts = m_conflicts;
    item->updateBindings(bindings);
    item->updateGlobalShortcutRegistration(globalShortcutRegistered);
    rebuildConflicts();

    IdSet changedIds{command->id()};
    for(const auto& id : previousConflicts | std::views::keys) {
        changedIds.emplace(id);
    }
    for(const auto& id : m_conflicts | std::views::keys) {
        changedIds.emplace(id);
    }

    emitDataChanged(changedIds);
    Q_EMIT bindingsSynchronised(command);
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
        for(const auto& binding : removeEmptyBindings(item.bindings())) {
            const QString key = shortcutKey(binding.shortcut);
            if(key.isEmpty() || !uniqueShortcuts.emplace(key).second) {
                continue;
            }

            commandsByShortcut[key].push_back(id);
            shortcutsByKey.try_emplace(key, binding.shortcut);
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
        Q_EMIT dataChanged(index, index.siblingAtColumn(columnCount({}) - 1));
    }
}
} // namespace Fooyin
