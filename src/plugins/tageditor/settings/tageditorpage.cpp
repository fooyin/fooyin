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

#include "tageditorpage.h"

#include "settings/tageditorfieldregistry.h"
#include "settings/tageditorfieldsmodel.h"
#include "tageditorconstants.h"

#include <gui/widgets/checkboxdelegate.h>
#include <gui/widgets/extendabletableview.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>

namespace Fooyin::TagEditor {
class TagEditorFieldsPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit TagEditorFieldsPageWidget(TagEditorFieldRegistry* registry, ActionManager* actionManager);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void updateButtonState();

    ActionManager* m_actionManager;

    TagEditorFieldRegistry* m_registry;
    ExtendableTableView* m_fieldList;
    TagEditorFieldsModel* m_model;
};

TagEditorFieldsPageWidget::TagEditorFieldsPageWidget(TagEditorFieldRegistry* registry, ActionManager* actionManager)
    : m_actionManager{actionManager}
    , m_registry{registry}
    , m_fieldList{new ExtendableTableView(m_actionManager, this)}
    , m_model{new TagEditorFieldsModel(m_registry, this)}
{
    m_fieldList->setExtendableModel(m_model);
    m_fieldList->setTools(ExtendableTableView::Move);

    auto* checkDelegate = new CheckBoxDelegate(this);
    m_fieldList->setItemDelegateForColumn(3, checkDelegate);
    m_fieldList->setItemDelegateForColumn(4, checkDelegate);

    // Hide index column
    m_fieldList->hideColumn(0);

    m_fieldList->setExtendableColumn(1);
    m_fieldList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fieldList->verticalHeader()->hide();
    m_fieldList->horizontalHeader()->setStretchLastSection(false);
    m_fieldList->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_fieldList->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    auto* hintLabel = new QLabel(QStringLiteral("🛈 Multiple values can be specified using \";\""), this);

    auto* mainLayout = new QGridLayout(this);
    mainLayout->addWidget(m_fieldList, 0, 0, 1, 2);
    mainLayout->addWidget(hintLabel, 1, 0, 1, 2);
    mainLayout->setColumnStretch(1, 1);

    QObject::connect(m_fieldList->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &TagEditorFieldsPageWidget::updateButtonState);
    QObject::connect(m_model, &QAbstractItemModel::rowsMoved, this, &TagEditorFieldsPageWidget::updateButtonState);
}

void TagEditorFieldsPageWidget::load()
{
    m_model->populate();
    updateButtonState();
}

void TagEditorFieldsPageWidget::apply()
{
    m_model->processQueue();
}

void TagEditorFieldsPageWidget::reset()
{
    m_registry->reset();
    m_registry->loadDefaultFields();
}

void TagEditorFieldsPageWidget::updateButtonState()
{
    const auto selection = m_fieldList->selectionModel()->selectedIndexes();
    const bool isEmpty   = selection.empty();

    bool canMoveUp{false};
    bool canMoveDown{false};

    if(!isEmpty) {
        canMoveUp   = selection.front().row() > 0;
        canMoveDown = selection.back().row() < m_registry->count() - 1;
    }

    m_fieldList->removeRowAction()->setEnabled(!isEmpty);
    m_fieldList->moveUpAction()->setEnabled(!isEmpty && canMoveUp);
    m_fieldList->moveDownAction()->setEnabled(!isEmpty && canMoveDown);
}

TagEditorFieldsPage::TagEditorFieldsPage(TagEditorFieldRegistry* registry, ActionManager* actionManager,
                                         SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::TagEditorFields);
    setName(tr("Fields"));
    setCategory({tr("Tag Editor")});
    setWidgetCreator([registry, actionManager] { return new TagEditorFieldsPageWidget(registry, actionManager); });
}
} // namespace Fooyin::TagEditor

#include "moc_tageditorpage.cpp"
#include "tageditorpage.moc"
