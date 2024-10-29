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
    { t.id } -> std::convertible_to<int>;
    { t.name } -> std::convertible_to<QString>;
    { t.index } -> std::convertible_to<int>;
    { t.isDefault } -> std::convertible_to<bool>;
};

namespace Fooyin {
class FYUTILS_EXPORT RegistryBase : public QObject
{
    Q_OBJECT

public:
    explicit RegistryBase(QObject* parent = nullptr);

signals:
    void itemAdded(int id);
    void itemChanged(int id);
    void itemRemoved(int id);
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
    { }

    [[nodiscard]] ItemList items() const
    {
        return m_items;
    }

    [[nodiscard]] int count() const
    {
        return static_cast<int>(m_items.size());
    }

    [[nodiscard]] bool empty() const
    {
        return m_items.empty();
    }

    void reset()
    {
        m_itemsChanged = false;
        m_settings->fileRemove(m_settingKey);
        m_items.clear();
        loadDefaults();
    }

    Item addItem(const Item& item)
    {
        Item newItem{item};
        newItem.name  = findUniqueName(newItem.name);
        newItem.id    = findValidId();
        newItem.index = static_cast<int>(m_items.size());
        m_items.push_back(newItem);

        m_itemsChanged = true;
        emit itemAdded(newItem.id);
        saveItems();
        return newItem;
    }

    bool changeItem(const Item& item)
    {
        const auto itemIt
            = std::ranges::find_if(m_items, [item](const auto& regItem) { return regItem.id == item.id; });
        if(itemIt == m_items.end()) {
            return false;
        }

        if(itemIt->isDefault || *itemIt == item) {
            return false;
        }

        Item changedItem{item};
        if(itemIt->name != changedItem.name) {
            changedItem.name = findUniqueName(changedItem.name);
        }
        *itemIt = changedItem;

        m_itemsChanged = true;
        std::ranges::sort(m_items, {}, &Item::index);
        emit itemChanged(changedItem.id);
        saveItems();
        return true;
    }

    std::optional<Item> itemById(int id) const
    {
        if(m_items.empty()) {
            return {};
        }

        auto it = std::ranges::find_if(m_items, [id](const auto& item) { return item.id == id; });
        if(it != m_items.end()) {
            return *it;
        }

        return {};
    }

    std::optional<Item> itemByIndex(int index) const
    {
        if(m_items.empty()) {
            return {};
        }

        auto it = std::ranges::find_if(m_items, [index](const auto& item) { return item.index == index; });
        if(it != m_items.end()) {
            return *it;
        }

        return {};
    }

    std::optional<Item> itemByName(const QString& name) const
    {
        if(m_items.empty()) {
            return {};
        }

        auto it = std::ranges::find_if(m_items, [name](const auto& item) { return item.name == name; });
        if(it != m_items.end()) {
            return *it;
        }

        return {};
    }

    bool removeById(int id)
    {
        if(std::erase_if(m_items, [id](const auto& item) { return !item.isDefault && item.id == id; }) == 0) {
            return false;
        }

        m_itemsChanged = true;
        emit itemRemoved(id);
        saveItems();
        return true;
    }

    void saveItems() const
    {
        if(!m_itemsChanged) {
            m_settings->fileRemove(m_settingKey);
            return;
        }

        QByteArray byteArray;
        QDataStream stream(&byteArray, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_0);

        const int customCount = std::ranges::count_if(m_items, [](const auto& regItem) { return !regItem.isDefault; });
        stream << customCount;

        for(const auto& item : m_items | std::views::filter([](const auto& regItem) { return !regItem.isDefault; })) {
            stream << item;
        }

        byteArray = qCompress(byteArray, 9);

        m_settings->fileSet(m_settingKey, byteArray);
    }

    void loadItems()
    {
        ItemList oldItems{m_items};
        m_items.clear();
        loadDefaults();

        QByteArray byteArray = m_settings->fileValue(m_settingKey).toByteArray();

        ItemList defaultItemsToAdjust;

        if(!byteArray.isEmpty()) {
            byteArray = qUncompress(byteArray);

            QDataStream stream(&byteArray, QIODevice::ReadOnly);
            stream.setVersion(QDataStream::Qt_6_0);

            int size;
            stream >> size;

            while(size > 0) {
                --size;

                Item item;
                stream >> item;

                item.name  = findUniqueName(item.name);
                item.index = static_cast<int>(m_items.size());

                if(const auto existingItem = itemById(item.id)) {
                    defaultItemsToAdjust.emplace_back(existingItem.value());
                }

                m_items.push_back(item);
            }

            m_itemsChanged = true;
        }

        // If we add new default items but the user has added custom items, there will be an id conflict.
        // Adjust for this here.
        for(const auto& item : defaultItemsToAdjust) {
            auto itemToAdjust = item;
            const auto itemIt
                = std::ranges::find_if(m_items, [item](const auto& regItem) { return regItem.id == item.id; });
            if(itemIt != m_items.end()) {
                itemToAdjust.id = findValidId();
                *itemIt         = itemToAdjust;
                emit itemChanged(itemToAdjust.id);
            }
        }
    }

protected:
    virtual void loadDefaults() { }

    void addDefaultItem(const Item& item, bool isEditable = false)
    {
        Item defaultItem{item};
        defaultItem.isDefault = !isEditable;

        if(defaultItem.name.isEmpty()) {
            defaultItem.name = findUniqueName(defaultItem.name);
        }
        if(defaultItem.id < 0) {
            defaultItem.id = findValidId();
        }
        if(defaultItem.index < 0 || std::cmp_greater(defaultItem.index, m_items.size())) {
            defaultItem.index = static_cast<int>(m_items.size());
        }

        m_items.insert(m_items.begin() + defaultItem.index, defaultItem);
    }

private:
    [[nodiscard]] QString findUniqueName(const QString& name) const
    {
        const QString uniqueName{name.isEmpty() ? QStringLiteral("New item") : name};
        return Utils::findUniqueString(uniqueName, m_items, [](const auto& item) { return item.name; });
    }

    [[nodiscard]] int findValidId() const
    {
        if(m_items.empty()) {
            return 0;
        }

        auto ids         = m_items | std::views::transform([](const auto& regItem) { return regItem.id; });
        const int nextId = *std::ranges::max_element(ids) + 1;
        return nextId;
    }

    SettingsManager* m_settings;
    QString m_settingKey;
    ItemList m_items;
    bool m_itemsChanged{false};
};
} // namespace Fooyin
