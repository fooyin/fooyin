/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include "filterscolumnpage.h"

#include "filtercolumnregistry.h"
#include "filterconstants.h"
#include "filterscolumnmodel.h"

#include <utils/extendabletableview.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>
#include <QHeaderView>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

namespace Fooyin::Filters {
class FiltersColumnPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit FiltersColumnPageWidget(ActionManager* actionManager, FilterColumnRegistry* columnRegistry);

    void load() override;
    void apply() override;
    void reset() override;

private:
    ActionManager* m_actionManager;
    FilterColumnRegistry* m_columnsRegistry;

    ExtendableTableView* m_columnList;
    FiltersColumnModel* m_model;
};

FiltersColumnPageWidget::FiltersColumnPageWidget(ActionManager* actionManager, FilterColumnRegistry* columnRegistry)
    : m_actionManager{actionManager}
    , m_columnsRegistry{columnRegistry}
    , m_columnList{new ExtendableTableView(m_actionManager, this)}
    , m_model{new FiltersColumnModel(m_columnsRegistry, this)}
{
    m_columnList->setExtendableModel(m_model);

    // Hide index column
    m_columnList->hideColumn(0);

    m_columnList->setExtendableColumn(1);
    m_columnList->verticalHeader()->hide();
    m_columnList->horizontalHeader()->setStretchLastSection(true);
    m_columnList->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    auto updateButtonState = [this]() {
        const auto selection = m_columnList->selectionModel()->selectedIndexes();
        if(auto* remove = m_columnList->removeRowAction()) {
            remove->setDisabled(std::ranges::all_of(selection, [](const QModelIndex& index) {
                return index.data(Qt::UserRole).value<FilterColumn>().isDefault;
            }));
        }
    };

    updateButtonState();

    QObject::connect(m_columnList->selectionModel(), &QItemSelectionModel::selectionChanged, this, updateButtonState);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_columnList);
}

void FiltersColumnPageWidget::load()
{
    m_model->populate();
}

void FiltersColumnPageWidget::apply()
{
    m_model->processQueue();
}

void FiltersColumnPageWidget::reset()
{
    m_columnsRegistry->reset();
}

FiltersColumnPage::FiltersColumnPage(ActionManager* actionManager, FilterColumnRegistry* columnRegistry,
                                     SettingsManager* settings)
    : SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::FiltersFields);
    setName(tr("Columns"));
    setCategory({tr("Widgets"), tr("Filters")});
    setWidgetCreator(
        [actionManager, columnRegistry] { return new FiltersColumnPageWidget(actionManager, columnRegistry); });
}
} // namespace Fooyin::Filters

#include "filterscolumnpage.moc"
#include "moc_filterscolumnpage.cpp"
