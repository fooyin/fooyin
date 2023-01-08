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
#include <map>

namespace Core::Widgets {
class WidgetFactory : public QObject
{
    Q_OBJECT

public:
    using Instantiator = FyWidget* (*)();

    struct FactoryWidget
    {
        QString name;
        Instantiator instantiator;
        QStringList subMenus;
    };

protected:
    template <typename U>
    static FyWidget* createInstance()
    {
        return new U();
    }
    using FactoryWidgets = std::map<QString, FactoryWidget>;

    FactoryWidgets widgets;

public:
    template <typename U>
    bool registerClass(const QString& name, const QStringList& subMenus = {})
    {
        static_assert(std::is_base_of<FyWidget, U>::value, "Class must derive from the factory's base class");

        if(widgets.count(name)) {
            qDebug() << ("Subclass already registered");
            return false;
        }

        FactoryWidget fw;
        fw.name = name;
        fw.instantiator = &createInstance<U>;
        fw.subMenus = subMenus;

        widgets.emplace(name, fw);
        return true;
    }

    [[nodiscard]] FyWidget* make(const QString& name) const
    {
        if(!widgets.count(name)) {
            return nullptr;
        }

        auto widget = widgets.at(name);
        if(!widget.instantiator) {
            return nullptr;
        }
        return widget.instantiator();
    }

    [[nodiscard]] FactoryWidgets registeredWidgets() const
    {
        return widgets;
    }
};
} // namespace Core::Widgets
