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

#include "configurablecontextmenupage.h"

#include "configurablecontextmenudelegate.h"

#include <algorithm>

#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QTreeView>

using namespace Qt::StringLiterals;

namespace Fooyin {
ConfigurableContextMenuWidget::ConfigurableContextMenuWidget(QString description,
                                                             ConfigurableContextMenuDefinition definition,
                                                             QWidget* parent)
    : SettingsPageWidget{}
    , m_description{std::move(description)}
    , m_definition{std::move(definition)}
    , m_model{new ConfigurableContextMenuModel(this)}
    , m_tree{new QTreeView(this)}
{
    if(parent) {
        setParent(parent);
    }

    auto* layout = new QGridLayout(this);

    m_model->setReorderingEnabled(m_definition.allowReordering);
    m_tree->setModel(m_model);
    m_tree->setItemDelegate(new ConfigurableContextMenuDelegate(m_tree));
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setUniformRowHeights(false);
    m_tree->header()->setStretchLastSection(true);
    m_tree->setDragEnabled(m_definition.allowReordering);
    m_tree->setAcceptDrops(m_definition.allowReordering);
    m_tree->setDropIndicatorShown(m_definition.allowReordering);
    m_tree->setDefaultDropAction(Qt::MoveAction);
    m_tree->setDragDropMode(m_definition.allowReordering ? QAbstractItemView::InternalMove
                                                         : QAbstractItemView::NoDragDrop);
    m_tree->setSelectionMode(m_definition.allowReordering ? QAbstractItemView::SingleSelection
                                                          : QAbstractItemView::ExtendedSelection);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);

    if(m_definition.allowReordering) {
        QObject::connect(m_tree, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
            auto* menu = new QMenu(m_tree);

            const QModelIndex index = m_tree->indexAt(pos);
            const bool isTopLevel   = index.isValid() && !index.parent().isValid();
            const bool isSeparator  = m_model->isSeparator(index);

            if(isTopLevel && !isSeparator) {
                if(m_model->canInsertSeparator(index.row())) {
                    menu->addAction(tr("Insert separator before"), menu,
                                    [this, row = index.row()] { m_model->insertSeparator(row); });
                }
                if(m_model->canInsertSeparator(index.row() + 1)) {
                    menu->addAction(tr("Insert separator after"), menu,
                                    [this, row = index.row() + 1] { m_model->insertSeparator(row); });
                }
            }

            if((!index.isValid() || isTopLevel) && m_model->canInsertSeparator(m_model->rowCount({}))) {
                menu->addAction(tr("Add separator"), menu, [this] { m_model->insertSeparator(m_model->rowCount({})); });
            }

            if(isTopLevel && isSeparator) {
                menu->addSeparator();
                menu->addAction(tr("Remove separator"), menu, [this, index] { m_model->removeSeparator(index); });
            }

            if(!menu->isEmpty()) {
                menu->popup(m_tree->viewport()->mapToGlobal(pos));
            }
        });
    }

    layout->addWidget(m_tree, 0, 0);
    layout->setRowStretch(0, 1);
    layout->setColumnStretch(0, 1);

    auto* descriptionLabel = new QLabel(u"🛈 "_s + m_description, this);
    descriptionLabel->setWordWrap(true);
    layout->addWidget(descriptionLabel, 1, 0);
}

void ConfigurableContextMenuWidget::load()
{
    const QStringList disabledIds   = m_definition.readDisabledIds ? m_definition.readDisabledIds() : QStringList{};
    const QStringList topLevelOrder = m_definition.readTopLevelOrder ? m_definition.readTopLevelOrder() : QStringList{};

    m_model->rebuild(m_definition.nodeFactory ? m_definition.nodeFactory() : ContextMenuNodeList{}, topLevelOrder);
    m_model->applyDisabledIds(disabledIds);
}

void ConfigurableContextMenuWidget::apply()
{
    if(m_definition.writeTopLevelOrder) {
        m_definition.writeTopLevelOrder(m_model->topLevelLayoutIds());
    }
    if(m_definition.writeDisabledIds) {
        m_definition.writeDisabledIds(m_model->disabledIds());
    }
}

void ConfigurableContextMenuWidget::reset()
{
    if(m_definition.writeTopLevelOrder) {
        m_definition.writeTopLevelOrder(m_definition.defaultTopLevelOrder);
    }
    if(m_definition.writeDisabledIds) {
        m_definition.writeDisabledIds(m_definition.defaultDisabledIds);
    }

    load();
}
} // namespace Fooyin
