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

#include <utils/treeitem.h>

namespace Fooyin {
using Data = std::variant<PlaylistTrackItem, PlaylistContainerItem>;

class PlaylistItem : public TreeItem<PlaylistItem>
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
        Indentation,
        Index,
        BaseKey,
        MultiColumnMode,
        CellMargin,
    };

    enum class State
    {
        None,
        Update,
        Delete,
    };

    PlaylistItem();
    PlaylistItem(ItemType type, Data data, PlaylistItem* parentItem);

    [[nodiscard]] bool pending() const;
    [[nodiscard]] State state() const;
    [[nodiscard]] ItemType type() const;
    [[nodiscard]] Data& data() const;
    [[nodiscard]] QString baseKey() const;
    [[nodiscard]] QString key() const;
    [[nodiscard]] int indentation() const;
    [[nodiscard]] int index() const;

    void setPending(bool pending);
    void setState(State state);
    void setBaseKey(const QString& key);
    void setKey(const QString& key);
    void setIndentation(int indentation);
    void setIndex(int index);

    void appendChild(PlaylistItem* child) override;
    void insertChild(int row, PlaylistItem* child) override;
    void removeChild(int index) override;

private:
    bool m_pending;
    State m_state;
    ItemType m_type;
    mutable Data m_data;
    QString m_baseKey;
    QString m_key;
    int m_indentation;
    int m_index;
};
using PlaylistItemList = std::vector<PlaylistItem*>;
} // namespace Fooyin
