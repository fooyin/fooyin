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
#include "tageditorsettings.h"

#include <gui/widgets/checkboxdelegate.h>
#include <gui/widgets/extendabletableview.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>

using namespace Qt::StringLiterals;

namespace Fooyin::TagEditor {
class TagEditorFieldsPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit TagEditorFieldsPageWidget(TagEditorFieldRegistry* registry, SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void updateButtonState();
    void updateHint();

    TagEditorFieldRegistry* m_registry;
    SettingsManager* m_settings;
    ExtendableTableView* m_fieldList;
    TagEditorFieldsModel* m_model;
    QLabel* m_hintLabel;
};

TagEditorFieldsPageWidget::TagEditorFieldsPageWidget(TagEditorFieldRegistry* registry, SettingsManager* settings)
    : m_registry{registry}
    , m_settings{settings}
    , m_fieldList{new ExtendableTableView(this)}
    , m_model{new TagEditorFieldsModel(m_registry, this)}
    , m_hintLabel{new QLabel(this)}
{
    m_fieldList->setExtendableModel(m_model);
    m_fieldList->setTools(ExtendableTableView::Move);

    auto* checkDelegate = new CheckBoxDelegate(this);
    m_fieldList->setItemDelegateForColumn(1, checkDelegate);
    m_fieldList->setItemDelegateForColumn(4, checkDelegate);
    m_fieldList->setItemDelegateForColumn(5, checkDelegate);

    // Hide index column
    m_fieldList->hideColumn(0);

    m_fieldList->setExtendableColumn(2);
    m_fieldList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fieldList->verticalHeader()->hide();
    m_fieldList->horizontalHeader()->setStretchLastSection(false);
    m_fieldList->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_fieldList->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    auto* mainLayout = new QGridLayout(this);
    mainLayout->addWidget(m_fieldList, 0, 0, 1, 2);
    mainLayout->addWidget(m_hintLabel, 1, 0, 1, 2);
    mainLayout->setColumnStretch(1, 1);

    QObject::connect(m_fieldList->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &TagEditorFieldsPageWidget::updateButtonState);
    QObject::connect(m_model, &QAbstractItemModel::rowsMoved, this, &TagEditorFieldsPageWidget::updateButtonState);
}

void TagEditorFieldsPageWidget::load()
{
    m_model->populate();
    updateHint();
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
    updateHint();
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

void TagEditorFieldsPageWidget::updateHint()
{
    const QStringList separators = multiValueSeparators(*m_settings);
    m_hintLabel->setText(u"🛈 "_s
                         + tr("Use any of \"%1\" in the editor to enter multiple values.").arg(separators.join(u' ')));
}

TagEditorFieldsPage::TagEditorFieldsPage(TagEditorFieldRegistry* registry, SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::TagEditorFields);
    setName(tr("Fields"));
    setCategory({tr("Tagging"), tr("Tag Editor")});
    setWidgetCreator([registry, settings] { return new TagEditorFieldsPageWidget(registry, settings); });
}
} // namespace Fooyin::TagEditor

#include "moc_tageditorpage.cpp"
#include "tageditorpage.moc"
