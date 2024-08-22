/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

#include "playlistview.h"

#include "playlistmodel.h"

#include <utils/stardelegate.h>
#include <utils/stareditor.h>

#include <QHeaderView>
#include <QMouseEvent>
#include <QPainter>

namespace Fooyin {
PlaylistView::PlaylistView(QWidget* parent)
    : ExpandedTreeView{parent}
    , m_playlistLoaded{false}
    , m_starDelegate{nullptr}
    , m_ratingColumn{-1}
{
    setObjectName(QStringLiteral("PlaylistView"));

    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::MoveAction);
    setDropIndicatorShown(true);
    setTextElideMode(Qt::ElideRight);
    setSelectBeforeDrag(true);
    setUniformHeightRole(PlaylistItem::Type);
    viewport()->setAcceptDrops(true);
}

void PlaylistView::setupRatingDelegate()
{
    const int columnCount = header()->count();
    for(int column{0}; column < columnCount; ++column) {
        if(auto* starDelegate = qobject_cast<StarDelegate*>(itemDelegateForColumn(column))) {
            m_starDelegate = starDelegate;
            m_ratingColumn = column;
            setMouseTracking(true);
            return;
        }
    }

    m_starDelegate = nullptr;
    m_ratingColumn = -1;
    setMouseTracking(false);
}

void PlaylistView::playlistAboutToBeReset()
{
    m_playlistLoaded = false;
}

void PlaylistView::playlistReset()
{
    m_playlistLoaded = true;
}

bool PlaylistView::playlistLoaded() const
{
    return m_playlistLoaded;
}

void PlaylistView::mouseMoveEvent(QMouseEvent* event)
{
    if(m_starDelegate) {
        const QModelIndex index = indexAt(event->pos());
        if(index.isValid() && index.column() == m_ratingColumn) {
            ratingHoverIn(index, event->pos());
        }
        else if(m_starDelegate->hoveredIndex().isValid()) {
            ratingHoverOut();
        }
    }

    ExpandedTreeView::mouseMoveEvent(event);
}

void PlaylistView::mousePressEvent(QMouseEvent* event)
{
    if(editTriggers() & QAbstractItemView::NoEditTriggers || event->button() != Qt::LeftButton) {
        ExpandedTreeView::mousePressEvent(event);
        return;
    }

    const QModelIndex index = indexAt(event->pos());
    if(!index.isValid() || index.column() != m_ratingColumn) {
        ExpandedTreeView::mousePressEvent(event);
        return;
    }

    const auto starRating = qvariant_cast<StarRating>(index.data());
    const auto align      = static_cast<Qt::Alignment>(index.data(Qt::TextAlignmentRole).toInt());
    const auto rating     = StarEditor::ratingAtPosition(event->pos(), visualRect(index), starRating, align);

    if(selectedIndexes().contains(index)) {
        TrackList tracks;
        const QModelIndexList selected = selectionModel()->selectedRows();
        for(const QModelIndex& selectedIndex : selected) {
            if(selectedIndex.data(PlaylistItem::Type).toInt() == PlaylistItem::Track) {
                auto track = selectedIndex.data(PlaylistItem::ItemData).value<Track>();
                track.setRating(rating);
                tracks.push_back(track);
            }
        }
        emit tracksRated(tracks);
    }
    else {
        auto track = index.data(PlaylistItem::ItemData).value<Track>();
        track.setRating(rating);
        emit tracksRated({track});
    }

    ExpandedTreeView::mousePressEvent(event);
}

void PlaylistView::leaveEvent(QEvent* event)
{
    if(m_starDelegate && m_starDelegate->hoveredIndex().isValid()) {
        ratingHoverOut();
    }

    ExpandedTreeView::leaveEvent(event);
}

void PlaylistView::paintEvent(QPaintEvent* event)
{
    QPainter painter{viewport()};

    auto drawCentreText = [this, &painter](const QString& text) {
        QRect textRect = painter.fontMetrics().boundingRect(text);
        textRect.moveCenter(viewport()->rect().center());
        painter.drawText(textRect, Qt::AlignCenter, text);
    };

    if(auto* playlistModel = qobject_cast<PlaylistModel*>(model())) {
        if(playlistModel->haveTracks()) {
            if(playlistModel->playlistIsLoaded() || m_playlistLoaded) {
                ExpandedTreeView::paintEvent(event);
            }
            else {
                drawCentreText(tr("Loading Playlist…"));
            }
        }
        else {
            drawCentreText(tr("Empty Playlist"));
        }
    }
}

QAbstractItemView::DropIndicatorPosition PlaylistView::dropPosition(const QPoint& pos, const QRect& rect,
                                                                    const QModelIndex& index)
{
    DropIndicatorPosition dropPos{OnViewport};
    const int midpoint = static_cast<int>(std::round(static_cast<double>((rect.height())) / 2));
    const auto type    = index.data(PlaylistItem::Type).toInt();

    if(type == PlaylistItem::Subheader) {
        dropPos = OnItem;
    }
    else if(pos.y() - rect.top() < midpoint || type == PlaylistItem::Header) {
        dropPos = AboveItem;
    }
    else if(rect.bottom() - pos.y() < midpoint) {
        dropPos = BelowItem;
    }

    return dropPos;
}

void PlaylistView::ratingHoverIn(const QModelIndex& index, const QPoint& pos)
{
    if(editTriggers() & QAbstractItemView::NoEditTriggers) {
        return;
    }

    const QModelIndexList selected = selectedIndexes();
    const QModelIndex prevIndex    = m_starDelegate->hoveredIndex();
    m_starDelegate->setHoverIndex(index, pos, selected);
    setCursor(Qt::PointingHandCursor);

    update(prevIndex);
    update(index);

    for(const QModelIndex& selectedIndex : selected) {
        if(selectedIndex.column() == m_ratingColumn) {
            update(selectedIndex);
        }
    }
}

void PlaylistView::ratingHoverOut()
{
    if(editTriggers() & QAbstractItemView::NoEditTriggers) {
        return;
    }

    const QModelIndex prevIndex = m_starDelegate->hoveredIndex();
    m_starDelegate->setHoverIndex({});
    setCursor({});

    update(prevIndex);

    const QModelIndexList selected = selectedIndexes();
    for(const QModelIndex& selectedIndex : selected) {
        if(selectedIndex.column() == m_ratingColumn) {
            update(selectedIndex);
        }
    }
}
} // namespace Fooyin

#include "moc_playlistview.cpp"
