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

#include <QActionGroup>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QScrollBar>
#include <QSortFilterProxyModel>

namespace Fy::Gui::Widgets {
PlaylistView::PlaylistView(QWidget* parent)
    : QTreeView(parent)
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

void PlaylistView::contextMenuEvent(QContextMenuEvent* e)
{
    Q_UNUSED(e)
}
} // namespace Fy::Gui::Widgets
