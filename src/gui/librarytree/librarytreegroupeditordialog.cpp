/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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

#include "librarytreegroupeditordialog.h"

#include "librarytree/librarytreegroupregistry.h"
#include "settings/librarytree/librarytreegroupmodel.h"

#include <gui/scripting/scripteditor.h>
#include <gui/widgets/extendabletableview.h>
#include <gui/widgets/multilinedelegate.h>

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace Fooyin {
class LibraryTreeGroupEditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LibraryTreeGroupEditorWidget(ActionManager* actionManager, LibraryTreeGroupRegistry* groupsRegistry,
                                          QWidget* parent = nullptr);

    void apply();
    void reset();

private:
    void updateButtonState();

    LibraryTreeGroupRegistry* m_groupsRegistry;
    ExtendableTableView* m_groupList;
    LibraryTreeGroupModel* m_model;
    QToolButton* m_openEditor;
};

LibraryTreeGroupEditorWidget::LibraryTreeGroupEditorWidget(ActionManager* actionManager,
                                                           LibraryTreeGroupRegistry* groupsRegistry, QWidget* parent)
    : QWidget{parent}
    , m_groupsRegistry{groupsRegistry}
    , m_groupList{new ExtendableTableView(actionManager, this)}
    , m_model{new LibraryTreeGroupModel(m_groupsRegistry, this)}
    , m_openEditor{new QToolButton(this)}
{
    m_groupList->setExtendableModel(m_model);
    m_groupList->setItemDelegateForColumn(2, new MultiLineEditDelegate(this));
    m_groupList->hideColumn(0);
    m_groupList->setExtendableColumn(1);
    m_groupList->verticalHeader()->hide();
    m_groupList->horizontalHeader()->setStretchLastSection(true);
    m_groupList->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    m_openEditor->setText(tr("Script Editor"));
    m_groupList->addCustomTool(m_openEditor);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_groupList);

    QObject::connect(m_groupList->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &LibraryTreeGroupEditorWidget::updateButtonState);

    QObject::connect(m_openEditor, &QToolButton::clicked, this, [this]() {
        const auto selection    = m_groupList->selectionModel()->selectedIndexes();
        const QModelIndex index = selection.front();
        ScriptEditor::openEditor(index.data().toString(), [this, index](const QString& script) {
            m_model->setData(index, script, Qt::EditRole);
        });
    });

    m_model->populate();
    updateButtonState();
}

void LibraryTreeGroupEditorWidget::apply()
{
    m_model->processQueue();
    m_model->populate();
    updateButtonState();
}

void LibraryTreeGroupEditorWidget::reset()
{
    m_groupsRegistry->reset();
    m_model->populate();
    updateButtonState();
}

void LibraryTreeGroupEditorWidget::updateButtonState()
{
    const auto selection = m_groupList->selectionModel()->selectedIndexes();
    if(selection.empty()) {
        m_openEditor->setDisabled(true);
        m_groupList->removeRowAction()->setDisabled(true);
        return;
    }

    const bool hasCustom = std::ranges::any_of(selection, [](const QModelIndex& index) {
        return !index.data(Qt::UserRole).value<LibraryTreeGrouping>().isDefault;
    });

    m_groupList->removeRowAction()->setEnabled(hasCustom);
    m_openEditor->setEnabled(selection.size() == 1 && selection.front().column() == 2);
}

LibraryTreeGroupEditorDialog::LibraryTreeGroupEditorDialog(ActionManager* actionManager,
                                                           LibraryTreeGroupRegistry* groupsRegistry, QWidget* parent)
    : QDialog{parent}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Manage Library Tree Groupings"));
    resize(700, 500);

    auto* editor = new LibraryTreeGroupEditorWidget(actionManager, groupsRegistry, this);
    auto* info   = new QLabel(tr("Grouping presets are shared across all widgets."), this);
    info->setWordWrap(true);

    auto* buttons
        = new QDialogButtonBox(QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Apply
                                   | QDialogButtonBox::StandardButton::Cancel | QDialogButtonBox::StandardButton::Reset,
                               this);
    if(auto* reset = buttons->button(QDialogButtonBox::StandardButton::Reset)) {
        reset->setText(tr("Restore Defaults"));
    }

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(info);
    layout->addWidget(editor);
    layout->addWidget(buttons);

    QObject::connect(buttons->button(QDialogButtonBox::StandardButton::Apply), &QPushButton::clicked, editor,
                     &LibraryTreeGroupEditorWidget::apply);
    QObject::connect(buttons->button(QDialogButtonBox::StandardButton::Reset), &QPushButton::clicked, editor,
                     &LibraryTreeGroupEditorWidget::reset);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, [this, editor]() {
        editor->apply();
        accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
} // namespace Fooyin

#include "librarytreegroupeditordialog.moc"
