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

#include <utils/settings/settingsmanager.h>

#include <QIODevice>
#include <QObject>

#include <ranges>

namespace Fy::Utils {
class SettingsManager;

class RegistryBase : public QObject
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
 * Items are saved and loaded to/from a QByteArray which is stored in SettingKey.
 * @tparam Item A struct or class with public members: int id, int index, QString name.
 * @tparam SettingKey An enum value for a setting stored in SettingsManager.
 */
template <typename Item, auto SettingKey>
class ItemRegistry : public RegistryBase
{
public:
    using IndexItemMap = std::map<int, Item>;

    explicit ItemRegistry(Utils::SettingsManager* settings, QObject* parent = nullptr)
        : RegistryBase{parent}
        , m_settings{settings}
    { }

    [[nodiscard]] const IndexItemMap& items() const
    {
        return m_items;
    }

    virtual Item addItem(const Item& item)
    {
        auto find = [](const IndexItemMap& items, const QString& name) -> int {
            return static_cast<int>(std::count_if(items.cbegin(), items.cend(), [name](const auto& item) {
                return item.second.name == name;
            }));
        };

        auto findValidId = [this]() -> int {
            if(m_items.empty()) {
                return 0;
            }

            auto ids = m_items | std::views::values | std::views::transform([](const Item& item) {
                           return item.id;
                       });

            const int nextId = *std::ranges::max_element(ids) + 1;
            return nextId;
        };

        Item newItem{item};
        if(newItem.name.isEmpty()) {
            newItem.name = "New item";
        }
        if(find(m_items, newItem.name)) {
            auto count = std::max(find(m_items, newItem.name + " ("), 1);
            newItem.name += QString{" (%1)"}.arg(count);
        }
        newItem.id    = findValidId();
        newItem.index = static_cast<int>(m_items.size());

        return m_items.emplace(newItem.index, newItem).first->second;
    }

    virtual bool changeItem(const Item& item)
    {
        auto itemIt = std::find_if(m_items.begin(), m_items.end(), [item](const auto& regItem) {
            return regItem.first == item.index;
        });
        if(itemIt != m_items.end()) {
            const Item oldItem = std::exchange(itemIt->second, item);
            emit itemChanged(item.id);
            return true;
        }
        return false;
    }

    Item itemById(int id) const
    {
        if(m_items.empty()) {
            return {};
        }
        auto it = std::find_if(m_items.cbegin(), m_items.cend(), [id](const auto& item) {
            return item.second.id == id;
        });
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
        auto it = std::ranges::find_if(std::as_const(m_items), [name](const auto& item) {
            return item.second.name == name;
        });
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
        QByteArray byteArray;
        QDataStream out(&byteArray, QIODevice::WriteOnly);

        out << m_items;

        byteArray = byteArray.toBase64();
        m_settings->set<SettingKey>(byteArray);
    };

    virtual void loadItems()
    {
        m_items.clear();

        QByteArray currentFields = m_settings->value<SettingKey>();
        currentFields            = QByteArray::fromBase64(currentFields);

        QDataStream in(&currentFields, QIODevice::ReadOnly);

        in >> m_items;
    };

protected:
    Utils::SettingsManager* m_settings;
    IndexItemMap m_items;
};
} // namespace Fy::Utils

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
