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

#include <utils/helpers.h>
#include <utils/settings/settingsmanager.h>

#include <QIODevice>
#include <QObject>

#include <ranges>
#include <utility>

template <typename T>
concept ValidRegistry = requires(T t) {
    {
        t.id
    } -> std::convertible_to<int>;
    {
        t.name
    } -> std::convertible_to<QString>;
    {
        t.index
    } -> std::convertible_to<int>;
};

namespace Fooyin {
class FYUTILS_EXPORT RegistryBase : public QObject
{
    Q_OBJECT

public:
    explicit RegistryBase(QObject* parent = nullptr)
        : QObject{parent}
    { }

signals:
    void itemChanged(int id);
};

/*!
 * Represents a registry of items ordered by index.
 * Items are saved and loaded to/from a QByteArray which is stored in the passed settingKey.
 * @tparam Item A struct or class with public members: int id, int index, QString name.
 */
template <typename Item>
    requires ValidRegistry<Item>
class ItemRegistry : public RegistryBase
{
public:
    using IndexItemMap = std::map<int, Item>;

    explicit ItemRegistry(QString settingKey, SettingsManager* settings, QObject* parent = nullptr)
        : RegistryBase{parent}
        , m_settings{settings}
        , m_settingKey{std::move(settingKey)}
    {
        m_settings->createSetting(m_settingKey, {});
    }

    [[nodiscard]] const IndexItemMap& items() const
    {
        return m_items;
    }

    virtual Item addItem(const Item& item)
    {
        auto findValidId = [this]() -> int {
            if(m_items.empty()) {
                return 0;
            }

            auto ids = m_items | std::views::values | std::views::transform([](const Item& item) { return item.id; });

            const int nextId = *std::ranges::max_element(ids) + 1;
            return nextId;
        };

        Item newItem{item};
        newItem.name  = findUniqueName(newItem.name);
        newItem.id    = findValidId();
        newItem.index = static_cast<int>(m_items.size());

        return m_items.emplace(newItem.index, newItem).first->second;
    }

    virtual bool changeItem(const Item& item)
    {
        auto itemIt
            = std::ranges::find_if(m_items, [item](const auto& regItem) { return regItem.second.id == item.id; });

        if(itemIt != m_items.end()) {
            Item changedItem{item};
            if(itemIt->second.name != changedItem.name) {
                changedItem.name = findUniqueName(changedItem.name);
            }
            itemIt->second = changedItem;
            emit itemChanged(changedItem.id);
            return true;
        }
        return false;
    }

    Item itemById(int id) const
    {
        if(m_items.empty()) {
            return {};
        }
        auto it = std::ranges::find_if(std::as_const(m_items), [id](const auto& item) { return item.second.id == id; });
        if(it == m_items.end()) {
            return m_items.at(0);
        }
        return it->second;
    }

    Item itemByIndex(int index) const
    {
        if(!m_items.contains(index)) {
            return {};
        }
        return m_items.at(index);
    }

    Item itemByName(const QString& name) const
    {
        if(m_items.empty()) {
            return {};
        }
        auto it = std::ranges::find_if(std::as_const(m_items),
                                       [name](const auto& item) { return item.second.name == name; });
        if(it == m_items.end()) {
            return m_items.at(0);
        }
        return it->second;
    }

    bool removeByIndex(int index)
    {
        if(!m_items.contains(index)) {
            return false;
        }
        m_items.erase(index);
        return true;
    }

    virtual void saveItems()
    {
        if(m_items.empty()) {
            return;
        }

        QByteArray byteArray;
        QDataStream out(&byteArray, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_5);

        out << m_items;
        byteArray = qCompress(byteArray, 9);

        m_settings->set(m_settingKey, byteArray);
    }

    virtual void loadItems()
    {
        m_items.clear();

        QByteArray byteArray = m_settings->value(m_settingKey).toByteArray();

        if(!byteArray.isEmpty()) {
            byteArray = qUncompress(byteArray);

            QDataStream in(&byteArray, QIODevice::ReadOnly);
            in.setVersion(QDataStream::Qt_6_5);

            in >> m_items;
        }
    }

protected:
    IndexItemMap m_items;

private:
    QString findUniqueName(const QString& name)
    {
        const QString uniqueName{name.isEmpty() ? "New item" : name};
        return Utils::findUniqueString(uniqueName, std::as_const(m_items),
                                       [](const auto& item) { return item.second.name; });
    }

    SettingsManager* m_settings;
    QString m_settingKey;
};
} // namespace Fooyin

template <class T>
QDataStream& operator<<(QDataStream& stream, const std::map<int, T>& itemMap)
{
    stream << static_cast<int>(itemMap.size());
    for(const auto& [index, preset] : itemMap) {
        stream << index;
        stream << preset;
    }
    return stream;
}

template <class T>
QDataStream& operator>>(QDataStream& stream, std::map<int, T>& itemMap)
{
    int size;
    stream >> size;

    while(size > 0) {
        --size;

        T item;
        int index;
        stream >> index;
        stream >> item;
        itemMap.emplace(index, item);
    }
    return stream;
}
