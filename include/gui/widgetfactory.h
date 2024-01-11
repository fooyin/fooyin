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

#include "fygui_export.h"

#include <gui/fywidget.h>

#include <map>

namespace Fooyin {
class FYGUI_EXPORT WidgetFactory
{
private:
    using Instantiator = std::function<FyWidget*()>;

    struct FactoryWidget
    {
        QString name;
        Instantiator instantiator;
        QStringList subMenus;
        int limit{0};
    };

    friend class WidgetProvider;

    using FactoryWidgets = std::map<QString, FactoryWidget>;
    FactoryWidgets widgets;

    std::optional<FactoryWidget> widget(const QString& key) const
    {
        if(!widgets.contains(key)) {
            return {};
        }
        return widgets.at(key);
    }

    [[nodiscard]] FactoryWidgets registeredWidgets() const
    {
        return widgets;
    }

public:
    template <typename T, typename Factory>
    bool registerClass(const QString& key, Factory factory, const QString& displayName = QStringLiteral(""))
    {
        static_assert(std::is_base_of<FyWidget, T>::value, "Class must derive from the factory's base class");

        if(widgets.contains(key)) {
            qDebug() << "Subclass already registered";
            return false;
        }

        FactoryWidget fw;
        fw.name         = displayName.isEmpty() ? key : displayName;
        fw.instantiator = factory;

        widgets.emplace(key, fw);
        return true;
    }

    void setSubMenus(const QString& key, const QStringList& subMenus)
    {
        if(!widgets.contains(key)) {
            qDebug() << "Subclass not registered";
            return;
        }

        widgets.at(key).subMenus = subMenus;
    }

    void setLimit(const QString& key, int limit)
    {
        if(!widgets.contains(key)) {
            qDebug() << "Subclass not registered";
            return;
        }

        widgets.at(key).limit = limit;
    }
};
} // namespace Fooyin
