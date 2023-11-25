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

#include <utils/extendabletableview.h>

#include <gui/guiconstants.h>
#include <utils/actions/actionmanager.h>
#include <utils/actions/command.h>

#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QTextEdit>

constexpr auto ButtonText = "+ add new";

namespace Fooyin {
ExtendableTableView::ExtendableTableView(ActionManager* actionManager, QWidget* parent)
    : QTableView{parent}
    , m_actionManager{actionManager}
    , m_context{new WidgetContext(this, Context{"Context.ExtendableTableView"}, this)}
    , m_remove{new QAction(tr("Remove"), this)}
    , m_removeCommand{m_actionManager->registerAction(m_remove, Constants::Actions::Remove, m_context->context())}
    , m_mouseOverButton{false}
    , m_pendingRow{false}
{
    setMouseTracking(true);

    m_removeCommand->setDefaultShortcut(QKeySequence::Delete);
    m_actionManager->addContextObject(m_context);

    QObject::connect(m_remove, &QAction::triggered, this, [this]() {
        const QModelIndexList selected = selectionModel()->selectedIndexes();
        for(const QModelIndex& index : selected) {
            model()->removeRow(index.row());
        }
    });
}

void ExtendableTableView::rowAdded()
{
    m_pendingRow = false;
}

QAction* ExtendableTableView::removeAction() const
{
    return m_remove;
}

void ExtendableTableView::addContextActions(QMenu* /*menu*/) { }

void ExtendableTableView::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    addContextActions(menu);
    menu->addSeparator();
    menu->addAction(m_removeCommand->action());

    menu->popup(event->globalPos());
}

void ExtendableTableView::mouseMoveEvent(QMouseEvent* event)
{
    const bool isOverButton = m_buttonRect.contains(event->pos());

    if(isOverButton != m_mouseOverButton) {
        m_mouseOverButton = isOverButton;
        viewport()->update();
    }

    QTableView::mouseMoveEvent(event);
}

void ExtendableTableView::mousePressEvent(QMouseEvent* event)
{
    if(m_buttonRect.contains(event->pos())) {
        emit newRowClicked();
        m_pendingRow = true;
    }

    QTableView::mousePressEvent(event);
}

void ExtendableTableView::paintEvent(QPaintEvent* event)
{
    QTableView::paintEvent(event);

    if(m_pendingRow) {
        return;
    }

    QPainter painter{viewport()};

    const int lastRow    = model()->rowCount() - 1;
    const QRect lastRect = visualRect(model()->index(lastRow, 0));

    const QFontMetrics fontMetrics{font()};

    static const int buttonHeight = 15;
    static const int leftMargin   = 50;
    static const int topMargin    = 8;
    static const int padding      = 15;
    const int width               = fontMetrics.horizontalAdvance(ButtonText) + padding;

    m_buttonRect = {rect().left() + leftMargin, lastRect.bottom() + topMargin, width, buttonHeight};

    if(m_mouseOverButton) {
        painter.fillRect(m_buttonRect, palette().button());
        painter.drawRect(m_buttonRect);
    }
    painter.drawText(m_buttonRect, Qt::AlignCenter, ButtonText);
}
} // namespace Fooyin

#include "utils/moc_extendabletableview.cpp"
