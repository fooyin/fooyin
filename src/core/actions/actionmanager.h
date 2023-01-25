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

namespace Core {
class ActionContainer;
class ActionManager : public QObject
{
    Q_OBJECT

public:
    explicit ActionManager(QObject* parent = nullptr);
    ~ActionManager() override;

    void setMainWindow(QMainWindow* mainWindow);

    ActionContainer* createMenu(const Utils::Id& id);
    ActionContainer* createMenuBar(const Utils::Id& id);
    void registerAction(QAction* action, const Utils::Id& id);

    QAction* action(const Utils::Id& id);
    ActionContainer* actionContainer(const Utils::Id& id);

protected:
    void containerDestroyed(QObject* sender);

private:
    struct Private;
    std::unique_ptr<ActionManager::Private> p;
};
} // namespace Core
