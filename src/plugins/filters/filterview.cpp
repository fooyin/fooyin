/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "filterview.h"

#include "core/player/playermanager.h"

#include <QActionGroup>
#include <QHeaderView>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>

namespace Filters {
FilterView::FilterView(PlayerManager* playerManager, QWidget* parent)
    : QTreeView(parent)
    , m_playerManager(playerManager)
{
    setObjectName("FilterView");
    setupView();
}

FilterView::~FilterView() = default;

void FilterView::setupView()
{
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setMouseTracking(true);
    setRootIsDecorated(false);
    setItemsExpandable(false);
    setIndentation(0);
    setExpandsOnDoubleClick(false);
    setWordWrap(true);
    setTextElideMode(Qt::ElideLeft);
    setSortingEnabled(false);
    header()->setSectionsClickable(true);
    header()->setContextMenuPolicy(Qt::CustomContextMenu);
}

void FilterView::mouseMoveEvent(QMouseEvent* e)
{
    QTreeView::mouseMoveEvent(e);
}

void FilterView::mousePressEvent(QMouseEvent* e)
{
    auto pressedKey = e->modifiers();

    if(e->button() == Qt::LeftButton && pressedKey == Qt::NoModifier) {
        selectionModel()->clear();
    }

    QTreeView::mousePressEvent(e);
}

void FilterView::keyPressEvent(QKeyEvent* e)
{
    const auto key = e->key();

    if(key == Qt::Key_Enter || key == Qt::Key_Return) {
        //        m_library->prepareTracks();
    }
    QTreeView::keyPressEvent(e);
}

void FilterView::contextMenuEvent(QContextMenuEvent* e)
{
    Q_UNUSED(e)
}
} // namespace Filters
