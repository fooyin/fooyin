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

#include "actioncontainer.h"

#include "core/constants.h"

#include <QAction>
#include <QMenu>
#include <QMenuBar>

namespace Core {
ActionContainer::ActionContainer(const Utils::Id& id, QObject* parent)
    : QObject{parent}
    , m_id{id}
{ }

Utils::Id ActionContainer::id() const
{
    return m_id;
}

QMenu* ActionContainer::menu() const
{
    return nullptr;
}

QMenuBar* ActionContainer::menuBar() const
{
    return nullptr;
}

QAction* ActionContainer::insertLocation(const Utils::Id& group) const
{
    auto it = findGroup(group);
    if(it != m_groups.cend()) {
        return nullptr;
    }
    return insertLocation(it);
}

QAction* ActionContainer::actionForItem(QObject* item) const
{
    if(auto* actContainer = qobject_cast<ActionContainer*>(item)) {
        if(auto* container = actContainer->containerAction()) {
            return container;
        }
    }
    return nullptr;
}

void ActionContainer::appendGroup(const Utils::Id& group)
{
    m_groups.append(Group{group});
}

void ActionContainer::insertGroup(const Utils::Id& beforeGroup, const Utils::Id& group)
{
    auto it = std::find_if(m_groups.cbegin(), m_groups.cend(), [&](const auto& group) {
        return (group.id == beforeGroup);
    });
    if(it != m_groups.cend()) {
        m_groups.insert(it, Group{group});
    }
}

void ActionContainer::addAction(QAction* action, const Utils::Id& group)
{
    if(!action) {
        return;
    }

    const Utils::Id actualGroupId = group.isValid() ? group : Utils::Id(Core::Constants::Groups::One);

    const QList<Group>::const_iterator groupIt = findGroup(actualGroupId);
    if(groupIt == m_groups.constEnd()) {
        qDebug() << "Can't find group" << group.name() << "in container" << id().name();
        return;
    }
    m_groups[groupIt - m_groups.constBegin()].items.append(action);
    connect(action, &QObject::destroyed, this, &ActionContainer::itemDestroyed);

    QAction* beforeAction = insertLocation(groupIt);
    insertAction(beforeAction, action);
}

void ActionContainer::addMenu(ActionContainer* menu, const Utils::Id& group)
{
    auto* container = static_cast<ActionContainer*>(menu);
    if(!container->canBeAddedToContainer(this)) {
        return;
    }

    const Utils::Id groupId = group.isValid() ? group : Utils::Id(Core::Constants::Groups::One);

    auto groupIt = findGroup(groupId);
    if(groupIt == m_groups.cend()) {
        return;
    }
    m_groups[groupIt - m_groups.cbegin()].items.append(menu);
    connect(menu, &QObject::destroyed, this, &ActionContainer::itemDestroyed);

    QAction* beforeAction = insertLocation(groupIt);
    insertMenu(beforeAction, menu);
}

void ActionContainer::itemDestroyed(QObject* sender)
{
    for(Group& group : m_groups) {
        if(group.items.removeAll(sender) > 0) {
            break;
        }
    }
}

void ActionContainer::addMenu(ActionContainer* beforeContainer, ActionContainer* menu)
{
    auto* container = static_cast<ActionContainer*>(menu);
    if(container->canBeAddedToContainer(this)) {
        return;
    }

    for(Group& group : m_groups) {
        const int insertionPoint = static_cast<int>(group.items.indexOf(beforeContainer));
        if(insertionPoint >= 0) {
            group.items.insert(insertionPoint, menu);
            break;
        }
    }
    connect(menu, &QObject::destroyed, this, &ActionContainer::itemDestroyed);

    QAction* beforeAction = beforeContainer->containerAction();
    if(beforeAction) {
        insertMenu(beforeAction, menu);
    }
}

QAction* ActionContainer::addSeparator(const Utils::Id& group)
{
    return addSeparator(group, {});
}

QAction* ActionContainer::addSeparator(const Utils::Id& group, QAction** outSeparator)
{
    static int separatorIdCount = 0;

    auto* separator = new QAction(this);
    separator->setSeparator(true);
    const Utils::Id sepId = id().append(".Separator.").append(++separatorIdCount);
    emit registerSeperator(separator, sepId);
    addAction(separator, group);
    if(outSeparator) {
        *outSeparator = separator;
    }
    return separator;
}

QList<ActionContainer::Group>::const_iterator ActionContainer::findGroup(const Utils::Id& groupId) const
{
    return std::find_if(m_groups.cbegin(), m_groups.cend(), [&](const auto& group) {
        return (group.id == groupId);
    });
}

QAction* ActionContainer::insertLocation(QList<Group>::const_iterator group) const
{
    if(group == m_groups.cend()) {
        return nullptr;
    }
    ++group;
    while(group != m_groups.cend()) {
        if(!group->items.isEmpty()) {
            QObject* item   = group->items.first();
            QAction* action = actionForItem(item);
            if(action) {
                return action;
            }
        }
        ++group;
    }
    return nullptr;
}

MenuBarActionContainer::MenuBarActionContainer(const Utils::Id& id, QObject* parent)
    : ActionContainer{id, parent}
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

void MenuBarActionContainer::insertMenu(QAction* beforeAction, ActionContainer* container)
{
    QMenu* menu = container->menu();
    if(!menu) {
        return;
    }
    m_menuBar->insertMenu(beforeAction, menu);
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

bool MenuBarActionContainer::canBeAddedToContainer(ActionContainer* container) const
{
    Q_UNUSED(container)
    return false;
}

MenuActionContainer::MenuActionContainer(const Utils::Id& id, QObject* parent)
    : ActionContainer{id, parent}
    , m_menu{new QMenu()}
{
    connect(m_menu, &QMenu::aboutToHide, this, &MenuActionContainer::aboutToHide);
}

MenuActionContainer::~MenuActionContainer()
{
    delete m_menu;
}

QMenu* MenuActionContainer::menu() const
{
    return m_menu;
}

QAction* MenuActionContainer::containerAction() const
{
    return m_menu->menuAction();
}

void MenuActionContainer::insertAction(QAction* beforeAction, QAction* action)
{
    m_menu->insertAction(beforeAction, action);
}

void MenuActionContainer::insertMenu(QAction* beforeAction, ActionContainer* container)
{
    QMenu* menu = container->menu();
    if(!menu) {
        return;
    }
    m_menu->insertMenu(beforeAction, menu);
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

bool MenuActionContainer::canBeAddedToContainer(ActionContainer* container) const
{
    return qobject_cast<MenuActionContainer*>(container) || qobject_cast<MenuBarActionContainer*>(container);
}
} // namespace Core
