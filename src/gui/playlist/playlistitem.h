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

#include <utils/treeitem.h>

#include <QObject>

namespace Fy {
namespace Core {
class Track;
class Album;
class Container;
} // namespace Core

namespace Gui::Widgets {
using ItemType = std::variant<Core::Track*, Core::Album*, Core::Container*>;

class PlaylistItem : public Utils::TreeItem<PlaylistItem>
{
public:
    enum Type
    {
        Root      = 0,
        Track     = 1,
        Container = 2,
        Album     = 3,
    };

    enum Role
    {
        Id        = Qt::UserRole + 6,
        Artist    = Qt::UserRole + 7,
        Date      = Qt::UserRole + 8,
        Duration  = Qt::UserRole + 9,
        Cover     = Qt::UserRole + 10,
        Number    = Qt::UserRole + 11,
        PlayCount = Qt::UserRole + 12,
        MultiDisk = Qt::UserRole + 13,
        Playing   = Qt::UserRole + 14,
        Path      = Qt::UserRole + 15,
        Data      = Qt::UserRole + 16,
    };

    explicit PlaylistItem(Type type = Type::Root, const ItemType& data = {}, PlaylistItem* parentItem = nullptr);

    void setKey(const QString& key);

    [[nodiscard]] ItemType data() const;
    [[nodiscard]] Type type();
    [[nodiscard]] QString key() const;

private:
    ItemType m_data;
    Type m_type;
    QString m_key;
};
} // namespace Gui::Widgets
} // namespace Fy
