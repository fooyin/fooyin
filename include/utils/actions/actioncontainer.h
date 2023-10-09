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

#include "fyutils_export.h"

#include <utils/id.h>

#include <QObject>

class QAction;
class QMenu;
class QMenuBar;

namespace Fy::Utils {
namespace Groups {
constexpr auto Default = "Group.Default";
}

class FYUTILS_EXPORT ActionContainer : public QObject
{
    Q_OBJECT

public:
    struct Group
    {
        explicit Group(const Id& id)
            : id{id}
        { }
        Id id;
        QList<QObject*> items;
    };
    using GroupList = std::vector<Group>;

    explicit ActionContainer(const Id& id, QObject* parent = nullptr);
    ~ActionContainer() override = default;

    [[nodiscard]] Id id() const;
    [[nodiscard]] virtual QMenu* menu() const;
    [[nodiscard]] virtual QMenuBar* menuBar() const;

    [[nodiscard]] QAction* insertLocation(const Id& group) const;
    void appendGroup(const Id& group);
    void insertGroup(const Id& beforeGroup, const Id& group);
    void addAction(QAction* action, const Id& group = {});
    void addMenu(ActionContainer* menu, const Id& group = {});
    void addMenu(ActionContainer* beforeContainer, ActionContainer* menu);
    QAction* addSeparator(const Id& group = {});

    [[nodiscard]] virtual QAction* containerAction() const = 0;
    virtual QAction* actionForItem(QObject* item) const;

    virtual void insertAction(QAction* beforeAction, QAction* action)          = 0;
    virtual void insertMenu(QAction* beforeAction, ActionContainer* container) = 0;

    virtual bool isEmpty()  = 0;
    virtual bool isHidden() = 0;
    virtual void clear()    = 0;

signals:
    void aboutToHide();
    void registerSeparator(QAction* action, const Id& id);

protected:
    virtual bool canBeAddedToContainer(ActionContainer* container) const = 0;

    GroupList m_groups;

private:
    void itemDestroyed(QObject* sender);

    [[nodiscard]] GroupList::const_iterator findGroup(const Id& groupId) const;
    [[nodiscard]] QAction* insertLocation(GroupList::const_iterator group) const;

    Id m_id;
};

class FYUTILS_EXPORT MenuActionContainer : public ActionContainer
{
    Q_OBJECT

public:
    explicit MenuActionContainer(const Id& id, QObject* parent = nullptr);
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

class FYUTILS_EXPORT MenuBarActionContainer : public ActionContainer
{
    Q_OBJECT

public:
    explicit MenuBarActionContainer(const Id& id, QObject* parent = nullptr);

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
} // namespace Fy::Utils
