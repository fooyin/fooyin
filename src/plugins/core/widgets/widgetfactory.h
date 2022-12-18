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

#include "core/gui/fywidget.h"

#include <QDebug>
#include <QHash>

namespace Widgets {
class WidgetFactory : public QObject
{
    Q_OBJECT

public:
    using Instantiator = FyWidget* (*)();

protected:
    template <typename U>
    static FyWidget* createInstance()
    {
        return new U();
    }
    using Instantiators = QMap<QString, Instantiator>;
    using SubMenus = QMap<QString, QStringList>;

    Instantiators instantiators;
    SubMenus subMenus;

public:
    template <typename U>
    bool registerClass(const QString& key, const QStringList& subMenu = {})
    {
        static_assert(std::is_base_of<FyWidget, U>::value, "Class must derive from the factory's base class");
        if(instantiators.contains(key)) {
            qDebug() << ("Subclass already registered");
            return false;
        }
        instantiators.insert(key, &createInstance<U>);
        if(!subMenu.isEmpty()) {
            subMenus.insert(key, subMenu);
        }
        return true;
    }

    [[nodiscard]] FyWidget* make(const QString& key) const
    {
        auto it = instantiators.value(key, nullptr);
        if(!it) {
            return nullptr;
        }
        return it();
    }

    [[nodiscard]] QList<QString> widgetNames() const
    {
        return instantiators.keys();
    }

    [[nodiscard]] SubMenus menus() const
    {
        return subMenus;
    }
};
} // namespace Widgets
