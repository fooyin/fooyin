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

#pragma once

#include <utils/id.h>

#include <QObject>

class QAction;
class QMainWindow;

namespace Utils {
class ActionContainer;
class ActionManager : public QObject
{
    Q_OBJECT

public:
    explicit ActionManager(QObject* parent = nullptr);
    ~ActionManager() override = default;

    void setMainWindow(QMainWindow* mainWindow);

    ActionContainer* createMenu(const Id& id);
    ActionContainer* createMenuBar(const Id& id);
    void registerAction(QAction* action, const Id& id);

    QAction* action(const Id& id);
    ActionContainer* actionContainer(const Id& id);

protected:
    void containerDestroyed(QObject* sender);

private:
    QMainWindow* m_mainWindow;

    std::unordered_map<Id, QAction*, Id::IdHash> m_idCmdMap;
    std::unordered_map<Id, ActionContainer*, Id::IdHash> m_idContainerMap;
};
} // namespace Utils
