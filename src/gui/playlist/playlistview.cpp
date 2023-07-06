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

#include "playlistview.h"

#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>

namespace Fy::Gui::Widgets::Playlist {
PlaylistView::PlaylistView(QWidget* parent)
    : QTreeView{parent}
{
    setObjectName("PlaylistView");
    setupView();
}

void PlaylistView::setupView()
{
    setRootIsDecorated(false);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setMouseTracking(true);
    setItemsExpandable(false);
    setIndentation(0);
    setExpandsOnDoubleClick(false);
    setWordWrap(true);
    setTextElideMode(Qt::ElideLeft);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setSortingEnabled(false);
    header()->setSectionsClickable(true);
    header()->setContextMenuPolicy(Qt::CustomContextMenu);
}

void PlaylistView::drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const
{
    Q_UNUSED(painter)
    Q_UNUSED(rect)
    Q_UNUSED(index)
}

void PlaylistView::paintEvent(QPaintEvent* event)
{
    if(model() && model()->rowCount() > 0) {
        return QTreeView::paintEvent(event);
    }
    // Empty playlist
    QPainter painter{viewport()};
    const QString text{tr("Empty Playlist")};

    QRect textRect = painter.fontMetrics().boundingRect(text);
    textRect.moveCenter(viewport()->rect().center());
    painter.drawText(textRect, Qt::AlignCenter, text);
}
} // namespace Fy::Gui::Widgets::Playlist
