/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "filterexpansionmodel.h"

#include <gui/guiconstants.h>
#include <gui/iconloader.h>
#include <utils/datastream.h>

#include <QIODevice>
#include <QIcon>
#include <QMimeData>

namespace Fooyin::Filters {
QVariant ExpansionTrackModel::data(const QModelIndex& index, int role) const
{
    const auto* node = item(index);
    if(!node) {
        return {};
    }

    const auto& row = node->track >= 0     ? m_rows[node->track]
                    : node->subheader >= 0 ? m_subheaders[node->subheader].content
                                           : m_header;

    switch(role) {
        case Qt::DisplayRole:
        case Qt::ToolTipRole:
            return row.text;
        case Qt::SizeHintRole:
            return rowSize(row);
        case Qt::FontRole:
            return row.font ? QVariant::fromValue(*row.font) : QVariant{};
        case Qt::BackgroundRole:
            return row.background.isValid() ? QVariant::fromValue(row.background) : QVariant{};
        case LayoutRole:
            return QVariant::fromValue(row.layout);
        case PlayingIconRole:
            if(row.playingIcon == ScriptPlayingIcon::Play) {
                return Gui::iconFromTheme(Constants::Icons::Play);
            }
            if(row.playingIcon == ScriptPlayingIcon::Pause) {
                return Gui::iconFromTheme(Constants::Icons::Pause);
            }
            return {};
        case SubheaderRole:
            return node->subheader >= 0;
        case HeaderRole:
            return node->track < 0 && node->subheader < 0;
        case TrackIndexRole:
            return node->track;
        default:
            return {};
    }
}

Qt::ItemFlags ExpansionTrackModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);
    if(trackIndex(index) >= 0) {
        defaultFlags |= Qt::ItemIsDragEnabled | Qt::ItemNeverHasChildren;
    }
    else {
        defaultFlags &= ~Qt::ItemIsSelectable;
    }
    return defaultFlags;
}

QStringList ExpansionTrackModel::mimeTypes() const
{
    return {QString::fromLatin1(Constants::Mime::TrackIds)};
}

QMimeData* ExpansionTrackModel::mimeData(const QModelIndexList& indexes) const
{
    TrackIds ids;

    const auto selected = selectedTracks(indexes);
    for(const int track : selected) {
        ids.push_back(m_tracks.at(track).id());
    }

    QByteArray data;
    QDataStream stream{&data, QIODevice::WriteOnly};

    stream << ids;

    auto* mime = new QMimeData();
    mime->setData(QString::fromLatin1(Constants::Mime::TrackIds), data);
    return mime;
}

const TrackList& ExpansionTrackModel::tracks() const
{
    return m_tracks;
}

int ExpansionTrackModel::trackIndex(const QModelIndex& index) const
{
    const auto* node = item(index);
    return node ? node->track : -1;
}

int ExpansionTrackModel::firstTrack(const QModelIndex& index) const
{
    const auto* node = item(index);
    return node && node->trackCount > 0 ? node->firstTrack : -1;
}

QModelIndex ExpansionTrackModel::indexForTrack(int track) const
{
    return track >= 0 && std::cmp_less(track, m_trackItems.size()) ? indexOfItem(m_trackItems[track]) : QModelIndex{};
}

std::set<int> ExpansionTrackModel::selectedTracks(const QModelIndexList& indexes) const
{
    std::set<int> selected;
    for(const auto& index : indexes) {
        const auto* node = item(index);
        if(node) {
            for(int track = node->firstTrack; track < node->firstTrack + node->trackCount; ++track) {
                selected.emplace(track);
            }
        }
    }
    return selected;
}

bool ExpansionTrackModel::changesStructure(const ExpansionResult& result) const
{
    return (result.sections & ExpansionSubheaders)
        && !std::ranges::equal(m_subheaders, result.subheaders, [](const auto& previous, const auto& next) {
               return previous.firstTrack == next.firstTrack && previous.trackCount == next.trackCount;
           });
}

void ExpansionTrackModel::setContent(const ExpansionResult& result)
{
    beginResetModel();

    m_tracks = result.tracks ? *result.tracks : TrackList{};
    m_header = headerRow(result.header);

    m_rows.assign(m_tracks.size(), {});
    for(const auto& [track, row] : result.rows) {
        m_rows.at(track) = row;
    }

    m_subheaders = result.subheaders;
    rebuildItems();

    endResetModel();
}

