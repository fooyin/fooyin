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

#include <utils/settings/settingsmanager.h>

#include <QHeaderView>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

namespace Fy::Filters::Settings {
class FiltersFieldsPageWidget : public Utils::SettingsPageWidget
{
public:
    explicit FiltersFieldsPageWidget(FieldRegistry* fieldsRegistry);

    void apply() override;

private:
    void addField() const;
    void removeField() const;

    FieldRegistry* m_fieldsRegistry;

    QTableView* m_fieldList;
    FieldModel* m_model;
};

FiltersFieldsPageWidget::FiltersFieldsPageWidget(FieldRegistry* fieldsRegistry)
    : m_fieldsRegistry{fieldsRegistry}
    , m_fieldList{new QTableView(this)}
    , m_model{new FieldModel(m_fieldsRegistry, this)}
{
    m_fieldList->setModel(m_model);

    // Hide index column
    m_fieldList->hideColumn(0);

    m_fieldList->verticalHeader()->hide();
    m_fieldList->horizontalHeader()->setStretchLastSection(true);
    m_fieldList->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto* buttons       = new QWidget(this);
    auto* buttonsLayout = new QVBoxLayout(buttons);
    buttonsLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto* addButton    = new QPushButton("Add", this);
    auto* removeButton = new QPushButton("Remove", this);

    buttonsLayout->addWidget(addButton);
    buttonsLayout->addWidget(removeButton);

    auto* fieldLayout = new QHBoxLayout();
    fieldLayout->addWidget(m_fieldList);
    fieldLayout->addWidget(buttons);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(fieldLayout);

    connect(addButton, &QPushButton::clicked, this, [this]() {
        addField();
    });
    connect(removeButton, &QPushButton::clicked, this, [this]() {
        removeField();
    });
}

void FiltersFieldsPageWidget::apply()
{
    m_model->processQueue();
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

FiltersFieldsPage::FiltersFieldsPage(FieldRegistry* fieldsRegistry, Utils::SettingsManager* settings)
    : Utils::SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::FiltersFields);
    setName(tr("Fields"));
    setCategory("Category.Filters");
    setCategoryName(tr("Filters"));
    setWidgetCreator([fieldsRegistry] {
        return new FiltersFieldsPageWidget(fieldsRegistry);
    });
    setCategoryIconPath(Constants::Icons::Category::Filters);
}
} // namespace Fy::Filters::Settings
