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

#include "gui/guiconstants.h"
#include "sortingmodel.h"

#include <core/coresettings.h>

#include <utils/settings/settingsmanager.h>

#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

namespace Fy::Gui::Settings {
class LibrarySortingPageWidget : public Utils::SettingsPageWidget
{
public:
    explicit LibrarySortingPageWidget(Core::Library::SortingRegistry* sortRegistry, Utils::SettingsManager* settings);

    void apply() override;

private:
    void addSorting() const;
    void removeSorting() const;

    Core::Library::SortingRegistry* m_sortRegistry;
    Utils::SettingsManager* m_settings;

    QTableView* m_sortList;
    SortingModel* m_model;
};

LibrarySortingPageWidget::LibrarySortingPageWidget(Core::Library::SortingRegistry* sortRegistry,
                                                   Utils::SettingsManager* settings)
    : m_sortRegistry{sortRegistry}
    , m_settings{settings}
    , m_sortList{new QTableView(this)}
    , m_model{new SortingModel(m_sortRegistry, this)}
{
    m_sortList->setModel(m_model);

    m_sortList->hideColumn(0);
    m_sortList->verticalHeader()->hide();
    m_sortList->horizontalHeader()->setStretchLastSection(true);
    m_sortList->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto* buttonsLayout = new QVBoxLayout();
    buttonsLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto* addButton    = new QPushButton("Add", this);
    auto* removeButton = new QPushButton("Remove", this);

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

void LibrarySortingPageWidget::addSorting() const
{
    m_model->addNewSortScript();
}

void LibrarySortingPageWidget::removeSorting() const
{
    const auto selectedIndexes = m_sortList->selectionModel()->selectedRows();
    for(const auto& index : selectedIndexes) {
        m_model->markForRemoval(index.data(Qt::UserRole).value<Core::Library::Sorting::SortScript>());
    }
}

LibrarySortingPage::LibrarySortingPage(Core::Library::SortingRegistry* sortRegistry, Utils::SettingsManager* settings)
    : Utils::SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::LibrarySorting);
    setName(tr("Sorting"));
    setCategory({"Library", "Sorting"});
    setWidgetCreator([sortRegistry, settings] {
        return new LibrarySortingPageWidget(sortRegistry, settings);
    });
}
} // namespace Fy::Gui::Settings
