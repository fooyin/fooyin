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

#include "librarytreepage.h"

#include "gui/guiconstants.h"
#include "gui/guisettings.h"
#include "gui/librarytree/librarytreegroupregistry.h"
#include "gui/settings/librarytree/librarytreegroupmodel.h"

#include <utils/settings/settingsmanager.h>

#include <QHeaderView>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

namespace Fy::Gui::Settings {
class LibraryTreePageWidget : public Utils::SettingsPageWidget
{
public:
    explicit LibraryTreePageWidget(Widgets::LibraryTreeGroupRegistry* groupsRegistry);

    void apply() override;

private:
    void addGroup() const;
    void removeGroup() const;

    Widgets::LibraryTreeGroupRegistry* m_groupsRegistry;

    QTableView* m_groupList;
    LibraryTreeGroupModel* m_model;
};

LibraryTreePageWidget::LibraryTreePageWidget(Widgets::LibraryTreeGroupRegistry* groupsRegistry)
    : m_groupsRegistry{groupsRegistry}
    , m_groupList{new QTableView(this)}
    , m_model{new LibraryTreeGroupModel(m_groupsRegistry, this)}
{
    m_groupList->setModel(m_model);

    // Hide index column
    m_groupList->hideColumn(0);

    m_groupList->verticalHeader()->hide();
    m_groupList->horizontalHeader()->setStretchLastSection(true);
    m_groupList->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto* buttons       = new QWidget(this);
    auto* buttonsLayout = new QVBoxLayout(buttons);
    buttonsLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto* addButton    = new QPushButton("Add", this);
    auto* removeButton = new QPushButton("Remove", this);

    buttonsLayout->addWidget(addButton);
    buttonsLayout->addWidget(removeButton);

    auto* fieldLayout = new QHBoxLayout();
    fieldLayout->addWidget(m_groupList);
    fieldLayout->addWidget(buttons);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(fieldLayout);

    QObject::connect(addButton, &QPushButton::clicked, this, &LibraryTreePageWidget::addGroup);
    QObject::connect(removeButton, &QPushButton::clicked, this, &LibraryTreePageWidget::removeGroup);
}

void LibraryTreePageWidget::apply()
{
    m_model->processQueue();
}

void LibraryTreePageWidget::addGroup() const
{
    m_model->addNewGroup();
}

void LibraryTreePageWidget::removeGroup() const
{
    const auto selectedItems = m_groupList->selectionModel()->selectedRows();
    for(const auto& selected : selectedItems) {
        const auto* item = static_cast<LibraryTreeGroupItem*>(selected.internalPointer());
        m_model->markForRemoval(item->group());
    }
}

LibraryTreePage::LibraryTreePage(Widgets::LibraryTreeGroupRegistry* groupsRegistry, Utils::SettingsManager* settings)
    : Utils::SettingsPage{settings->settingsDialog()}
{
    setId(Constants::Page::WidgetsLibraryTree);
    setName(tr("Library Tree"));
    setCategory("Category.Widgets");
    setCategoryName(tr("Widgets"));
    setWidgetCreator([groupsRegistry] {
        return new LibraryTreePageWidget(groupsRegistry);
    });
    setCategoryIconPath(Constants::Icons::Category::Widgets);
}
} // namespace Fy::Gui::Settings
