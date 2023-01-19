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

#include <QList>

namespace Gui::Widgets {
class InfoItem
{
public:
    enum class Type : qint8
    {
        Header = 0,
        Entry,
    };

    explicit InfoItem(Type type = Type::Header, QString title = {});
    ~InfoItem();

    void appendChild(InfoItem* child);

    [[nodiscard]] InfoItem* child(int number);
    [[nodiscard]] int childCount() const;
    [[nodiscard]] static int columnCount();
    [[nodiscard]] QString data() const;
    [[nodiscard]] Type type();
    [[nodiscard]] int row() const;
    [[nodiscard]] InfoItem* parent() const;

private:
    QList<InfoItem*> m_children;
    Type m_type;
    QString m_title;
    InfoItem* m_parent;
};
} // namespace Gui::Widgets
