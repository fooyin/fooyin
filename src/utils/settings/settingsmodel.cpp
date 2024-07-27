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

#include "settingsmodel.h"

#include <utils/settings/settingsmanager.h>
#include <utils/settings/settingspage.h>

namespace Fooyin {
SettingsItem::SettingsItem()
    : SettingsItem{nullptr, nullptr}
{ }

SettingsItem::SettingsItem(SettingsCategory* data, SettingsItem* parent)
    : TreeItem{parent}
    , m_data{data}
{ }

SettingsCategory* SettingsItem::data() const
{
    return m_data;
}

void SettingsItem::sortChildren()
{
    for(SettingsItem* child : m_children) {
        child->sortChildren();
        child->resetRow();
    }
    std::ranges::sort(m_children, [](const SettingsItem* lhs, const SettingsItem* rhs) {
        return lhs->m_data->name < rhs->m_data->name;
    });
}

SettingsModel::SettingsModel(QObject* parent)
    : TreeModel{parent}
{ }

QVariant SettingsModel::data(const QModelIndex& index, int role) const
{
    if(!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    const auto* item       = static_cast<SettingsItem*>(index.internalPointer());
    SettingsCategory* data = item->data();

    switch(role) {
        case(Qt::DisplayRole):
            return data->name;
        case(SettingsItem::Data):
            return QVariant::fromValue(data);
        default:
            break;
    }

    return {};
}

void SettingsModel::setPages(const PageList& pages)
{
    beginResetModel();

    m_categories.clear();
    m_pageIds.clear();

    for(const auto& page : pages) {
        if(m_pageIds.contains(page->id())) {
            qCWarning(SETTINGS) << "Duplicate settings page:" << page->id().name();
            continue;
        }

        m_pageIds.insert(page->id());
        SettingsItem* parent         = rootItem();
        const QStringList categories = page->category();
        Id categoryId;
        SettingsCategory* category = nullptr;

        for(const QString& categoryName : categories) {
            categoryId = categoryId.append(categoryName);

            if(!m_items.contains(categoryId)) {
                m_categories.emplace(categoryId, SettingsCategory{.id = categoryId, .name = categoryName});
                category = &m_categories.at(categoryId);
                m_items.emplace(categoryId, SettingsItem{category, parent});
                auto* item = &m_items.at(categoryId);
                parent->appendChild(item);
            }

            category   = &m_categories.at(categoryId);
            auto* item = &m_items.at(categoryId);
            parent     = item;
        }

        if(category) {
            category->pages.emplace_back(page);
        }
    }

    // rootItem()->sortChildren();
    endResetModel();
}

SettingsCategory* SettingsModel::categoryForPage(const Id& page)
{
    for(auto& [_, category] : m_categories) {
        const int pageIndex = category.findPageById(page);
        if(pageIndex >= 0) {
            return &category;
        }
    }
    return nullptr;
}

QModelIndex SettingsModel::indexForCategory(const Id& categoryId) const
{
    for(const auto& [_, category] : m_items) {
        if(category.data()->id == categoryId) {
            return createIndex(category.row(), 0, &category);
        }
    }
    return {};
}
} // namespace Fooyin
