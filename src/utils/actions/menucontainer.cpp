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

namespace Fooyin {
MenuBarContainer::MenuBarContainer(const Id& id, ActionManager* manager)
    : ActionContainer{id, manager}
    , m_menuBar{nullptr}
{ }

QMenu* MenuBarContainer::menu() const
{
    return nullptr;
}

void MenuBarContainer::setMenuBar(QMenuBar* menuBar)
{
    m_menuBar = menuBar;
}

QMenuBar* MenuBarContainer::menuBar() const
{
    return m_menuBar;
}

QAction* MenuBarContainer::containerAction() const
{
    return nullptr;
}

void MenuBarContainer::insertAction(QAction* beforeAction, QAction* action)
{
    m_menuBar->insertAction(beforeAction, action);
}

void MenuBarContainer::insertAction(QAction* beforeAction, Command* action)
{
    insertAction(beforeAction, action->action());
}

void MenuBarContainer::insertMenu(QAction* beforeAction, ActionContainer* container)
{
    if(QMenu* currentMenu = container->menu()) {
        m_menuBar->insertMenu(beforeAction, currentMenu);
    }
}

bool MenuBarContainer::isEmpty()
{
    return false;
}

bool MenuBarContainer::isHidden()
{
    return m_menuBar->isHidden();
}

void MenuBarContainer::clear()
{
    m_menuBar->clear();
}

bool MenuBarContainer::update()
{
    return false;
}

bool MenuBarContainer::canBeAddedToContainer(ActionContainer* /*container*/) const
{
    return false;
}

MenuContainer::MenuContainer(const Id& id, ActionManager* manager)
    : ActionContainer{id, manager}
    , m_menu{std::make_unique<QMenu>()}
{ }

MenuContainer::~MenuContainer() = default;

QMenu* MenuContainer::menu() const
{
    return m_menu.get();
}

QAction* MenuContainer::containerAction() const
{
    return m_menu->menuAction();
}

void MenuContainer::insertAction(QAction* beforeAction, QAction* action)
{
    m_menu->insertAction(beforeAction, action);
}

void MenuContainer::insertAction(QAction* beforeAction, Command* action)
{
    insertAction(beforeAction, action->action());
}

void MenuContainer::insertMenu(QAction* beforeAction, ActionContainer* container)
{
    if(QMenu* currentMenu = container->menu()) {
        m_menu->insertMenu(beforeAction, currentMenu);
    }
}

bool MenuContainer::isEmpty()
{
    return m_menu->isEmpty();
}

bool MenuContainer::isHidden()
{
    return m_menu->isHidden();
}

void MenuContainer::clear()
{
    m_menu->clear();
}

bool MenuContainer::update()
{
    if(disabledBehavior() == Show) {
        return true;
    }

    QList<QAction*> actions = m_menu->actions();

    const auto groupHasActions = [this, &actions](const auto& group) -> bool {
        return std::ranges::any_of(std::as_const(group.items), [&](QObject* item) {
            if(auto* container = qobject_cast<ActionContainer*>(item)) {
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

    bool hasActions = std::ranges::any_of(std::as_const(actionGroups()),
                                          [&](const Group& group) { return groupHasActions(group); });

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

bool MenuContainer::canBeAddedToContainer(ActionContainer* container) const
{
    return qobject_cast<MenuContainer*>(container) || qobject_cast<MenuBarContainer*>(container);
}
} // namespace Fooyin

#include "moc_menucontainer.cpp"
