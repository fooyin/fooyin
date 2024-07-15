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

#pragma once

#include <utils/actions/actioncontainer.h>

namespace Fooyin {
class ActionManager;

class MenuBarContainer : public ActionContainer
{
    Q_OBJECT

public:
    explicit MenuBarContainer(const Id& id, ActionManager* manager);

    [[nodiscard]] QMenu* menu() const override;
    void setMenuBar(QMenuBar* menuBar);
    [[nodiscard]] QMenuBar* menuBar() const override;

    [[nodiscard]] QAction* containerAction() const override;

    void insertAction(QAction* beforeAction, QAction* action) override;
    void insertAction(QAction* beforeAction, Command* action) override;
    void insertMenu(QAction* beforeAction, ActionContainer* container) override;

    bool isEmpty() override;
    bool isHidden() override;
    void clear() override;

    bool update() override;

protected:
    bool canBeAddedToContainer(ActionContainer* container) const override;

private:
    QMenuBar* m_menuBar;
};

class MenuContainer : public ActionContainer
{
    Q_OBJECT

public:
    explicit MenuContainer(const Id& id, ActionManager* manager);
    ~MenuContainer() override;

    [[nodiscard]] QMenu* menu() const override;

    [[nodiscard]] QAction* containerAction() const override;

    void insertAction(QAction* beforeAction, QAction* action) override;
    void insertAction(QAction* beforeAction, Command* action) override;
    void insertMenu(QAction* beforeAction, ActionContainer* container) override;

    bool isEmpty() override;
    bool isHidden() override;
    void clear() override;

    bool update() override;

protected:
    bool canBeAddedToContainer(ActionContainer* container) const override;

private:
    std::unique_ptr<QMenu> m_menu;
};
} // namespace Fooyin
