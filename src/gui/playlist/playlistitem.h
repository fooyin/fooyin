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

#include "playlistitemmodels.h"

#include <core/models/trackfwd.h>

#include <utils/treeitem.h>
#include <utils/utils.h>

#include <QObject>

namespace Fy::Gui::Widgets::Playlist {
using Data = std::variant<Track, Container>;

class PlaylistItem : public Utils::TreeItem<PlaylistItem>
{
public:
    enum ItemType
    {
        Root = Qt::UserRole + 1,
        Header,
        Subheader,
        Track,
    };

    enum Role
    {
        Id = Qt::UserRole + 10,
        Title,
        Subtitle,
        Info,
        Left,
        Right,
        Simple,
        ShowCover,
        Cover,
        Playing,
        Path,
        ItemData,
        Type,
        Indentation
    };

    explicit PlaylistItem(ItemType type = Root, Data data = {}, PlaylistItem* parentItem = nullptr);

    [[nodiscard]] bool pending() const;
    [[nodiscard]] ItemType type() const;
    [[nodiscard]] Data& data() const;
    [[nodiscard]] QString key() const;
    [[nodiscard]] int indentation() const;

    void setPending(bool pending);
    void setKey(const QString& key);
    void setParent(PlaylistItem* parent);
    void setIndentation(int indentation);

    void resetChildren();

private:
    bool m_pending;
    ItemType m_type;
    mutable Data m_data;
    QString m_key;
    int m_indentation;
};
} // namespace Fy::Gui::Widgets::Playlist
