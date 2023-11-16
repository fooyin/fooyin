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

#include "librarysortingpage.h"

#include "core/library/sortingregistry.h"
#include "gui/guiconstants.h"
#include "sortingmodel.h"

#include <core/coresettings.h>

#include <utils/settings/settingsmanager.h>

#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

namespace Fooyin {
class LibrarySortingPageWidget : public SettingsPageWidget
{
public:
    explicit LibrarySortingPageWidget(SortingRegistry* sortRegistry, SettingsManager* settings);

    void apply() override;
    void reset() override;

private:
    void addSorting() const;
    void removeSorting() const;

    SortingRegistry* m_sortRegistry;
    SettingsManager* m_settings;

    QTableView* m_sortList;
    SortingModel* m_model;
};

LibrarySortingPageWidget::LibrarySortingPageWidget(SortingRegistry* sortRegistry, SettingsManager* settings)
    : m_sortRegistry{sortRegistry}
    , m_settings{settings}
    , m_sortList{new QTableView(this)}
    , m_model{new SortingModel(m_sortRegistry, this)}
{
    m_model->populate();
    m_sortList->setModel(m_model);

    m_sortList->hideColumn(0);
    m_sortList->verticalHeader()->hide();
    m_sortList->horizontalHeader()->setStretchLastSection(true);
    m_sortList->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto* buttonsLayout = new QVBoxLayout();
    buttonsLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto* addButton    = new QPushButton(tr("Add"), this);
    auto* removeButton = new QPushButton(tr("Remove"), this);

    buttonsLayout->addWidget(addButton);
    buttonsLayout->addWidget(removeButton);
    buttonsLayout->addStretch();

    auto* mainLayout = new QGridLayout(this);
    mainLayout->addWidget(m_sortList, 0, 0, 1, 3);
    mainLayout->addLayout(buttonsLayout, 0, 3);
    mainLayout->setColumnStretch(2, 1);

    QObject::connect(addButton, &QPushButton::clicked, this, &LibrarySortingPageWidget::addSorting);
    QObject::connect(removeButton, &QPushButton::clicked, this, &LibrarySortingPageWidget::removeSorting);
}

void LibrarySortingPageWidget::apply()
{
    m_model->processQueue();
}

void LibrarySortingPageWidget::reset()
{
    m_settings->reset<Settings::Core::LibrarySorting>();
    m_sortRegistry->loadItems();
    m_model->populate();
}

void LibrarySortingPageWidget::addSorting() const
{
    m_model->addNewSortScript();
}

void LibrarySortingPageWidget::removeSorting() const
{
    const auto selectedIndexes = m_sortList->selectionModel()->selectedRows();
    for(const auto& index : selectedIndexes) {
        m_model->markForRemoval(index.data(Qt::UserRole).value<SortScript>());
    }
}

LibrarySortingPage::LibrarySortingPage(SortingRegistry* sortRegistry, SettingsManager* settings)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::LibrarySorting);
    setName(tr("Sorting"));
    setCategory({tr("Library"), tr("Sorting")});
    setWidgetCreator([sortRegistry, settings] { return new LibrarySortingPageWidget(sortRegistry, settings); });
}
} // namespace Fooyin
