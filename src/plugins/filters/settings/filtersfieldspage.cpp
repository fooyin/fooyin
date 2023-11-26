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

#include "filtersfieldspage.h"

#include "constants.h"
#include "fieldmodel.h"
#include "fieldregistry.h"
#include "filtersettings.h"

#include <utils/extendabletableview.h>
#include <utils/settings/settingsmanager.h>

#include <QHeaderView>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

namespace Fooyin::Filters {
class FiltersFieldsPageWidget : public SettingsPageWidget
{
public:
    explicit FiltersFieldsPageWidget(ActionManager* actionManager, FieldRegistry* fieldsRegistry,
                                     SettingsManager* settings);

    void apply() override;
    void reset() override;

private:
    ActionManager* m_actionManager;
    FieldRegistry* m_fieldsRegistry;
    SettingsManager* m_settings;

    ExtendableTableView* m_fieldList;
    FieldModel* m_model;
};

FiltersFieldsPageWidget::FiltersFieldsPageWidget(ActionManager* actionManager, FieldRegistry* fieldsRegistry,
                                                 SettingsManager* settings)
    : m_actionManager{actionManager}
    , m_fieldsRegistry{fieldsRegistry}
    , m_settings{settings}
    , m_fieldList{new ExtendableTableView(m_actionManager, this)}
    , m_model{new FieldModel(m_fieldsRegistry, this)}
{
    m_fieldList->setExtendableModel(m_model);

    // Hide index column
    m_fieldList->hideColumn(0);

    m_fieldList->setExtendableColumn(1);
    m_fieldList->verticalHeader()->hide();
    m_fieldList->horizontalHeader()->setStretchLastSection(true);
    m_fieldList->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_fieldList->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_fieldList);

    m_model->populate();
}

void FiltersFieldsPageWidget::apply()
{
    m_model->processQueue();
}

void FiltersFieldsPageWidget::reset()
{
    m_settings->reset<Settings::Filters::FilterFields>();
    m_fieldsRegistry->loadItems();
    m_model->populate();
}

FiltersFieldsPage::FiltersFieldsPage(ActionManager* actionManager, FieldRegistry* fieldsRegistry,
                                     SettingsManager* settings)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::FiltersFields);
    setName(tr("Fields"));
    setCategory({tr("Plugins"), tr("Filters")});
    setWidgetCreator([actionManager, fieldsRegistry, settings] {
        return new FiltersFieldsPageWidget(actionManager, fieldsRegistry, settings);
    });
}
} // namespace Fooyin::Filters
