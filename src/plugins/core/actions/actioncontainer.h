/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include <QObject>
#include <utils/id.h>

class QMenu;
class QMenuBar;
class QAction;

namespace Core {
class ActionContainer : public QObject
{
    Q_OBJECT

public:
    struct Group
    {
        explicit Group(const Util::Id& id)
            : id{id}
        { }
        Util::Id id;
        QList<QObject*> items;
    };

    explicit ActionContainer(const Util::Id& id, QObject* parent = nullptr);
    ~ActionContainer() override;

    [[nodiscard]] Util::Id id() const;
    [[nodiscard]] virtual QMenu* menu() const;
    [[nodiscard]] virtual QMenuBar* menuBar() const;

    [[nodiscard]] QAction* insertLocation(const Util::Id& group) const;
    void appendGroup(const Util::Id& group);
    void insertGroup(Util::Id beforeGroup, Util::Id group);
    void addAction(QAction* action, const Util::Id& group = {});
    void addMenu(ActionContainer* menu, const Util::Id& group = {});
    void addMenu(ActionContainer* beforeContainer, ActionContainer* menu);
    QAction* addSeparator(const Util::Id& group = {});
    QAction* addSeparator(const Util::Id& group = {}, QAction** outSeparator = nullptr);

    [[nodiscard]] virtual QAction* containerAction() const = 0;
    virtual QAction* actionForItem(QObject* item) const;

    virtual void insertAction(QAction* beforeAction, QAction* action) = 0;
    virtual void insertMenu(QAction* beforeAction, ActionContainer* container) = 0;

    virtual bool isEmpty() = 0;
    virtual bool isHidden() = 0;
    virtual void clear() = 0;

signals:
    void aboutToHide();

protected:
    virtual bool canBeAddedToContainer(ActionContainer* container) const = 0;

    QList<Group> m_groups;

private:
    void itemDestroyed(QObject* sender);

    [[nodiscard]] QList<Group>::const_iterator findGroup(const Util::Id& groupId) const;
    [[nodiscard]] QAction* insertLocation(QList<Group>::const_iterator group) const;

    Util::Id m_id;
};

class MenuActionContainer : public ActionContainer
{
    Q_OBJECT

public:
    explicit MenuActionContainer(const Util::Id& id, QObject* parent = nullptr);
    ~MenuActionContainer() override;

    [[nodiscard]] QMenu* menu() const override;

    [[nodiscard]] QAction* containerAction() const override;

    void insertAction(QAction* beforeAction, QAction* action) override;
    void insertMenu(QAction* beforeAction, ActionContainer* container) override;

    bool isEmpty() override;
    bool isHidden() override;
    void clear() override;

protected:
    bool canBeAddedToContainer(ActionContainer* container) const override;

private:
    QMenu* m_menu;
};

class MenuBarActionContainer : public ActionContainer
{
    Q_OBJECT

public:
    explicit MenuBarActionContainer(const Util::Id& id, QObject* parent = nullptr);

    [[nodiscard]] QMenu* menu() const override;
    void setMenuBar(QMenuBar* menuBar);
    [[nodiscard]] QMenuBar* menuBar() const override;

    [[nodiscard]] QAction* containerAction() const override;

    void insertAction(QAction* beforeAction, QAction* action) override;
    void insertMenu(QAction* beforeAction, ActionContainer* container) override;

    bool isEmpty() override;
    bool isHidden() override;
    void clear() override;

protected:
    bool canBeAddedToContainer(ActionContainer* container) const override;

private:
    QMenuBar* m_menuBar;
};
}; // namespace Core
