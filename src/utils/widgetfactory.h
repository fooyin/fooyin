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

#include <QDebug>
#include <QHash>

namespace Util {
template <typename Key, typename T, typename... Args>
class WidgetFactory
{
public:
    using Instantiator = T* (*)(Args...);

protected:
    template <typename U>
    static T* createInstance(Args... args)
    {
        return new U(std::forward<Args>(args)...);
    }
    using Instantiators = QHash<Key, Instantiator>;

    Instantiators instantiators;

public:
    template <typename U>
    bool registerClass(const Key& key)
    {
        static_assert(std::is_base_of<T, U>::value, "Class must derive from the factory's base class");
        if(instantiators.contains(key)) {
            qDebug() << ("Subclass already registered");
            return false;
        }
        instantiators.insert(key, &createInstance<U>);
        return true;
    }

    T* make(const Key& key, Args... args) const
    {
        auto it = instantiators.value(key, nullptr);
        if(!it) {
            return nullptr;
        }
        return it(std::forward<Args>(args)...);
    }

    QList<Key> widgetNames() const
    {
        return instantiators.keys();
    }
};
} // namespace Util
