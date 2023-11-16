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

#include "quicksetupdialog.h"

#include "quicksetupmodel.h"

#include <utils/paths.h>

#include <QDir>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace Fy::Gui {
QuickSetupDialog::QuickSetupDialog(LayoutProvider* layoutProvider, QWidget* parent)
    : QDialog{parent}
    , m_layout{new QVBoxLayout(this)}
    , m_layoutList{new QListView(this)}
    , m_model{new QuickSetupModel(layoutProvider, parent)}
    , m_accept{new QPushButton(tr("OK"), this)}
{
    setObjectName("Quick Setup");
    setWindowTitle(tr("Quick Setup"));

    setupUi();
    m_layoutList->setModel(m_model);

    QObject::connect(m_layoutList->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &QuickSetupDialog::changeLayout);
    QObject::connect(m_accept, &QPushButton::pressed, this, &QuickSetupDialog::close);
}

void QuickSetupDialog::setupUi()
{
    m_layoutList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_layout->addWidget(m_layoutList);
    m_layout->addWidget(m_accept);
}

void QuickSetupDialog::changeLayout(const QItemSelection& selected, const QItemSelection& /*deselected*/)
{
    if(selected.isEmpty()) {
        return;
    }
    const auto layout = selected.indexes().constFirst().data(QuickSetupModel::Layout).value<Layout>();
    emit layoutChanged(layout);
}

void QuickSetupDialog::showEvent(QShowEvent* event)
{
    // Centre to parent widget
    const QRect parentRect{parentWidget()->mapToGlobal(QPoint(0, 0)), parentWidget()->size()};
    move(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(), parentRect).topLeft());

    QDialog::showEvent(event);
}
} // namespace Fy::Gui

#include "moc_quicksetupdialog.cpp"
