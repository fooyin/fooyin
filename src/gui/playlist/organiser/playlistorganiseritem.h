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

#include <QString>

namespace Fooyin {
class Playlist;

class PlaylistOrganiserItem : public TreeItem<PlaylistOrganiserItem>
{
public:
    enum Type
    {
        Root = 0,
        GroupItem,
        PlaylistItem
    };

    enum Data
    {
        ItemType = Qt::UserRole,
        PlaylistData,
    };

    PlaylistOrganiserItem();
    PlaylistOrganiserItem(QString title, PlaylistOrganiserItem* parent);
    PlaylistOrganiserItem(Playlist* playlist, PlaylistOrganiserItem* parent);

    Type type() const;
    QString title() const;
    Playlist* playlist() const;

    void setTitle(const QString& title);

private:
    Type m_type;
    QString m_title;
    Playlist* m_playlist;
};

} // namespace Fooyin
