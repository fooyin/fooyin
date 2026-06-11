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

#include "radioguideconfigdialog.h"

#include "radioguideconfigmodel.h"
#include "radioguidemodel.h"

#include <gui/guiutils.h>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QSignalBlocker>
#include <QTreeView>

using namespace Qt::StringLiterals;

namespace Fooyin::RadioBrowser {
namespace {
QString actionEntryKey(Action action)
{
    return u"action:%1"_s.arg(static_cast<int>(action));
}
} // namespace

RadioGuideConfigDialog::RadioGuideConfigDialog(RadioGuideWidget* guideWidget, QWidget* parent)
    : WidgetConfigDialog{guideWidget, tr("Radio Guide Settings"), parent}
    , m_tree{new QTreeView(this)}
    , m_model{new RadioGuideConfigModel(this)}
    , m_addSectionAction{new QAction(tr("Add Section"), this)}
    , m_addTagAction{new QAction(tr("Add Preset"), this)}
    , m_removeAction{new QAction(tr("Remove"), this)}
    , m_moveUpAction{new QAction(tr("Move Up"), this)}
    , m_moveDownAction{new QAction(tr("Move Down"), this)}
    , m_countries{new QCheckBox(tr("Show Countries"), this)}
    , m_startupEntry{new QComboBox(this)}
{
    resize(600, 500);

    m_tree->setModel(m_model);
    m_tree->setRootIsDecorated(true);
    m_tree->setItemsExpandable(true);
    m_tree->setAlternatingRowColors(false);
    m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setDragEnabled(true);
    m_tree->setAcceptDrops(true);
    m_tree->setDropIndicatorShown(true);
    m_tree->setDefaultDropAction(Qt::MoveAction);
    m_tree->setDragDropMode(QAbstractItemView::InternalMove);
    m_tree->expandAll();
    m_tree->header()->setStretchLastSection(true);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);

    auto* general       = new QGroupBox(tr("General"), this);
    auto* generalLayout = new QGridLayout(general);

    int row{0};
    generalLayout->addWidget(m_countries, row++, 0, 1, 2);
    generalLayout->addWidget(new QLabel(tr("Startup selection") + u":"_s, this), row, 0);
    generalLayout->addWidget(m_startupEntry, row++, 1);
    generalLayout->setColumnStretch(1, 1);

    auto* layout{contentLayout()};

    row = 0;
    layout->addWidget(general, row++, 0);
    layout->addWidget(Gui::createSectionHeader(tr("Tags"), this), row++, 0);
    layout->addWidget(m_tree, row++, 0);
    layout->setColumnStretch(0, 1);
    layout->setRowStretch(2, 1);

    QObject::connect(m_tree->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &RadioGuideConfigDialog::updateActionState);
    QObject::connect(m_tree, &QWidget::customContextMenuRequested, this, &RadioGuideConfigDialog::showContextMenu);
    QObject::connect(m_addSectionAction, &QAction::triggered, this, &RadioGuideConfigDialog::addSection);
    QObject::connect(m_addTagAction, &QAction::triggered, this, &RadioGuideConfigDialog::addTag);
    QObject::connect(m_removeAction, &QAction::triggered, this, [this]() {
        if(m_model->removeIndex(m_tree->currentIndex())) {
            updateActionState();
        }
    });
    QObject::connect(m_moveUpAction, &QAction::triggered, this, [this]() { moveCurrentRow(-1); });
    QObject::connect(m_moveDownAction, &QAction::triggered, this, [this]() { moveCurrentRow(1); });
    QObject::connect(m_model, &QAbstractItemModel::rowsMoved, this, &RadioGuideConfigDialog::updateActionState);

    loadCurrentConfig();
    updateActionState();
}

RadioGuideWidget::ConfigData RadioGuideConfigDialog::config() const
{
    return {.sections        = m_model->sections(),
            .showCountries   = m_countries->isChecked(),
            .startupEntryKey = m_startupEntry->currentData().toString()};
}

void RadioGuideConfigDialog::setConfig(const RadioGuideWidget::ConfigData& config)
{
    const QStringList expanded = expandedSections();

    m_model->setSections(config.sections);
    m_countries->setChecked(config.showCountries);
    populateStartupEntries(config.startupEntryKey);

    if(expanded.empty()) {
        m_tree->expandAll();
    }
    else {
        restoreExpandedSections(expanded);
    }

    updateActionState();
}

