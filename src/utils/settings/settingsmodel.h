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

#include "settingscategory.h"

#include "utils/id.h"

#include <QAbstractListModel>
#include <QIcon>

#include <set>

namespace Utils {
using CategoryList = std::vector<SettingsCategory*>;

class SettingsModel : public QAbstractListModel
{
public:
    SettingsModel();
    ~SettingsModel() override;

    [[nodiscard]] int rowCount(const QModelIndex& parent) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;

    [[nodiscard]] const CategoryList& categories() const;

    void setPages(const PageList& pages);

private:
    SettingsCategory* findCategoryById(const Id& id);

    CategoryList m_categories;
    std::set<Id> m_pageIds;
    QIcon m_emptyIcon;
};
} // namespace Utils
