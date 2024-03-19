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
PlaylistContainerItem::PlaylistContainerItem()
    : m_rowHeight{0}
{ }

TrackList PlaylistContainerItem::tracks() const
{
    return m_tracks;
}

int PlaylistContainerItem::trackCount() const
{
    return static_cast<int>(m_tracks.size());
}

FormattedScript PlaylistContainerItem::title() const
{
    return m_title;
}

FormattedScript PlaylistContainerItem::subtitle() const
{
    return m_subtitle;
}

FormattedScript PlaylistContainerItem::sideText() const
{
    return m_sideText;
}

FormattedScript PlaylistContainerItem::info() const
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

void PlaylistContainerItem::updateGroupText(ScriptParser* parser, ScriptFormatter* formatter)
{
    if(m_tracks.empty()) {
        return;
    }

    if(!parser || !formatter) {
        return;
    }

    auto evaluateBlocks = [this, parser, formatter](FormattedScript& script) {
        script.text.clear();
        const auto evalScript = parser->evaluate(script.script, m_tracks);
        script.text           = formatter->evaluate(evalScript);
    };

    evaluateBlocks(m_title);
    evaluateBlocks(m_subtitle);
    evaluateBlocks(m_info);
    evaluateBlocks(m_sideText);
}

void PlaylistContainerItem::setTitle(const FormattedScript& title)
{
    m_title = title;
}

void PlaylistContainerItem::setSubtitle(const FormattedScript& subtitle)
{
    m_subtitle = subtitle;
}

void PlaylistContainerItem::setSideText(const FormattedScript& text)
{
    m_sideText = text;
}

void PlaylistContainerItem::setInfo(const FormattedScript& info)
{
    m_info = info;
}

void PlaylistContainerItem::setRowHeight(int height)
{
    m_rowHeight = height;
}

void PlaylistContainerItem::addTrack(const Track& track)
{
    m_tracks.emplace_back(track);
}

void PlaylistContainerItem::addTracks(const TrackList& tracks)
{
    std::ranges::copy(tracks, std::back_inserter(m_tracks));
}

void PlaylistContainerItem::clearTracks()
{
    m_tracks.clear();
}

void PlaylistContainerItem::calculateSize()
{
    if(m_rowHeight > 0) {
        m_size.setHeight(m_rowHeight);
        return;
    }

    QSize totalSize;

    auto addSize = [&totalSize](const FormattedScript& script, bool addToTotal = true) {
        QSize blockSize;
        for(const auto& title : script.text) {
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

    if(!m_title.text.empty()) {
        addSize(m_title);
    }

    QSize subtitleSize;

    if(!m_subtitle.text.empty()) {
        subtitleSize = addSize(m_subtitle, false);
    }

    if(!m_sideText.text.empty()) {
        QSize sideSize = addSize(m_sideText, false);
        subtitleSize.setWidth(subtitleSize.width() + sideSize.width());
        subtitleSize.setHeight(std::max(subtitleSize.height(), sideSize.height()));
    }

    totalSize.setWidth(totalSize.width() + subtitleSize.width());
    totalSize.setHeight(totalSize.height() + subtitleSize.height() + 4);

    if(!m_info.text.empty()) {
        addSize(m_info);
    }

    m_size = totalSize;
}

PlaylistTrackItem::PlaylistTrackItem(FormattedScript left, FormattedScript right, const Track& track)
    : m_left{std::move(left)}
    , m_right{std::move(right)}
    , m_track{track}
{ }

std::vector<FormattedScript> PlaylistTrackItem::columns() const
{
    return m_columns;
}

FormattedScript PlaylistTrackItem::column(int column) const
{
    if(column < 0 || std::cmp_greater_equal(column, m_columns.size())) {
        return {};
    }

    return m_columns.at(column);
}

PlaylistTrackItem::PlaylistTrackItem(std::vector<FormattedScript> columns, const Track& track)
    : m_columns{std::move(columns)}
    , m_track{track}
{ }

FormattedScript PlaylistTrackItem::left() const
{
    return m_left;
}

FormattedScript PlaylistTrackItem::right() const
{
    return m_right;
}

Track PlaylistTrackItem::track() const
{
    return m_track;
}
} // namespace Fooyin
