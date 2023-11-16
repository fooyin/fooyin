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

#include <utils/settings/settingsmanager.h>

#include <QHeaderView>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

namespace Fooyin::Filters {
class FiltersFieldsPageWidget : public SettingsPageWidget
{
public:
    explicit FiltersFieldsPageWidget(FieldRegistry* fieldsRegistry, SettingsManager* settings);

    void apply() override;
    void reset() override;

private:
    void addField() const;
    void removeField() const;

    FieldRegistry* m_fieldsRegistry;
    SettingsManager* m_settings;

    QTableView* m_fieldList;
    FieldModel* m_model;
};

FiltersFieldsPageWidget::FiltersFieldsPageWidget(FieldRegistry* fieldsRegistry, SettingsManager* settings)
    : m_fieldsRegistry{fieldsRegistry}
    , m_settings{settings}
    , m_fieldList{new QTableView(this)}
    , m_model{new FieldModel(m_fieldsRegistry, this)}
{
    m_model->populate();
    m_fieldList->setModel(m_model);

    // Hide index column
    m_fieldList->hideColumn(0);

    m_fieldList->verticalHeader()->hide();
    m_fieldList->horizontalHeader()->setStretchLastSection(true);
    m_fieldList->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto* buttons       = new QWidget(this);
    auto* buttonsLayout = new QVBoxLayout(buttons);
    buttonsLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto* addButton    = new QPushButton(tr("Add"), this);
    auto* removeButton = new QPushButton(tr("Remove"), this);

    buttonsLayout->addWidget(addButton);
    buttonsLayout->addWidget(removeButton);

    auto* fieldLayout = new QHBoxLayout();
    fieldLayout->addWidget(m_fieldList);
    fieldLayout->addWidget(buttons);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(fieldLayout);

    QObject::connect(addButton, &QPushButton::clicked, this, &FiltersFieldsPageWidget::addField);
    QObject::connect(removeButton, &QPushButton::clicked, this, &FiltersFieldsPageWidget::removeField);
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

void FiltersFieldsPageWidget::addField() const
{
    m_model->addNewField();
}

void FiltersFieldsPageWidget::removeField() const
{
    const auto selectedItems = m_fieldList->selectionModel()->selectedRows();
    for(const auto& selected : selectedItems) {
        const auto* item = static_cast<FieldItem*>(selected.internalPointer());
        m_model->markForRemoval(item->field());
    }
}

FiltersFieldsPage::FiltersFieldsPage(FieldRegistry* fieldsRegistry, SettingsManager* settings)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::FiltersFields);
    setName(tr("Fields"));
    setCategory({tr("Plugins"), tr("Filters")});
    setWidgetCreator([fieldsRegistry, settings] { return new FiltersFieldsPageWidget(fieldsRegistry, settings); });
}
} // namespace Fooyin::Filters
