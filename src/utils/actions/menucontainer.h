/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

class MenuContainer : public ActionContainer
{
    Q_OBJECT

public:
    struct Group
    {
        explicit Group(const Id& id)
            : id{id}
        { }
        Id id;
        std::vector<QObject*> items;
    };
    using GroupList = std::vector<Group>;

    explicit MenuContainer(const Id& id, ActionManager* manager);
    ~MenuContainer() override;

    [[nodiscard]] Id id() const override;
    [[nodiscard]] QMenu* menu() const override;
    [[nodiscard]] QMenuBar* menuBar() const override;

    [[nodiscard]] QAction* insertLocation(const Id& group) const override;

    void appendGroup(const Id& group) override;
    void insertGroup(const Id& beforeGroup, const Id& group) override;

    void addAction(QAction* action, const Id& group = {}) override;
    void addAction(Command* action, const Id& group = {}) override;

    void addMenu(ActionContainer* menu, const Id& group = {}) override;
    void addMenu(ActionContainer* beforeContainer, ActionContainer* menu) override;

    Command* addSeparator(const Context& context, const Id& group = {}) override;
    Command* addSeparator(const Id& group = {}) override;

    [[nodiscard]] virtual QAction* containerAction() const = 0;
    virtual QAction* actionForItem(QObject* item) const;

    virtual void insertAction(QAction* beforeAction, QAction* action)          = 0;
    virtual void insertAction(QAction* beforeAction, Command* action)          = 0;
    virtual void insertMenu(QAction* beforeAction, ActionContainer* container) = 0;

    DisabledBehavior disabledBehavior() const override;
    void setDisabledBehavior(DisabledBehavior behavior) override;

    bool isEmpty() override;
    bool isHidden() override;
    void clear() override;

    virtual bool update() = 0;

signals:
    void requestUpdate(MenuContainer* container);

protected:
    virtual bool canBeAddedToContainer(ActionContainer* container) const = 0;

    GroupList groups;

private:
    struct Private;
    std::unique_ptr<Private> p;
};

class MenuBarActionContainer : public MenuContainer
{
    Q_OBJECT

public:
    explicit MenuBarActionContainer(const Id& id, ActionManager* manager);

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

class MenuActionContainer : public MenuContainer
{
    Q_OBJECT

public:
    explicit MenuActionContainer(const Id& id, ActionManager* manager);
    ~MenuActionContainer() override;

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
