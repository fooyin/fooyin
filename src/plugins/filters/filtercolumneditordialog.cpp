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

#include "filtercolumneditordialog.h"

#include "filtercolumnregistry.h"
#include "filterscolumnmodel.h"

#include <gui/scripting/scripteditor.h>
#include <gui/widgets/extendabletableview.h>

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace Fooyin::Filters {
class FilterColumnEditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FilterColumnEditorWidget(ActionManager* actionManager, FilterColumnRegistry* columnRegistry,
                                      QWidget* parent = nullptr);

    void apply();
    void reset();

private:
    void updateButtonState();

    FilterColumnRegistry* m_columnRegistry;
    ExtendableTableView* m_columnList;
    FiltersColumnModel* m_model;
    QToolButton* m_openEditor;
};

FilterColumnEditorWidget::FilterColumnEditorWidget(ActionManager* actionManager, FilterColumnRegistry* columnRegistry,
                                                   QWidget* parent)
    : QWidget{parent}
    , m_columnRegistry{columnRegistry}
    , m_columnList{new ExtendableTableView(actionManager, this)}
    , m_model{new FiltersColumnModel(m_columnRegistry, this)}
    , m_openEditor{new QToolButton(this)}
{
    m_columnList->setExtendableModel(m_model);
    m_columnList->hideColumn(0);
    m_columnList->setExtendableColumn(1);
    m_columnList->verticalHeader()->hide();
    m_columnList->horizontalHeader()->setStretchLastSection(true);
    m_columnList->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    m_openEditor->setText(tr("Script Editor"));
    m_columnList->addCustomTool(m_openEditor);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_columnList);

    QObject::connect(m_columnList->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &FilterColumnEditorWidget::updateButtonState);

    QObject::connect(m_openEditor, &QToolButton::clicked, this, [this]() {
        const auto selection    = m_columnList->selectionModel()->selectedIndexes();
        const QModelIndex index = selection.front();
        ScriptEditor::openEditor(index.data().toString(), [this, index](const QString& script) {
            m_model->setData(index, script, Qt::EditRole);
        });
    });

    m_model->populate();
    updateButtonState();
}

void FilterColumnEditorWidget::apply()
{
    m_model->processQueue();
    m_model->populate();
    updateButtonState();
}

void FilterColumnEditorWidget::reset()
{
    m_columnRegistry->reset();
    m_model->populate();
    updateButtonState();
}

void FilterColumnEditorWidget::updateButtonState()
{
    const auto selection = m_columnList->selectionModel()->selectedIndexes();
    if(selection.empty()) {
        m_openEditor->setDisabled(true);
        m_columnList->removeRowAction()->setDisabled(true);
        return;
    }

    const bool hasCustom = std::ranges::any_of(
        selection, [](const QModelIndex& index) { return !index.data(Qt::UserRole).value<FilterColumn>().isDefault; });

    m_columnList->removeRowAction()->setEnabled(hasCustom);
    m_openEditor->setEnabled(selection.size() == 1 && selection.front().column() == 2);
}

FilterColumnEditorDialog::FilterColumnEditorDialog(ActionManager* actionManager, FilterColumnRegistry* columnRegistry,
                                                   QWidget* parent)
    : QDialog{parent}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Manage Filter Columns"));
    resize(700, 500);

    auto* editor = new FilterColumnEditorWidget(actionManager, columnRegistry, this);
    auto* info   = new QLabel(tr("Column presets are shared across all widgets."), this);
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
                     &FilterColumnEditorWidget::apply);
    QObject::connect(buttons->button(QDialogButtonBox::StandardButton::Reset), &QPushButton::clicked, editor,
                     &FilterColumnEditorWidget::reset);
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, [this, editor]() {
        editor->apply();
        accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
} // namespace Fooyin::Filters

#include "filtercolumneditordialog.moc"
