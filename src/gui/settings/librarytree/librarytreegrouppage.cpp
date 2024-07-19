/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "librarytreegrouppage.h"

#include "internalguisettings.h"
#include "librarytree/librarytreegroupregistry.h"
#include "librarytreegroupmodel.h"

#include <gui/guiconstants.h>
#include <utils/multilinedelegate.h>

#include <QAction>
#include <QGridLayout>
#include <QHeaderView>

namespace Fooyin {
class LibraryTreeGroupPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit LibraryTreeGroupPageWidget(ActionManager* actionManager, LibraryTreeGroupRegistry* groupsRegistry);

    void load() override;
    void apply() override;
    void reset() override;

private:
    LibraryTreeGroupRegistry* m_groupsRegistry;

    ExtendableTableView* m_groupList;
    LibraryTreeGroupModel* m_model;
};

LibraryTreeGroupPageWidget::LibraryTreeGroupPageWidget(ActionManager* actionManager,
                                                       LibraryTreeGroupRegistry* groupsRegistry)
    : m_groupsRegistry{groupsRegistry}
    , m_groupList{new ExtendableTableView(actionManager, this)}
    , m_model{new LibraryTreeGroupModel(m_groupsRegistry, this)}
{
    m_groupList->setExtendableModel(m_model);

    m_groupList->setItemDelegateForColumn(2, new MultiLineEditDelegate(this));

    // Hide index column
    m_groupList->hideColumn(0);

    m_groupList->setExtendableColumn(1);
    m_groupList->verticalHeader()->hide();
    m_groupList->horizontalHeader()->setStretchLastSection(true);
    m_groupList->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_groupList->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto updateButtonState = [this]() {
        const auto selection = m_groupList->selectionModel()->selectedIndexes();
        if(auto* remove = m_groupList->removeRowAction()) {
            remove->setDisabled(std::ranges::all_of(selection, [](const QModelIndex& index) {
                return index.data(Qt::UserRole).value<LibraryTreeGrouping>().isDefault;
            }));
        }
    };

    updateButtonState();

    QObject::connect(m_groupList->selectionModel(), &QItemSelectionModel::selectionChanged, this, updateButtonState);

    auto* mainLayout = new QGridLayout(this);
    mainLayout->addWidget(m_groupList, 0, 0);
}

void LibraryTreeGroupPageWidget::load()
{
    m_model->populate();
}

void LibraryTreeGroupPageWidget::apply()
{
    m_model->processQueue();
}

void LibraryTreeGroupPageWidget::reset()
{
    m_groupsRegistry->reset();
}

LibraryTreeGroupPage::LibraryTreeGroupPage(ActionManager* actionManager, LibraryTreeGroupRegistry* groupsRegistry,
                                           SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::LibraryTreeGroups);
    setName(tr("Grouping"));
    setCategory({tr("Widgets"), tr("Library Tree")});
    setWidgetCreator(
        [actionManager, groupsRegistry] { return new LibraryTreeGroupPageWidget(actionManager, groupsRegistry); });
}
} // namespace Fooyin

#include "librarytreegrouppage.moc"
#include "moc_librarytreegrouppage.cpp"
