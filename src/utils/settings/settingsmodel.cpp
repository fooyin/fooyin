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

#include "settingsmodel.h"

#include "settingspage.h"

namespace Utils {
SettingsModel::SettingsModel()
{
    QPixmap emptyPixmap(24, 24);
    emptyPixmap.fill(Qt::transparent);
    m_emptyIcon = QIcon{emptyPixmap};
}

SettingsModel::~SettingsModel()
{
    for(const auto& category : m_categories) {
        delete category;
    }
    m_categories.clear();
}

int SettingsModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_categories.size());
}

QVariant SettingsModel::data(const QModelIndex& index, int role) const
{
    const int row = index.row();

    switch(role) {
        case Qt::DisplayRole:
            return m_categories.at(row)->name;
        case Qt::DecorationRole: {
            QIcon icon = m_categories.at(row)->icon;
            if(icon.isNull()) {
                icon = m_emptyIcon;
            }
            return icon;
        }
        default:
            return {};
    }
}

const CategoryList& SettingsModel::categories() const
{
    return m_categories;
}

void SettingsModel::setPages(const PageList& pages)
{
    beginResetModel();

    m_categories.clear();
    m_pageIds.clear();

    for(const auto& page : pages) {
        if(m_pageIds.count(page->id())) {
            qWarning() << "Duplicate settings page " << page->id().name();
            continue;
        }
        m_pageIds.insert(page->id());

        const Id categoryId        = page->category();
        SettingsCategory* category = findCategoryById(categoryId);
        if(!category) {
            category            = new SettingsCategory();
            category->id        = categoryId;
            category->tabWidget = nullptr;
            category->index     = -1;
            m_categories.emplace_back(category);
        }
        if(category->name.isEmpty()) {
            category->name = page->categoryName();
        }
        if(category->icon.isNull()) {
            category->icon = page->categoryIcon();
        }
        category->pages.emplace_back(page);
    }

    std::sort(m_categories.begin(), m_categories.end(), [](const auto* c1, const auto* c2) {
        return c1->name < c2->name;
    });

    endResetModel();
}

SettingsCategory* SettingsModel::findCategoryById(const Id& id)
{
    for(const auto& category : m_categories) {
        if(category->id == id) {
            return category;
        }
    }
    return nullptr;
}
} // namespace Utils
