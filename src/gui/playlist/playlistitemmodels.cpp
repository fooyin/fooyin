/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "playlistitemmodels.h"

#include <QFontMetrics>

namespace Fooyin {
PlaylistContainerItem::PlaylistContainerItem(bool isSimple)
    : m_simple{isSimple}
    , m_rowHeight{0}
{ }

const RichText& PlaylistContainerItem::title() const
{
    return m_title;
}

const RichText& PlaylistContainerItem::subtitle() const
{
    return m_subtitle;
}

const RichText& PlaylistContainerItem::sideText() const
{
    return m_sideText;
}

const RichText& PlaylistContainerItem::info() const
{
    return m_info;
}

int PlaylistContainerItem::rowHeight() const
{
    return m_rowHeight;
}

QSize PlaylistContainerItem::size() const
{
    return m_size;
}

int PlaylistContainerItem::scriptIndex() const
{
    return m_scriptIndex;
}

const std::optional<Track>& PlaylistContainerItem::coverTrack() const
{
    return m_coverTrack;
}

void PlaylistContainerItem::setTitle(const RichText& title)
{
    m_title = title;
}

void PlaylistContainerItem::setSubtitle(const RichText& subtitle)
{
    m_subtitle = subtitle;
}

void PlaylistContainerItem::setSideText(const RichText& text)
{
    m_sideText = text;
}

void PlaylistContainerItem::setInfo(const RichText& info)
{
    m_info = info;
}

void PlaylistContainerItem::setRowHeight(int height)
{
    m_rowHeight = height;
}

void PlaylistContainerItem::setScriptIndex(int index)
{
    m_scriptIndex = index;
}

void PlaylistContainerItem::setCoverTrack(const Track& track)
{
    m_coverTrack = track;
}

void PlaylistContainerItem::clearCoverTrack()
{
    m_coverTrack.reset();
}

void PlaylistContainerItem::calculateSize()
{
    if(m_rowHeight > 0) {
        m_size.setHeight(m_rowHeight);
        return;
    }

    QSize totalSize;

    auto addSize = [&totalSize](const RichText& text, bool addToTotal = true) {
        QSize blockSize;
        for(const auto& title : text.blocks) {
            const QFontMetrics fm{title.format.font};
            const QRect br = fm.boundingRect(title.text);
            blockSize.setWidth(blockSize.width() + br.width());
            blockSize.setHeight(std::max(blockSize.height(), br.height()));
        }
        if(addToTotal) {
            totalSize.setWidth(totalSize.width() + blockSize.width());
            totalSize.setHeight(totalSize.height() + blockSize.height() + 4);
        }
        return blockSize;
    };

    if(!m_title.empty()) {
        addSize(m_title);
    }

    QSize subtitleSize;

    if(!m_subtitle.empty()) {
        subtitleSize = addSize(m_subtitle, false);
    }

    if(!m_sideText.empty()) {
        const QSize sideSize = addSize(m_sideText, false);
        subtitleSize.setWidth(subtitleSize.width() + sideSize.width());
        subtitleSize.setHeight(std::max(subtitleSize.height(), sideSize.height()));
    }

    if(m_simple) {
        totalSize.setWidth(totalSize.width() + subtitleSize.width());
        totalSize.setHeight(std::max(totalSize.height(), subtitleSize.height()));
    }
    else {
        totalSize.setWidth(totalSize.width() + subtitleSize.width());
        totalSize.setHeight(totalSize.height() + subtitleSize.height() + 4);

        if(!m_info.empty()) {
            addSize(m_info);
        }
    }

    m_size = totalSize;
}

PlaylistTrackItem::PlaylistTrackItem(std::vector<RichText> columns, const PlaylistTrack& track)
    : m_columns{std::move(columns)}
    , m_track{track}
    , m_rowHeight{0}
{ }

PlaylistTrackItem::PlaylistTrackItem(RichText left, RichText right, const PlaylistTrack& track)
    : m_left{std::move(left)}
    , m_right{std::move(right)}
    , m_track{track}
    , m_rowHeight{0}
    , m_depth{0}
{ }

const std::vector<RichText>& PlaylistTrackItem::columns() const
{
    return m_columns;
}

const RichText& PlaylistTrackItem::column(int column) const
{
    static const RichText EmptyText;

    if(column < 0 || std::cmp_greater_equal(column, m_columns.size())) {
        return EmptyText;
    }

    return m_columns.at(column);
}

const RichText& PlaylistTrackItem::left() const
{
    return m_left;
}

const RichText& PlaylistTrackItem::right() const
{
    return m_right;
}

const PlaylistTrack& PlaylistTrackItem::track() const
{
    return m_track;
}

int PlaylistTrackItem::index() const
{
    return m_track.indexInPlaylist;
}

int PlaylistTrackItem::rowHeight() const
{
    return m_rowHeight;
}

int PlaylistTrackItem::depth() const
{
    return m_depth;
}

QSize PlaylistTrackItem::size(int column) const
{
    auto calculateScriptWidth = [](const RichText& richText) {
        QSize blockSize;
        for(const auto& [blockText, format] : richText.blocks) {
            const QFontMetrics fm{format.font};
            const QRect br = fm.boundingRect(blockText);
            blockSize.setWidth(blockSize.width() + br.width());
        }

        return blockSize;
    };

    if(column < 0) {
        return {};
    }

    if(!m_columns.empty()) {
        if(std::cmp_greater_equal(column, m_columns.size())) {
            return {};
        }

        if(std::cmp_less(m_sizes.size(), m_columns.size())) {
            m_sizes.resize(m_columns.size());
        }

        QSize& cachedSize = m_sizes[column];
        if(!cachedSize.isValid()) {
            cachedSize = calculateScriptWidth(m_columns.at(column));
            if(m_rowHeight > 0) {
                cachedSize.setHeight(m_rowHeight);
            }
        }
        return cachedSize;
    }

    if(column > 0) {
        return {};
    }

    if(m_sizes.empty()) {
        m_sizes.resize(1);
    }

    QSize& cachedSize = m_sizes.front();
    if(!cachedSize.isValid()) {
        if(!m_left.empty()) {
            cachedSize = calculateScriptWidth(m_left);
        }

        if(!m_right.empty()) {
            const QSize rightSize = calculateScriptWidth(m_right);
            cachedSize.setWidth(cachedSize.width() + rightSize.width());
        }

        if(m_rowHeight > 0) {
            cachedSize.setHeight(m_rowHeight);
        }
    }

    return cachedSize;
}

void PlaylistTrackItem::setColumns(const std::vector<RichText>& columns)
{
    m_columns = columns;
    m_sizes.clear();
}

void PlaylistTrackItem::setLeftRight(const RichText& left, const RichText& right)
{
    m_left  = left;
    m_right = right;
    m_sizes.clear();
}

void PlaylistTrackItem::setTrack(const PlaylistTrack& track)
{
    m_track = track;
}

void PlaylistTrackItem::setIndex(int index)
{
    m_track.indexInPlaylist = index;
}

void PlaylistTrackItem::setRowHeight(int height)
{
    m_rowHeight = height;
    m_sizes.clear();
}

void PlaylistTrackItem::setDepth(int depth)
{
    m_depth = depth;
}

void PlaylistTrackItem::removeColumn(int column)
{
    if(column < 0 || std::cmp_greater_equal(column, m_columns.size())) {
        return;
    }

    m_columns.erase(m_columns.cbegin() + column);
    m_sizes.clear();
}

void PlaylistTrackItem::calculateSize()
{
    m_sizes.clear();
}
} // namespace Fooyin
