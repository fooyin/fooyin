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

#include "menucontainer.h"

#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>

#include <QMenuBar>

namespace {
bool canAddAction(Fooyin::Command* action)
{
    return action && action->action();
}
} // namespace

namespace Fooyin {
MenuContainer::MenuContainer(const Id& id, ActionManager* manager)
    : m_manager{manager}
    , m_id{id}
{ }

Id MenuContainer::id() const
{
    return m_id;
}

QMenu* MenuContainer::menu() const
{
    return nullptr;
}

QMenuBar* MenuContainer::menuBar() const
{
    return nullptr;
}

QAction* MenuContainer::insertLocation(const Id& group) const
{
    auto it = findGroup(group);
    if(it != m_groups.cend()) {
        return nullptr;
    }
    return determineInsertionLocation(it);
}

QAction* MenuContainer::actionForItem(QObject* item) const
{
    if(auto* cmd = qobject_cast<Command*>(item)) {
        return cmd->action();
    }

    if(auto* container = qobject_cast<MenuContainer*>(item)) {
        if(container->containerAction()) {
            return container->containerAction();
        }
    }

    return nullptr;
}

ActionContainer::DisabledBehavior MenuContainer::disabledBehavior() const
{
    return m_disabledBehavior;
}

void MenuContainer::setDisabledBehavior(DisabledBehavior behavior)
{
    m_disabledBehavior = behavior;
}

bool MenuContainer::isEmpty()
{
    return true;
}

bool MenuContainer::isHidden()
{
    return false;
}

void MenuContainer::clear() { }

void MenuContainer::itemDestroyed(QObject* sender)
{
    for(Group& group : m_groups) {
        if(std::erase(group.items, sender) > 0) {
            break;
        }
    }
}

MenuContainer::GroupList::const_iterator MenuContainer::findGroup(const Id& groupId) const
{
    return std::ranges::find_if(std::as_const(m_groups), [&](const auto& group) { return (group.id == groupId); });
}

QAction* MenuContainer::determineInsertionLocation(MenuContainer::GroupList::const_iterator group) const
{
    if(group == m_groups.cend()) {
        return nullptr;
    }

    ++group;

    while(group != m_groups.cend()) {
        if(!group->items.empty()) {
            QObject* item = group->items.front();
            if(QAction* action = actionForItem(item)) {
                return action;
            }
        }
        ++group;
    }
    return nullptr;
}

void MenuContainer::appendGroup(const Id& group)
{
    m_groups.emplace_back(group);
}

void MenuContainer::insertGroup(const Id& beforeGroup, const Id& newGroup)
{
    auto groupIt = std::ranges::find_if(m_groups, [&](const auto& group) { return (group.id == beforeGroup); });
    if(groupIt != m_groups.cend()) {
        m_groups.insert(groupIt, Group{newGroup});
    }
}

void MenuContainer::addAction(QAction* action, const Id& group)
{
    if(!action) {
        return;
    }

    const Id groupId = group.isValid() ? group : Id{Actions::Groups::Two};

    const auto groupIt = findGroup(groupId);
    if(groupIt == m_groups.cend()) {
        qDebug() << "Can't find group" << group.name() << "in container" << id().name();
        return;
    }

    m_groups.at(groupIt - m_groups.cbegin()).items.push_back(action);
    QObject::connect(action, &QObject::destroyed, this, &MenuContainer::itemDestroyed);

    QAction* beforeAction = determineInsertionLocation(groupIt);
    insertAction(beforeAction, action);
}

void MenuContainer::addAction(Command* action, const Id& group)
{
    if(!canAddAction(action)) {
        return;
    }

    const Id groupId = group.isValid() ? group : Id{Actions::Groups::Two};

    const auto groupIt = findGroup(groupId);
    if(groupIt == m_groups.cend()) {
        qDebug() << "Can't find group" << group.name() << "in container" << id().name();
        return;
    }
    m_groups.at(groupIt - m_groups.cbegin()).items.push_back(action);
    QObject::connect(action, &Command::activeStateChanged, this, [this]() { emit requestUpdate(this); });
    QObject::connect(action, &QObject::destroyed, this, &MenuContainer::itemDestroyed);

    QAction* beforeAction = determineInsertionLocation(groupIt);
    insertAction(beforeAction, action);
}

void MenuContainer::addMenu(ActionContainer* menu, const Id& group)
{
    auto* actionMenu = qobject_cast<MenuContainer*>(menu);
    if(actionMenu && !actionMenu->canBeAddedToContainer(this)) {
        return;
    }

    const Id groupId = group.isValid() ? group : Id{Actions::Groups::Two};

    auto groupIt = findGroup(groupId);
    if(groupIt == m_groups.cend()) {
        return;
    }
    m_groups[groupIt - m_groups.cbegin()].items.push_back(menu);
    QObject::connect(menu, &QObject::destroyed, this, &MenuContainer::itemDestroyed);

    QAction* beforeAction = determineInsertionLocation(groupIt);
    insertMenu(beforeAction, menu);
}

void MenuContainer::addMenu(ActionContainer* beforeContainer, ActionContainer* menu)
{
    auto* actionMenu = qobject_cast<MenuContainer*>(menu);
    if(actionMenu && !actionMenu->canBeAddedToContainer(this)) {
        return;
    }

    for(Group& group : m_groups) {
        auto insertionPoint = std::ranges::find(std::as_const(group.items), beforeContainer);
        if(insertionPoint != group.items.cend()) {
            group.items.insert(insertionPoint, menu);
            break;
        }
    }
    QObject::connect(menu, &QObject::destroyed, this, &MenuContainer::itemDestroyed);

    auto* beforeMenu      = qobject_cast<MenuContainer*>(beforeContainer);
    QAction* beforeAction = beforeMenu->containerAction();
    if(beforeAction) {
        insertMenu(beforeAction, menu);
    }
}

Command* MenuContainer::addSeparator(const Context& context, const Id& group)
{
    static int separatorCount{0};

    auto* separator = new QAction(this);
    separator->setSeparator(true);

    const Id separatorId = id().append(".Separator.").append(++separatorCount);
    Command* command     = m_manager->registerAction(separator, separatorId, context);
    addAction(command, group);
    return command;
}

Command* MenuContainer::addSeparator(const Id& group)
{
    static const Context context{Constants::Context::Global};
    return addSeparator(context, group);
}

MenuBarActionContainer::MenuBarActionContainer(const Id& id, ActionManager* manager)
    : MenuContainer{id, manager}
    , m_menuBar{nullptr}
{ }

QMenu* MenuBarActionContainer::menu() const
{
    return nullptr;
}

void MenuBarActionContainer::setMenuBar(QMenuBar* menuBar)
{
    m_menuBar = menuBar;
}

QMenuBar* MenuBarActionContainer::menuBar() const
{
    return m_menuBar;
}

QAction* MenuBarActionContainer::containerAction() const
{
    return nullptr;
}

void MenuBarActionContainer::insertAction(QAction* beforeAction, QAction* action)
{
    m_menuBar->insertAction(beforeAction, action);
}

void MenuBarActionContainer::insertAction(QAction* beforeAction, Command* action)
{
    insertAction(beforeAction, action->action());
}

void MenuBarActionContainer::insertMenu(QAction* beforeAction, ActionContainer* container)
{
    if(QMenu* currentMenu = container->menu()) {
        m_menuBar->insertMenu(beforeAction, currentMenu);
    }
}

bool MenuBarActionContainer::isEmpty()
{
    return false;
}

bool MenuBarActionContainer::isHidden()
{
    return m_menuBar->isHidden();
}

void MenuBarActionContainer::clear()
{
    m_menuBar->clear();
}

bool MenuBarActionContainer::update()
{
    return false;
}

bool MenuBarActionContainer::canBeAddedToContainer(ActionContainer* /*container*/) const
{
    return false;
}

MenuActionContainer::MenuActionContainer(const Id& id, ActionManager* manager)
    : MenuContainer{id, manager}
    , m_menu{std::make_unique<QMenu>()}
{ }

MenuActionContainer::~MenuActionContainer() = default;

QMenu* MenuActionContainer::menu() const
{
    return m_menu.get();
}

QAction* MenuActionContainer::containerAction() const
{
    return m_menu->menuAction();
}

void MenuActionContainer::insertAction(QAction* beforeAction, QAction* action)
{
    m_menu->insertAction(beforeAction, action);
}

void MenuActionContainer::insertAction(QAction* beforeAction, Command* action)
{
    insertAction(beforeAction, action->action());
}

void MenuActionContainer::insertMenu(QAction* beforeAction, ActionContainer* container)
{
    if(QMenu* currentMenu = container->menu()) {
        m_menu->insertMenu(beforeAction, currentMenu);
    }
}

bool MenuActionContainer::isEmpty()
{
    return m_menu->isEmpty();
}

bool MenuActionContainer::isHidden()
{
    return m_menu->isHidden();
}

void MenuActionContainer::clear()
{
    m_menu->clear();
}

bool MenuActionContainer::update()
{
    if(disabledBehavior() == Show) {
        return true;
    }

    QList<QAction*> actions = m_menu->actions();

    const auto groupHasActions = [this, &actions](const Group& group) -> bool {
        return std::ranges::any_of(std::as_const(group.items), [&](QObject* item) {
            if(auto* container = qobject_cast<MenuContainer*>(item)) {
                actions.removeAll(container->menu()->menuAction());
                if(container == this) {
                    qWarning() << "Container contains itself as a subcontainer";
                    return false;
                }
                return container->update();
            }
            if(auto* command = qobject_cast<Command*>(item)) {
                actions.removeAll(command->action());
                return command->isActive();
            }
            return false;
        });
    };

    bool hasActions
        = std::ranges::any_of(std::as_const(m_groups), [&](const Group& group) { return groupHasActions(group); });

    if(!hasActions) {
        hasActions = std::ranges::any_of(std::as_const(actions),
                                         [](QAction* action) { return !action->isSeparator() && action->isEnabled(); });
    }

    if(disabledBehavior() == Hide) {
        m_menu->menuAction()->setVisible(hasActions);
    }
    else if(disabledBehavior() == Disable) {
        m_menu->menuAction()->setEnabled(hasActions);
    }

    return hasActions;
}

bool MenuActionContainer::canBeAddedToContainer(ActionContainer* container) const
{
    return qobject_cast<MenuActionContainer*>(container) || qobject_cast<MenuBarActionContainer*>(container);
}
} // namespace Fooyin

#include "moc_menucontainer.cpp"
