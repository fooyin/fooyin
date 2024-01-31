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
    enum DisabledBehavior
    {
        Show = 0,
        Disable,
        Hide,
    };

    [[nodiscard]] virtual Id id() const             = 0;
    [[nodiscard]] virtual QMenu* menu() const       = 0;
    [[nodiscard]] virtual QMenuBar* menuBar() const = 0;

    [[nodiscard]] virtual QAction* insertLocation(const Id& group) const = 0;

    virtual void appendGroup(const Id& group)                        = 0;
    virtual void insertGroup(const Id& beforeGroup, const Id& group) = 0;

    virtual void addAction(QAction* action, const Id& group = {}) = 0;
    virtual void addAction(Command* action, const Id& group = {}) = 0;

    virtual void addMenu(ActionContainer* menu, const Id& group = {})             = 0;
    virtual void addMenu(ActionContainer* beforeContainer, ActionContainer* menu) = 0;

    virtual Command* addSeparator(const Context& context, const Id& group = {}) = 0;
    virtual Command* addSeparator(const Id& group = {})                         = 0;

    [[nodiscard]] virtual DisabledBehavior disabledBehavior() const = 0;
    virtual void setDisabledBehavior(DisabledBehavior behavior)     = 0;

    virtual bool isEmpty()  = 0;
    virtual bool isHidden() = 0;
    virtual void clear()    = 0;
};
} // namespace Fooyin
