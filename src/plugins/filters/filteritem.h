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

#include <QVariant>

namespace Filters {
class FilterItem
{
public:
    FilterItem(int id = -1, QString name = "", FilterItem* parent = {});
    ~FilterItem();

    void appendChild(FilterItem* child);

    FilterItem* child(int number);
    [[nodiscard]] int childCount() const;
    static int columnCount();
    [[nodiscard]] QVariant data(int role = 0) const;
    [[nodiscard]] int row() const;
    [[nodiscard]] FilterItem* parentItem() const;

private:
    int m_id;
    QString m_name;
    FilterItem* m_parentItem;
    QList<FilterItem*> m_childItems;
};
}; // namespace Filters
