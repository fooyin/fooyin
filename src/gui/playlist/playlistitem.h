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

#include <core/models/libraryitem.h>

#include <QList>

namespace Gui::Widgets {
class PlaylistItem
{
public:
    enum class Type : qint8
    {
        Track = 0,
        Disc,
        Container,
        Album,
        Root
    };

    enum Role
    {
        Id        = Qt::UserRole + 1,
        Artist    = Qt::UserRole + 2,
        Year      = Qt::UserRole + 3,
        Duration  = Qt::UserRole + 4,
        Cover     = Qt::UserRole + 5,
        Number    = Qt::UserRole + 6,
        PlayCount = Qt::UserRole + 7,
        MultiDisk = Qt::UserRole + 8,
        Playing   = Qt::UserRole + 9,
        Path      = Qt::UserRole + 10,
        Index     = Qt::UserRole + 11,
        Data      = Qt::UserRole + 12,
    };

    explicit PlaylistItem(Type type = Type::Root, Core::LibraryItem* data = {}, PlaylistItem* parentItem = nullptr);
    ~PlaylistItem() = default;

    void appendChild(PlaylistItem* child);
    void setIndex(int idx);

    PlaylistItem* child(int number);
    [[nodiscard]] int childCount() const;
    [[nodiscard]] int columnCount();
    [[nodiscard]] Core::LibraryItem* data() const;
    [[nodiscard]] Type type();
    [[nodiscard]] int index() const;
    [[nodiscard]] int row() const;
    [[nodiscard]] PlaylistItem* parent() const;

private:
    std::vector<PlaylistItem*> m_children;
    Core::LibraryItem* m_data;
    Type m_type;
    int m_index;
    PlaylistItem* m_parent;
};
} // namespace Gui::Widgets