void RadioGuideConfigDialog::updateActionState()
{
    const QModelIndex index = m_tree->currentIndex();
    const bool isItem       = index.isValid();

    m_removeAction->setEnabled(isItem);
    m_moveUpAction->setEnabled(isItem && index.row() > 0);
    m_moveDownAction->setEnabled(isItem && index.row() < m_model->rowCount(index.parent()) - 1);
}

QModelIndex RadioGuideConfigDialog::currentSection() const
{
    QModelIndex index = m_tree->currentIndex();
    if(!index.isValid()) {
        return {};
    }
    if(index.parent().isValid()) {
        index = index.parent();
    }
    return index.siblingAtColumn(0);
}

QStringList RadioGuideConfigDialog::expandedSections() const
{
    QStringList sections;
    for(int row{0}; row < m_model->rowCount({}); ++row) {
        const QModelIndex index = m_model->index(row, 0, {});
        if(index.isValid() && m_tree->isExpanded(index)) {
            sections.push_back(index.data(Qt::DisplayRole).toString());
        }
    }
    return sections;
}

void RadioGuideConfigDialog::restoreExpandedSections(const QStringList& sections)
{
    for(int row{0}; row < m_model->rowCount({}); ++row) {
        const QModelIndex index = m_model->index(row, 0, {});
        if(index.isValid()) {
            m_tree->setExpanded(index, sections.contains(index.data(Qt::DisplayRole).toString()));
        }
    }
}

void RadioGuideConfigDialog::moveCurrentRow(const int offset)
{
    const QModelIndex index = m_tree->currentIndex();
    if(!index.isValid()) {
        return;
    }

    if(const QModelIndex moved = m_model->moveIndex(index, offset); moved.isValid()) {
        m_tree->setCurrentIndex(moved);
        updateActionState();
    }
}

void RadioGuideConfigDialog::showContextMenu(const QPoint& pos)
{
    const QModelIndex index = m_tree->indexAt(pos);
    if(index.isValid()) {
        m_tree->setCurrentIndex(index.siblingAtColumn(0));
    }

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    menu->addAction(m_addSectionAction);
    menu->addAction(m_addTagAction);
    menu->addAction(m_removeAction);
    menu->addSeparator();
    menu->addAction(m_moveUpAction);
    menu->addAction(m_moveDownAction);

    menu->popup(m_tree->viewport()->mapToGlobal(pos));
}

void RadioGuideConfigDialog::populateStartupEntries(const QString& currentKey)
{
    const QSignalBlocker blocker{m_startupEntry};
    m_startupEntry->clear();

    m_startupEntry->addItem(tr("Restore previous selection"), QString{});
    m_startupEntry->addItem(tr("Popular"), actionEntryKey(Action::Popular));
    m_startupEntry->addItem(tr("Trending"), actionEntryKey(Action::Trending));
    m_startupEntry->addItem(tr("Now Listening"), actionEntryKey(Action::NowListening));
    m_startupEntry->addItem(tr("Newest"), actionEntryKey(Action::Newest));
    m_startupEntry->addItem(tr("Random"), actionEntryKey(Action::Random));
    m_startupEntry->addItem(tr("My Stations"), actionEntryKey(Action::MyStations));
    m_startupEntry->addItem(tr("Latest Search"), actionEntryKey(Action::LatestSearch));

    const int index = m_startupEntry->findData(currentKey);
    m_startupEntry->setCurrentIndex(index >= 0 ? index : 0);
}

void RadioGuideConfigDialog::addSection()
{
    const QModelIndex current = m_tree->currentIndex();

    int row = m_model->rowCount({});
    if(current.isValid()) {
        if(current.parent().isValid()) {
            row = current.parent().row() + 1;
        }
        else {
            row = current.row() + 1;
        }
    }

    const QModelIndex index = m_model->addSection(tr("New Section"), row);
    m_tree->setCurrentIndex(index);
    m_tree->edit(index);
    updateActionState();
}

void RadioGuideConfigDialog::addTag()
{
    const QModelIndex current = m_tree->currentIndex();
    QModelIndex section       = currentSection();
    if(!section.isValid()) {
        section = m_model->addSection(tr("New Section"));
    }

    const int row = current.isValid() && current.parent().isValid() ? current.row() + 1 : m_model->rowCount(section);
    const QModelIndex index = m_model->addPreset(section, {.name = tr("New Preset"), .tag = QString{}}, row);
    m_tree->setCurrentIndex(index);
    m_tree->edit(index);
    updateActionState();
}
} // namespace Fooyin::RadioBrowser