bool ExpansionTrackModel::updateContent(const ExpansionResult& result)
{
    if(changesStructure(result)) {
        beginResetModel();

        for(const auto& [track, row] : result.rows) {
            m_rows.at(track) = row;
        }
        if(result.sections & ExpansionHeader) {
            m_header = headerRow(result.header);
        }
        if(result.sections & ExpansionSubheaders) {
            m_subheaders = result.subheaders;
        }

        rebuildItems();
        endResetModel();
        return true;
    }

    bool resized{false};
    for(const auto& [track, row] : result.rows) {
        resized |= updateRow(m_rows.at(track), row, indexForTrack(track));
    }
    if(result.sections & ExpansionHeader) {
        resized |= updateRow(m_header, headerRow(result.header), indexOfItem(m_headerItem));
    }
    if(result.sections & ExpansionSubheaders) {
        for(size_t i{0}; i < m_subheaders.size(); ++i) {
            resized
                |= updateRow(m_subheaders[i].content, result.subheaders[i].content, indexOfItem(m_subheaderItems[i]));
        }
    }

    return resized;
}

void ExpansionTrackModel::rebuildItems()
{
    rootItem()->clearChildren();

    m_items.clear();
    m_items.reserve(m_tracks.size() + m_subheaders.size() + 1);

    m_trackItems.resize(m_tracks.size());
    m_subheaderItems.assign(m_subheaders.size(), nullptr);
    m_headerItem = nullptr;

    if(!m_tracks.empty()) {
        m_headerItem = appendItem(rootItem(), -1, -1, 0, static_cast<int>(m_tracks.size()));
    }

    size_t header{0};
    auto* group{m_headerItem};
    int groupEnd{0};
    for(int track{0}; std::cmp_less(track, m_tracks.size()); ++track) {
        if(track >= groupEnd) {
            group = m_headerItem;
        }
        if(header < m_subheaders.size() && m_subheaders[header].firstTrack == track) {
            const auto& subheader = m_subheaders[header];
            group                 = appendItem(m_headerItem, -1, static_cast<int>(header), track, subheader.trackCount);
            groupEnd              = track + subheader.trackCount;
            m_subheaderItems[header] = group;
            ++header;
        }
        m_trackItems[track] = appendItem(group, track, -1, track, 1);
    }
}

const ExpansionTrackItem* ExpansionTrackModel::item(const QModelIndex& index) const
{
    if(!index.isValid()) {
        return nullptr;
    }
    return itemForIndex(index);
}

ExpansionTrackItem* ExpansionTrackModel::appendItem(ExpansionTrackItem* parent, int track, int subheader,
                                                    int firstTrack, int trackCount)
{
    auto& item      = m_items.emplace_back();
    item.track      = track;
    item.subheader  = subheader;
    item.firstTrack = firstTrack;
    item.trackCount = trackCount;
    parent->appendChild(&item);
    return &item;
}

bool ExpansionTrackModel::updateRow(ExpansionRow& previous, const ExpansionRow& row, const QModelIndex& index)
{
    const bool sameLayout
        = std::ranges::equal(previous.layout.lines, row.layout.lines, [](const auto& left, const auto& right) {
              return left.left == right.left && left.right == right.right && left.rule == right.rule
                  && left.height == right.height;
          });

    if(previous.text == row.text && previous.playingIcon == row.playingIcon && previous.height == row.height
       && previous.font == row.font && previous.background == row.background && sameLayout
       && rowSize(previous) == rowSize(row)) {
        return false;
    }

    const bool heightChanged = previous.height != row.height;
    const bool sizeChanged   = rowSize(previous) != rowSize(row);

    previous = row;

    if(index.isValid()) {
        QList<int> roles{Qt::DisplayRole,    Qt::ToolTipRole, Qt::FontRole,
                         Qt::BackgroundRole, LayoutRole,      PlayingIconRole};
        if(sizeChanged) {
            roles.push_back(Qt::SizeHintRole);
        }
        Q_EMIT dataChanged(index, index, roles);
    }

    return heightChanged;
}

QSize ExpansionTrackModel::rowSize(const ExpansionRow& row)
{
    int width = row.layout.width;
    for(const auto& line : row.layout.lines) {
        if(!line.left.empty()) {
            width = std::max(width, line.rightWidth * 2);
        }
    }
    return {width + 8, row.height};
}

ExpansionRow ExpansionTrackModel::headerRow(const RichTextLayout& layout)
{
    return {
        .text = richTextLayoutPlainText(layout), .layout = layout, .height = layout.height > 0 ? layout.height + 2 : 0};
}
} // namespace Fooyin::Filters
