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
    {
        t.isDefault
    } -> std::convertible_to<bool>;
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
    using ItemList = std::vector<Item>;

    explicit ItemRegistry(QString settingKey, SettingsManager* settings, QObject* parent = nullptr)
        : RegistryBase{parent}
        , m_settings{settings}
        , m_settingKey{std::move(settingKey)}
    {
        if(!m_settings->contains(m_settingKey)) {
            m_settings->createSetting(m_settingKey, {});
        }

        m_settings->subscribe(m_settingKey, this, &ItemRegistry::loadItems);
    }

    [[nodiscard]] ItemList items() const
    {
        return m_items;
    }

    [[nodiscard]] bool empty() const
    {
        return m_items.empty();
    }

    Item addItem(const Item& item)
    {
        return addItem(item, true);
    }

    bool changeItem(const Item& item)
    {
        auto itemIt = std::ranges::find_if(m_items, [item](const auto& regItem) { return regItem.id == item.id; });

        if(itemIt != m_items.end()) {
            if(itemIt->isDefault || *itemIt == item) {
                return false;
            }

            Item changedItem{item};
            if(itemIt->name != changedItem.name) {
                changedItem.name = findUniqueName(changedItem.name);
            }
            *itemIt = changedItem;

            saveItems();
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
        auto it = std::ranges::find_if(m_items, [id](const auto& item) { return item.id == id; });
        if(it == m_items.end()) {
            return m_items.at(0);
        }
        return *it;
    }

    Item itemByIndex(int index) const
    {
        if(m_items.empty()) {
            return {};
        }
        auto it = std::ranges::find_if(m_items, [index](const auto& item) { return item.index == index; });
        if(it == m_items.end()) {
            return m_items.at(0);
        }
        return *it;
    }

    Item itemByName(const QString& name) const
    {
        if(m_items.empty()) {
            return {};
        }
        auto it = std::ranges::find_if(m_items, [name](const auto& item) { return item.name == name; });
        if(it == m_items.end()) {
            return m_items.at(0);
        }
        return *it;
    }

    bool removeById(int id)
    {
        if(std::erase_if(m_items, [id](const auto& item) { return !item.isDefault && item.id == id; }) == 0) {
            return false;
        }

        saveItems();
        return true;
    }

    bool removeByIndex(int index)
    {
        if(std::erase_if(m_items, [index](const auto& item) { return !item.isDefault && item.index == index; }) == 0) {
            return false;
        }

        saveItems();
        return true;
    }

    void saveItems() const
    {
        if(m_items.empty()) {
            return;
        }

        QByteArray byteArray;
        QDataStream stream(&byteArray, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_5);

        const int customCount = std::ranges::count_if(m_items, [](const auto& regItem) { return !regItem.isDefault; });
        stream << customCount;

        for(const auto& item : m_items | std::views::filter([](const auto& regItem) { return !regItem.isDefault; })) {
            stream << item;
        }

        byteArray = qCompress(byteArray, 9);

        m_settings->unsubscribe(m_settingKey, this);
        m_settings->set(m_settingKey, byteArray);
        m_settings->subscribe(m_settingKey, this, &ItemRegistry::loadItems);
    }

    void loadItems()
    {
        ItemList oldItems{m_items};
        m_items.clear();
        loadDefaults();

        QByteArray byteArray = m_settings->value(m_settingKey).toByteArray();

        if(!byteArray.isEmpty()) {
            byteArray = qUncompress(byteArray);

            QDataStream stream(&byteArray, QIODevice::ReadOnly);
            stream.setVersion(QDataStream::Qt_6_5);

            int size;
            stream >> size;

            while(size > 0) {
                --size;

                Item item;
                stream >> item;

                m_items.push_back(item);
            }
        }

        checkChangedItems(oldItems);
    }

    void reset()
    {
        m_settings->reset(m_settingKey);
    }

protected:
    virtual void loadDefaults(){};

    void addDefaultItem(const Item& item)
    {
        Item defaultItem{item};
        defaultItem.isDefault = true;
        addItem(defaultItem, false);
    }

private:
    [[nodiscard]] QString findUniqueName(const QString& name) const
    {
        const QString uniqueName{name.isEmpty() ? "New item" : name};
        return Utils::findUniqueString(uniqueName, m_items, [](const auto& item) { return item.name; });
    }

    Item addItem(const Item& item, bool save)
    {
        auto findValidId = [this]() -> int {
            if(m_items.empty()) {
                return 0;
            }

            auto ids = m_items | std::views::transform([](const auto& regItem) { return regItem.id; });

            const int nextId = *std::ranges::max_element(ids) + 1;
            return nextId;
        };

        Item newItem{item};
        newItem.name  = findUniqueName(newItem.name);
        newItem.id    = findValidId();
        newItem.index = static_cast<int>(m_items.size());

        m_items.push_back(newItem);

        if(save) {
            saveItems();
        }

        return newItem;
    }

    void checkChangedItems(const ItemList& oldItems)
    {
        for(const auto& item : m_items) {
            auto it = std::ranges::find_if(
                oldItems, [item](const auto& oldItem) { return !oldItem.isDefault && oldItem.id == item.id; });
            if(it != oldItems.cend() && *it != item) {
                emit itemChanged(it->id);
            }
        }
    }

    SettingsManager* m_settings;
    QString m_settingKey;
    ItemList m_items;
};
} // namespace Fooyin
