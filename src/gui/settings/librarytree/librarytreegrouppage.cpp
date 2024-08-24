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
#include <gui/scripting/scripteditor.h>
#include <utils/multilinedelegate.h>

#include <QAction>
#include <QGridLayout>
#include <QHeaderView>
#include <QToolButton>

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
    void updateButtonState();

    LibraryTreeGroupRegistry* m_groupsRegistry;

    ExtendableTableView* m_groupList;
    LibraryTreeGroupModel* m_model;

    QToolButton* m_openEditor;
};

LibraryTreeGroupPageWidget::LibraryTreeGroupPageWidget(ActionManager* actionManager,
                                                       LibraryTreeGroupRegistry* groupsRegistry)
    : m_groupsRegistry{groupsRegistry}
    , m_groupList{new ExtendableTableView(actionManager, this)}
    , m_model{new LibraryTreeGroupModel(m_groupsRegistry, this)}
    , m_openEditor{new QToolButton(this)}
{
    m_groupList->setExtendableModel(m_model);

    m_groupList->setItemDelegateForColumn(2, new MultiLineEditDelegate(this));

    // Hide index column
    m_groupList->hideColumn(0);

    m_groupList->setExtendableColumn(1);
    m_groupList->verticalHeader()->hide();
    m_groupList->horizontalHeader()->setStretchLastSection(true);
    m_groupList->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    m_openEditor->setText(tr("Script Editor"));
    m_groupList->addCustomTool(m_openEditor);

    auto* mainLayout = new QGridLayout(this);
    mainLayout->addWidget(m_groupList, 0, 0);

    QObject::connect(m_groupList->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &LibraryTreeGroupPageWidget::updateButtonState);

    QObject::connect(m_openEditor, &QToolButton::clicked, this, [this]() {
        const auto selection    = m_groupList->selectionModel()->selectedIndexes();
        const QModelIndex index = selection.front();
        ScriptEditor::openEditor(index.data().toString(), [this, index](const QString& script) {
            m_model->setData(index, script, Qt::EditRole);
        });
    });
}

void LibraryTreeGroupPageWidget::load()
{
    m_model->populate();
    updateButtonState();
}

void LibraryTreeGroupPageWidget::apply()
{
    m_model->processQueue();
}

void LibraryTreeGroupPageWidget::reset()
{
    m_groupsRegistry->reset();
}

void LibraryTreeGroupPageWidget::updateButtonState()
{
    const auto selection  = m_groupList->selectionModel()->selectedIndexes();
    const bool allDefault = std::ranges::all_of(selection, [](const QModelIndex& index) {
        return index.data(Qt::UserRole).value<LibraryTreeGrouping>().isDefault;
    });

    if(selection.empty() || allDefault) {
        m_openEditor->setDisabled(true);
        m_groupList->removeRowAction()->setDisabled(true);
        return;
    }

    m_groupList->removeRowAction()->setEnabled(true);
    m_openEditor->setEnabled(selection.size() == 1 && selection.front().column() == 2);
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
