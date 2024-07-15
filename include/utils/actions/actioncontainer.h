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

#include "fyutils_export.h"

#include <utils/actions/widgetcontext.h>
#include <utils/id.h>

#include <QObject>

class QAction;
class QMenu;
class QMenuBar;

namespace Fooyin {
class ActionContainerPrivate;
class ActionManager;
class Command;

namespace Actions::Groups {
constexpr auto One   = "Group.One";
constexpr auto Two   = "Group.Two";
constexpr auto Three = "Group.Three";
} // namespace Actions::Groups

class FYUTILS_EXPORT ActionContainer : public QObject
{
    Q_OBJECT

public:
    enum DisabledBehavior : uint8_t
    {
        Show = 0,
        Disable,
        Hide,
    };

    struct Group
    {
        explicit Group(const Id& id_)
            : id{id_}
        { }
        Id id;
        std::vector<QObject*> items;
    };
    using GroupList = std::vector<Group>;

    ActionContainer(const Id& id, ActionManager* manager);
    ~ActionContainer();

    [[nodiscard]] Id id() const;
    [[nodiscard]] virtual QMenu* menu() const;
    [[nodiscard]] virtual QMenuBar* menuBar() const;

    [[nodiscard]] QAction* insertLocation(const Id& group) const;

    void appendGroup(const Id& group);
    void insertGroup(const Id& beforeGroup, const Id& group);

    void addAction(QAction* action, const Id& group = {});
    void addAction(Command* action, const Id& group = {});

    void addMenu(ActionContainer* menu, const Id& group = {});
    void addMenu(ActionContainer* beforeContainer, ActionContainer* menu);

    Command* addSeparator(const Context& context, const Id& group = {});
    Command* addSeparator(const Id& group = {});

    [[nodiscard]] virtual QAction* containerAction() const = 0;
    virtual QAction* actionForItem(QObject* item) const;

    virtual void insertAction(QAction* beforeAction, QAction* action)          = 0;
    virtual void insertAction(QAction* beforeAction, Command* action)          = 0;
    virtual void insertMenu(QAction* beforeAction, ActionContainer* container) = 0;

    [[nodiscard]] virtual DisabledBehavior disabledBehavior() const;
    void setDisabledBehavior(DisabledBehavior behavior);

    virtual bool isEmpty();
    virtual bool isHidden();
    virtual void clear();
    virtual bool update() = 0;

signals:
    void requestUpdate(Fooyin::ActionContainer* container);

protected:
    [[nodiscard]] GroupList& actionGroups() const;
    virtual bool canBeAddedToContainer(ActionContainer* container) const = 0;

private:
    std::unique_ptr<ActionContainerPrivate> p;
};
} // namespace Fooyin
