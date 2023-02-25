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

#include "actionmanager.h"

#include "actioncontainer.h"

#include <QMainWindow>
#include <QMenuBar>

namespace Fy::Utils {
ActionManager::ActionManager(QObject* parent)
    : QObject{parent}
{ }

void ActionManager::setMainWindow(QMainWindow* mainWindow)
{
    m_mainWindow = mainWindow;
}

ActionContainer* ActionManager::createMenu(const Id& id)
{
    for(const auto& [mapId, container] : m_idContainerMap) {
        if(mapId == id) {
            return container;
        }
    }

    auto* mActionContainer = new MenuActionContainer(id, this);
    connect(mActionContainer, &ActionContainer::registerSeperator, this, &ActionManager::registerAction);
    connect(mActionContainer, &QObject::destroyed, this, &ActionManager::containerDestroyed);

    mActionContainer->appendGroup(Groups::Default);
    m_idContainerMap.emplace(id, mActionContainer);

    return mActionContainer;
}

ActionContainer* ActionManager::createMenuBar(const Id& id)
{
    for(const auto& [mapId, container] : m_idContainerMap) {
        if(mapId == id) {
            return container;
        }
    }

    auto* menuBar = new QMenuBar(m_mainWindow);
    menuBar->setObjectName(id.name());

    auto* mbActionContainer = new MenuBarActionContainer(id, this);
    connect(mbActionContainer, &ActionContainer::registerSeperator, this, &ActionManager::registerAction);
    connect(mbActionContainer, &QObject::destroyed, this, &ActionManager::containerDestroyed);

    mbActionContainer->setMenuBar(menuBar);
    m_idContainerMap.emplace(id, mbActionContainer);

    return mbActionContainer;
}

void ActionManager::registerAction(QAction* action, const Id& id)
{
    m_idCmdMap.emplace(id, action);
    m_mainWindow->addAction(action);
    action->setObjectName(id.name());
}

QAction* ActionManager::action(const Id& id)
{
    for(const auto& [mapId, action] : m_idCmdMap) {
        if(mapId == id) {
            return action;
        }
    }
    return nullptr;
}

ActionContainer* ActionManager::actionContainer(const Id& id)
{
    for(const auto& [mapId, container] : m_idContainerMap) {
        if(mapId == id) {
            return container;
        }
    }
    return nullptr;
}

void ActionManager::containerDestroyed(QObject* sender)
{
    auto* container = static_cast<ActionContainer*>(sender);
    m_idContainerMap.erase(container->id());
}
} // namespace Fy::Utils
