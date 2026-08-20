/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include "dirtree.h"

#include <gui/widgets/autoheaderview.h>

#include <QAction>
#include <QHeaderView>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>

#include <map>

namespace Fooyin {
DirTree::DirTree(QWidget* parent)
    : QTreeView{parent}
    , m_header{new AutoHeaderView(Qt::Horizontal, this)}
    , m_restoreSort{false}
{
    setHeader(m_header);
    setUniformRowHeights(true);
    setSelectionBehavior(SelectRows);
    setSelectionMode(ExtendedSelection);
    setDragEnabled(true);
    setDragDropMode(DragOnly);
    setDefaultDropAction(Qt::CopyAction);
    setHeaderHidden(false);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setTextElideMode(Qt::ElideRight);
    setAllColumnsShowFocus(true);

    m_header->setContextMenuPolicy(Qt::CustomContextMenu);
    m_header->setSectionsClickable(true);
    m_header->setSectionsMovable(true);

    QObject::connect(this, &QTreeView::expanded, this, &DirTree::resizeView);
    QObject::connect(this, &QTreeView::collapsed, this, &DirTree::resizeView);
    QObject::connect(m_header, &QHeaderView::customContextMenuRequested, this, &DirTree::showHeaderContextMenu);
    QObject::connect(m_header, &AutoHeaderView::stateRestored, this, [this]() {
        sortByColumn(m_header->sortIndicatorSection(), m_header->sortIndicatorOrder());
        resizeView();
    });
}

void DirTree::setModel(QAbstractItemModel* model)
{
    if(model) {
        QObject::connect(model, &QAbstractItemModel::columnsAboutToBeRemoved, this, &DirTree::preserveHeaderState);
        QObject::connect(model, &QAbstractItemModel::modelAboutToBeReset, this, &DirTree::preserveHeaderState);
        QObject::connect(model, &QAbstractItemModel::modelReset, this, &DirTree::restoreHeaderAfterModelReset,
                         Qt::QueuedConnection);
    }

    QTreeView::setModel(model);
}

void DirTree::initialiseHeader()
{
    static constexpr int NameColumn{0};
    static constexpr int SizeColumn{1};
    static constexpr int TypeColumn{2};
    static constexpr int ModifiedColumn{3};

    // Initialise proportional widths
    m_header->setStretchEnabled(true);

    m_header->setHeaderSectionHidden(NameColumn, false);
    m_header->setHeaderSectionHidden(SizeColumn, false);
    m_header->setHeaderSectionHidden(ModifiedColumn, false);
    m_header->setHeaderSectionHidden(TypeColumn, true);
    m_header->setHeaderSectionWidths(
        {{NameColumn, 0.45}, {SizeColumn, 0.12}, {TypeColumn, 0.18}, {ModifiedColumn, 0.25}});

    setSortingEnabled(true);
    sortByColumn(NameColumn, Qt::AscendingOrder);
    resizeView();
}

void DirTree::resizeView()
{
    if(!m_header->isStretchEnabled()) {
        m_header->resizeColumnToContents(0);
    }
}

bool DirTree::showHeader() const
{
    return !isHeaderHidden();
}

void DirTree::setShowHeader(bool show)
{
    if(showHeader() == show) {
        return;
    }

    setHeaderHidden(!show);
    Q_EMIT headerVisibilityChanged(show);
}

QByteArray DirTree::saveHeaderState() const
{
    return m_header->saveHeaderState();
}

void DirTree::restoreHeaderState(const QByteArray& state)
{
    // Restored layout state supersedes state captured for a model reset
    m_pendingHeaderState.clear();
    m_header->restoreHeaderState(state, m_restoreSort);
}

void DirTree::preserveHeaderState()
{
    if(m_pendingHeaderState.isEmpty()) {
        m_pendingHeaderState = m_header->saveHeaderState();
    }
}

void DirTree::setRestoreSortEnabled(bool enabled)
{
    m_restoreSort = enabled;
}

void DirTree::restoreHeaderAfterModelReset()
{
    if(!m_pendingHeaderState.isEmpty()) {
        const QByteArray state = std::exchange(m_pendingHeaderState, {});
        restoreHeaderState(state);
    }
}

void DirTree::setColumnVisible(int column, bool visible)
{
    if(!visible) {
        m_header->hideHeaderSection(column);
        return;
    }

    std::map<int, int> visibleWidths;
    int totalWidth{0};

    for(int visual{0}; visual < m_header->count(); ++visual) {
        const int logical = m_header->logicalIndex(visual);
        if(logical != column && !m_header->isSectionHidden(logical)) {
            const int width        = m_header->sectionSize(logical);
            visibleWidths[logical] = width;
            totalWidth += width;
        }
    }

    m_header->showHeaderSection(column);
    m_header->moveSection(m_header->visualIndex(column), m_header->count() - 1);

    if(m_header->isStretchEnabled()) {
        const auto visibleSections = static_cast<double>(visibleWidths.size() + 1);
        const double newWidth      = 1.0 / visibleSections;
        const double oldWidth      = 1.0 - newWidth;

        std::map<int, double> widths;
        for(const auto& [logical, width] : visibleWidths) {
            widths[logical] = totalWidth > 0 ? oldWidth * (static_cast<double>(width) / totalWidth)
                                             : oldWidth / static_cast<double>(visibleWidths.size());
        }
        widths[column] = newWidth;
        m_header->setHeaderSectionWidths(widths);
    }
    else {
        m_header->resizeColumnToContents(column);
    }
}

void DirTree::showHeaderContextMenu(const QPoint& pos)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* columnsMenu = menu->addMenu(tr("Columns"));
    for(int column{1}; column < m_header->count(); ++column) {
        auto* action = columnsMenu->addAction(model()->headerData(column, Qt::Horizontal).toString());
        action->setCheckable(true);
        action->setChecked(!m_header->isSectionHidden(column));
        QObject::connect(action, &QAction::toggled, this,
                         [this, column](bool visible) { setColumnVisible(column, visible); });
    }

    menu->addSeparator();

    auto* showHeaderAction = menu->addAction(tr("Show header"));
    showHeaderAction->setCheckable(true);
    showHeaderAction->setChecked(showHeader());
    QObject::connect(showHeaderAction, &QAction::toggled, this, &DirTree::setShowHeader);

    auto* autoSize = menu->addAction(tr("Auto-size sections"));
    autoSize->setCheckable(true);
    autoSize->setChecked(m_header->isStretchEnabled());
    QObject::connect(autoSize, &QAction::toggled, m_header, &AutoHeaderView::setStretchEnabled);

    menu->addSeparator();
    m_header->addHeaderAlignmentMenu(menu, m_header->mapToGlobal(pos));

    menu->popup(header()->mapToGlobal(pos));
}

void DirTree::resizeEvent(QResizeEvent* event)
{
    QTreeView::resizeEvent(event);
    resizeView();
}

void DirTree::mousePressEvent(QMouseEvent* event)
{
    const auto button = event->button();

    if(button == Qt::ForwardButton) {
        Q_EMIT forwardClicked();
    }
    else if(button == Qt::BackButton) {
        Q_EMIT backClicked();
    }
    else if(button == Qt::MiddleButton) {
        QTreeView::mousePressEvent(event);
        Q_EMIT middleClicked();
    }
    else {
        QTreeView::mousePressEvent(event);
    }
}

void DirTree::mouseDoubleClickEvent(QMouseEvent* event)
{
    if(event->button() != Qt::LeftButton) {
        return;
    }

    QTreeView::mouseDoubleClickEvent(event);
}
} // namespace Fooyin

#include "moc_dirtree.cpp"
