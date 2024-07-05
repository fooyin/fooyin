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

#include <QPainter>

namespace Fooyin {
PlaylistView::PlaylistView(QWidget* parent)
    : ExpandedTreeView{parent}
    , m_playlistLoaded{false}
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

void PlaylistView::playlistAboutToBeReset()
{
    m_playlistLoaded = false;
}

void PlaylistView::playlistReset()
{
    m_playlistLoaded = true;
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
} // namespace Fooyin

#include "moc_playlistview.cpp"
