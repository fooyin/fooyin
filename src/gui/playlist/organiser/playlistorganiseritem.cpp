/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include "playlistorganiseritem.h"

#include <core/playlist/playlist.h>
#include <gui/scripting/richtextutils.h>

namespace Fooyin {
PlaylistOrganiserItem::PlaylistOrganiserItem()
    : TreeItem{nullptr}
    , m_type{Root}
    , m_playlist{nullptr}
{ }

PlaylistOrganiserItem::PlaylistOrganiserItem(QString title, PlaylistOrganiserItem* parent)
    : TreeItem{parent}
    , m_type{GroupItem}
    , m_title{std::move(title)}
    , m_playlist{nullptr}
{ }

PlaylistOrganiserItem::PlaylistOrganiserItem(Playlist* playlist, PlaylistOrganiserItem* parent)
    : TreeItem{parent}
    , m_type{PlaylistItem}
    , m_playlist{playlist}
{
    if(playlist) {
        m_title = playlist->name();
    }
}

PlaylistOrganiserItem::Type PlaylistOrganiserItem::type() const
{
    return m_type;
}

QString PlaylistOrganiserItem::title() const
{
    return m_title;
}

Playlist* PlaylistOrganiserItem::playlist() const
{
    return m_playlist;
}

void PlaylistOrganiserItem::setTitle(const QString& title)
{
    m_title = title;
    invalidateRichText();
}

bool PlaylistOrganiserItem::hasRichText() const
{
    return m_hasRichText;
}

const RichText& PlaylistOrganiserItem::richTitle() const
{
    return m_richTitle;
}

const RichText& PlaylistOrganiserItem::rightRichTitle() const
{
    return m_rightRichTitle;
}

void PlaylistOrganiserItem::setRichText(RichText text)
{
    m_richTitle      = trimRichText(richTextForAlignment(text, RichAlignment::Left));
    m_rightRichTitle = trimRichText(richTextForAlignment(text, RichAlignment::Right));
    m_hasRichText    = true;
}

void PlaylistOrganiserItem::invalidateRichText()
{
    m_hasRichText = false;
}
} // namespace Fooyin
