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

#include "libraryfilterpage.h"

#include "../../filters/filterconstants.h"
#include "libraryfiltermodel.h"

#include <core/library/libraryfilterregistry.h>
#include <gui/scripting/scripteditor.h>
#include <gui/widgets/checkboxdelegate.h>
#include <gui/widgets/multilinedelegate.h>
#include <utils/settings/settingsmanager.h>

#include <QHeaderView>
#include <QToolButton>
#include <QVBoxLayout>

#include <unordered_set>

namespace Fooyin::Filters {
class LibraryFilterPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit LibraryFilterPageWidget(LibraryFilterRegistry* registry);

    void load() override;
    void apply() override;
    void reset() override;

    [[nodiscard]] QString validationError() const override;

private:
    void updateButtonState();

    LibraryFilterRegistry* m_registry;
    ExtendableTableView* m_filterList;
    LibraryFilterModel* m_model;
    QToolButton* m_openEditor;
};

LibraryFilterPageWidget::LibraryFilterPageWidget(LibraryFilterRegistry* registry)
    : m_registry{registry}
    , m_filterList{new ExtendableTableView(ExtendableTableView::Move, this)}
    , m_model{new LibraryFilterModel(this)}
    , m_openEditor{new QToolButton(this)}
{
    m_filterList->setExtendableModel(m_model);
    m_filterList->setExtendableColumn(1);
    m_filterList->setItemDelegateForColumn(0, new CheckBoxDelegate(this));
    m_filterList->setItemDelegateForColumn(2, new MultiLineEditDelegate(this));
    m_filterList->verticalHeader()->hide();
    m_filterList->horizontalHeader()->setStretchLastSection(false);
    m_filterList->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_filterList->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_filterList->horizontalHeader()->resizeSection(1, 180);
    m_filterList->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    m_openEditor->setText(tr("Script Editor"));
    m_filterList->addCustomTool(m_openEditor);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_filterList);

    QObject::connect(m_filterList->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                     &LibraryFilterPageWidget::updateButtonState);
    QObject::connect(m_model, &QAbstractItemModel::rowsMoved, this, &LibraryFilterPageWidget::updateButtonState);
    QObject::connect(m_openEditor, &QToolButton::clicked, this, [this]() {
        const QModelIndexList selection = m_filterList->selectionModel()->selectedIndexes();
        if(selection.size() != 1 || selection.front().column() != 2) {
            return;
        }

        const QModelIndex index = selection.front();
        ScriptEditor::openEditor(
            index.data(Qt::EditRole).toString(),
            [this, index](const QString& script) { m_model->setData(index, script, Qt::EditRole); }, {}, this);
    });
}

void LibraryFilterPageWidget::load()
{
    m_model->setPresets(m_registry->items());
    updateButtonState();
}

void LibraryFilterPageWidget::apply()
{
    auto presets = m_model->presets();

    std::unordered_set<int> retainedIds;
    retainedIds.reserve(presets.size());
    for(const LibraryFilter& preset : presets) {
        if(preset.id >= 0) {
            retainedIds.insert(preset.id);
        }
    }

    const auto filters = m_registry->items();
    for(const LibraryFilter& filter : filters) {
        if(!retainedIds.contains(filter.id)) {
            m_registry->removeById(filter.id);
        }
    }

    for(LibraryFilter& filter : presets) {
        filter.name       = filter.name.trimmed();
        filter.expression = filter.expression.trimmed();
        if(filter.id < 0) {
            filter = m_registry->addItem(filter);
        }
    }

    for(int index{0}; std::cmp_less(index, presets.size()); ++index) {
        LibraryFilter& preset = presets.at(index);
        preset.index          = index;
        m_registry->changeItem(preset);
    }

    m_model->setPresets(m_registry->items());
    updateButtonState();
}

void LibraryFilterPageWidget::reset()
{
    m_registry->reset();
    load();
}

QString LibraryFilterPageWidget::validationError() const
{
    return m_model->validationError();
}

void LibraryFilterPageWidget::updateButtonState()
{
    const QModelIndexList selection = m_filterList->selectionModel()->selectedIndexes();
    const bool hasSelection         = !selection.empty();
    m_filterList->removeRowAction()->setEnabled(hasSelection);
    m_openEditor->setEnabled(selection.size() == 1 && selection.front().column() == 2);

    if(!m_filterList->moveUpAction() || !m_filterList->moveDownAction()) {
        return;
    }

    m_filterList->moveUpAction()->setEnabled(hasSelection && selection.front().row() > 0);
    m_filterList->moveDownAction()->setEnabled(hasSelection && selection.back().row() < m_model->rowCount({}) - 1);
}

LibraryFilterPage::LibraryFilterPage(LibraryFilterRegistry* registry, SettingsManager* settings, QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::Filters);
    setName(tr("Filters"));
    setCategory({tr("Library"), tr("Filters")});
    setWidgetCreator([registry] { return new LibraryFilterPageWidget(registry); });
}
} // namespace Fooyin::Filters

#include "libraryfilterpage.moc"
#include "moc_libraryfilterpage.cpp"
