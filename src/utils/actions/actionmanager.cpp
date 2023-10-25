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

#include <utils/actions/actionmanager.h>

#include <utils/actions/actioncontainer.h>

#include <QMainWindow>
#include <QMenuBar>

namespace Fy::Utils {
ActionManager::ActionManager(QObject* parent)
    : QObject{parent}
    , m_mainWindow{nullptr}
{ }

void ActionManager::setMainWindow(QMainWindow* mainWindow)
{
    m_mainWindow = mainWindow;
}

ActionContainer* ActionManager::createMenu(const Id& id)
{
    if(m_idContainerMap.contains(id)) {
        return m_idContainerMap.at(id);
    }

    auto* container = new MenuActionContainer(id, this);
    registerContainer(id, container);

    return container;
}

ActionContainer* ActionManager::createMenuBar(const Id& id)
{
    if(m_idContainerMap.contains(id)) {
        return m_idContainerMap.at(id);
    }

    auto* menuBar = new QMenuBar(m_mainWindow);
    menuBar->setObjectName(id.name());

    auto* container = new MenuBarActionContainer(id, this);
    container->setMenuBar(menuBar);
    registerContainer(id, container);

    return container;
}

void ActionManager::registerAction(QAction* newAction, const Id& id)
{
    if(m_mainWindow) {
        m_idCmdMap.emplace(id, newAction);
        m_mainWindow->addAction(newAction);
        newAction->setObjectName(id.name());
    }
}

QAction* ActionManager::action(const Id& id)
{
    if(m_idCmdMap.contains(id)) {
        return m_idCmdMap.at(id);
    }
    return nullptr;
}

ActionContainer* ActionManager::actionContainer(const Id& id)
{
    if(m_idContainerMap.contains(id)) {
        return m_idContainerMap.at(id);
    }
    return nullptr;
}

void ActionManager::registerContainer(const Id& id, ActionContainer* actionContainer)
{
    QObject::connect(actionContainer, &ActionContainer::registerSeparator, this, &ActionManager::registerAction);
    QObject::connect(actionContainer, &QObject::destroyed, this, &ActionManager::containerDestroyed);

    actionContainer->appendGroup(Groups::Default);
    m_idContainerMap.emplace(id, actionContainer);
}

void ActionManager::containerDestroyed(QObject* sender)
{
    auto* container = static_cast<ActionContainer*>(sender);
    m_idContainerMap.erase(container->id());
}
} // namespace Fy::Utils
