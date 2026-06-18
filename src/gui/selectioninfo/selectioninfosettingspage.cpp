/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "selectioninfosettingspage.h"

#include "selectioninfofieldregistry.h"
#include "selectioninfofieldsmodel.h"

#include <gui/guiconstants.h>
#include <gui/widgets/checkboxdelegate.h>
#include <gui/widgets/extendabletableview.h>
#include <utils/settings/settingsmanager.h>

#include <QGridLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>

using namespace Qt::StringLiterals;

namespace Fooyin {
class SelectionInfoSettingsPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit SelectionInfoSettingsPageWidget(SelectionInfoFieldRegistry* registry);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void updateButtonState();

    SelectionInfoFieldRegistry* m_registry;
    ExtendableTableView* m_fieldList;
    SelectionInfoFieldsModel* m_model;
};

SelectionInfoSettingsPageWidget::SelectionInfoSettingsPageWidget(SelectionInfoFieldRegistry* registry)
    : m_registry{registry}
    , m_fieldList{new ExtendableTableView(this)}
    , m_model{new SelectionInfoFieldsModel(registry, this)}
{
    m_fieldList->setExtendableModel(m_model);
    m_fieldList->setTools(ExtendableTableView::Move);
    m_fieldList->setItemDelegateForColumn(1, new CheckBoxDelegate(this));
    m_fieldList->hideColumn(0);
    m_fieldList->setExtendableColumn(2);
    m_fieldList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fieldList->verticalHeader()->hide();
    m_fieldList->horizontalHeader()->setStretchLastSection(false);
    m_fieldList->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_fieldList->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_fieldList->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);

    auto* hint = new QLabel(
        u"🛈 "_s
            + tr("Enabled fields use the configured name and order. Other tags are shown as <TAG> when extended "
                 "metadata is enabled."),
        this);
    hint->setWordWrap(true);

    auto* layout = new QGridLayout(this);
    layout->addWidget(m_fieldList, 0, 0);
    layout->addWidget(hint, 1, 0);

    QObject::connect(m_fieldList->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &SelectionInfoSettingsPageWidget::updateButtonState);
    QObject::connect(m_model, &QAbstractItemModel::rowsMoved, this,
                     &SelectionInfoSettingsPageWidget::updateButtonState);
}

void SelectionInfoSettingsPageWidget::load()
{
    m_model->populate();
    updateButtonState();
}

void SelectionInfoSettingsPageWidget::apply()
{
    m_model->processQueue();
}

void SelectionInfoSettingsPageWidget::reset()
{
    m_registry->resetToDefaults();
    m_model->populate();
    updateButtonState();
}

void SelectionInfoSettingsPageWidget::updateButtonState()
{
    const auto selection   = m_fieldList->selectionModel()->selectedIndexes();
    const bool isEmpty     = selection.empty();
    const bool canMoveUp   = !isEmpty && selection.front().row() > 0;
    const bool canMoveDown = !isEmpty && selection.back().row() < m_model->rowCount({}) - 1;

    m_fieldList->removeRowAction()->setEnabled(!isEmpty);
    m_fieldList->moveUpAction()->setEnabled(canMoveUp);
    m_fieldList->moveDownAction()->setEnabled(canMoveDown);
}

SelectionInfoSettingsPage::SelectionInfoSettingsPage(SelectionInfoFieldRegistry* registry, SettingsManager* settings,
                                                     QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::InterfaceSelectionInfo);
    setName(tr("Selection Info"));
    setCategory({tr("Interface")});
    setRelativePosition(SettingsPageRelativePosition::After, Constants::Page::InterfaceTrackDisplay);
    setWidgetCreator([registry] { return new SelectionInfoSettingsPageWidget(registry); });
}
} // namespace Fooyin

#include "moc_selectioninfosettingspage.cpp"
#include "selectioninfosettingspage.moc"
