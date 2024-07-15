/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include <utils/actions/actioncontainer.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>

namespace {
bool canAddAction(Fooyin::Command* action)
{
    return action && action->action();
}
} // namespace

namespace Fooyin {
using GroupIterator = ActionContainer::GroupList::const_iterator;

class ActionContainerPrivate : public QObject
{
    Q_OBJECT

public:
    ActionContainerPrivate(ActionContainer* self, const Id& id, ActionManager* manager)
        : m_self{self}
        , m_manager{manager}
        , m_id{id}
    { }

    void itemDestroyed(QObject* sender);

    [[nodiscard]] GroupIterator findGroup(const Id& groupId) const;
    [[nodiscard]] ActionContainer* findContainer(const Id& id) const;
    [[nodiscard]] QAction* determineInsertionLocation(GroupIterator group) const;

    ActionContainer* m_self;
    ActionManager* m_manager;

    Id m_id;
    ActionContainer::GroupList m_groups;
    ActionContainer::DisabledBehavior m_disabledBehavior{ActionContainer::Disable};
};

void ActionContainerPrivate::itemDestroyed(QObject* sender)
{
    for(auto& group : m_groups) {
        if(std::erase(group.items, sender) > 0) {
            break;
        }
    }
}

GroupIterator ActionContainerPrivate::findGroup(const Id& groupId) const
{
    return std::ranges::find_if(std::as_const(m_groups), [&](const auto& group) { return (group.id == groupId); });
}

ActionContainer* ActionContainerPrivate::findContainer(const Id& id) const
{
    return m_manager->actionContainer(id);
}

QAction* ActionContainerPrivate::determineInsertionLocation(GroupIterator group) const
{
    if(group == m_groups.cend()) {
        return nullptr;
    }

    ++group;

    while(group != m_groups.cend()) {
        if(!group->items.empty()) {
            QObject* item = group->items.front();
            if(QAction* action = m_self->actionForItem(item)) {
                return action;
            }
        }
        ++group;
    }
    return nullptr;
}

ActionContainer::ActionContainer(const Id& id, ActionManager* manager)
    : p{std::make_unique<ActionContainerPrivate>(this, id, manager)}
{ }

ActionContainer::~ActionContainer() = default;

Id ActionContainer::id() const
{
    return p->m_id;
}

QMenu* ActionContainer::menu() const
{
    return nullptr;
}

QMenuBar* ActionContainer::menuBar() const
{
    return nullptr;
}

QAction* ActionContainer::insertLocation(const Id& group) const
{
    auto it = p->findGroup(group);
    if(it != actionGroups().cend()) {
        return nullptr;
    }
    return p->determineInsertionLocation(it);
}

QAction* ActionContainer::actionForItem(QObject* item) const
{
    if(auto* cmd = qobject_cast<Command*>(item)) {
        return cmd->action();
    }

    if(auto* container = qobject_cast<ActionContainer*>(item)) {
        if(container->containerAction()) {
            return container->containerAction();
        }
    }

    return nullptr;
}

ActionContainer::DisabledBehavior ActionContainer::disabledBehavior() const
{
    return p->m_disabledBehavior;
}

void ActionContainer::setDisabledBehavior(DisabledBehavior behavior)
{
    p->m_disabledBehavior = behavior;
}

bool ActionContainer::isEmpty()
{
    return true;
}

bool ActionContainer::isHidden()
{
    return false;
}

void ActionContainer::clear() { }

ActionContainer::GroupList& ActionContainer::actionGroups() const
{
    return p->m_groups;
}

void ActionContainer::appendGroup(const Id& group)
{
    actionGroups().emplace_back(group);
}

void ActionContainer::insertGroup(const Id& beforeGroup, const Id& newGroup)
{
    auto& groups = actionGroups();

    auto groupIt = std::ranges::find_if(groups, [&](const auto& group) { return (group.id == beforeGroup); });
    if(groupIt != groups.cend()) {
        groups.insert(groupIt, Group{newGroup});
    }
}

void ActionContainer::addAction(QAction* action, const Id& group)
{
    if(!action) {
        return;
    }

    const Id groupId = group.isValid() ? group : Id{Actions::Groups::Two};

    auto& groups       = actionGroups();
    const auto groupIt = p->findGroup(groupId);
    if(groupIt == groups.cend()) {
        qDebug() << "Can't find group" << group.name() << "in container" << id().name();
        return;
    }

    groups.at(groupIt - groups.cbegin()).items.push_back(action);
    QObject::connect(action, &QObject::destroyed, p.get(), &ActionContainerPrivate::itemDestroyed);

    QAction* beforeAction = p->determineInsertionLocation(groupIt);
    insertAction(beforeAction, action);
}

void ActionContainer::addAction(Command* action, const Id& group)
{
    if(!canAddAction(action)) {
        return;
    }

    const Id groupId = group.isValid() ? group : Id{Actions::Groups::Two};

    auto& groups       = actionGroups();
    const auto groupIt = p->findGroup(groupId);
    if(groupIt == groups.cend()) {
        qDebug() << "Can't find group" << group.name() << "in container" << id().name();
        return;
    }
    groups.at(groupIt - groups.cbegin()).items.push_back(action);
    QObject::connect(action, &Command::activeStateChanged, this, [this]() { emit requestUpdate(this); });
    QObject::connect(action, &QObject::destroyed, p.get(), &ActionContainerPrivate::itemDestroyed);

    QAction* beforeAction = p->determineInsertionLocation(groupIt);
    insertAction(beforeAction, action);
}

void ActionContainer::addMenu(ActionContainer* menu, const Id& group)
{
    auto* actionMenu = qobject_cast<ActionContainer*>(menu);
    if(actionMenu && !actionMenu->canBeAddedToContainer(this)) {
        return;
    }

    const Id groupId = group.isValid() ? group : Id{Actions::Groups::Two};

    auto& groups = actionGroups();
    auto groupIt = p->findGroup(groupId);
    if(groupIt == groups.cend()) {
        return;
    }
    groups[groupIt - groups.cbegin()].items.push_back(menu);
    QObject::connect(menu, &QObject::destroyed, p.get(), &ActionContainerPrivate::itemDestroyed);

    QAction* beforeAction = p->determineInsertionLocation(groupIt);
    insertMenu(beforeAction, menu);
}

void ActionContainer::addMenu(const Id& beforeContainer, ActionContainer* menu)
{
    if(auto* container = p->findContainer(beforeContainer)) {
        addMenu(container, menu);
    }
}

void ActionContainer::addMenu(ActionContainer* beforeContainer, ActionContainer* menu)
{
    auto* actionMenu = qobject_cast<ActionContainer*>(menu);
    if(actionMenu && !actionMenu->canBeAddedToContainer(this)) {
        return;
    }

    auto& groups = actionGroups();
    for(Group& group : groups) {
        auto insertionPoint = std::ranges::find(std::as_const(group.items), beforeContainer);
        if(insertionPoint != group.items.cend()) {
            group.items.insert(insertionPoint, menu);
            break;
        }
    }
    QObject::connect(menu, &QObject::destroyed, p.get(), &ActionContainerPrivate::itemDestroyed);

    auto* beforeMenu      = qobject_cast<ActionContainer*>(beforeContainer);
    QAction* beforeAction = beforeMenu->containerAction();
    if(beforeAction) {
        insertMenu(beforeAction, menu);
    }
}

Command* ActionContainer::addSeparator(const Context& context, const Id& group)
{
    static int separatorCount{0};

    auto* separator = new QAction(this);
    separator->setSeparator(true);

    const Id separatorId = id().append(".Separator.").append(++separatorCount);
    Command* command     = p->m_manager->registerAction(separator, separatorId, context);
    addAction(command, group);
    return command;
}

Command* ActionContainer::addSeparator(const Id& group)
{
    static const Context context{Constants::Context::Global};
    return addSeparator(context, group);
}
} // namespace Fooyin

#include "utils/actions/actioncontainer.moc"
#include "utils/actions/moc_actioncontainer.cpp"
