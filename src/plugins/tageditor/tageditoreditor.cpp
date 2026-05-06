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

#include "tageditoreditor.h"

#include "settings/tageditorfieldregistry.h"
#include "tageditorautocompletedelegate.h"
#include "tageditorconstants.h"
#include "tageditormodel.h"
#include "tageditorview.h"

#include <core/constants.h>
#include <core/coresettings.h>
#include <gui/widgets/autoheaderview.h>
#include <gui/widgets/multilinedelegate.h>
#include <gui/widgets/toolbutton.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>
#include <utils/stardelegate.h>

#include <QHeaderView>
#include <QMenu>
#include <QTimer>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin::TagEditor {
TagEditorEditor::TagEditorEditor(ActionManager* actionManager, TagEditorFieldRegistry* registry,
                                 SettingsManager* settings, QWidget* parent)
    : QWidget{parent}
    , m_registry{registry}
    , m_settings{settings}
    , m_readOnly{false}
    , m_firstReset{true}
    , m_view{new TagEditorView(actionManager, this)}
    , m_model{new TagEditorModel(settings, this)}
    , m_header{new AutoHeaderView(Qt::Horizontal, this)}
    , m_autocompleteDelegate{new TagEditorAutocompleteDelegate(this)}
    , m_multilineDelegate{nullptr}
    , m_starDelegate{nullptr}
    , m_toolsButton{new ToolButton(this)}
    , m_autoTrackNum{new QAction(tr("Auto &track number"), this)}
    , m_autoFillValuesAction{new QAction(tr("Automatically &fill values…"), this)}
    , m_changeFields{new QAction(tr("&Change default fields…"), this)}
{
    setObjectName(u"TagEditorEditor"_s);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_view);

    m_header->setStretchEnabled(true);
    m_header->setStretchLastSection(true);
    m_view->setHorizontalHeader(m_header);
    m_view->setExtendableModel(m_model);
    m_view->setItemDelegateForColumn(1, m_autocompleteDelegate);
    m_view->setupActions();

    m_toolsButton->setText(tr("Tools"));
    m_toolsButton->setMenuIndicatorHidden(false);
    m_toolsButton->setPopupMode(ToolButton::InstantPopup);
    auto* toolsMenu = new QMenu(m_toolsButton);
    toolsMenu->addAction(m_autoTrackNum);
    toolsMenu->addAction(m_autoFillValuesAction);
    toolsMenu->addAction(m_changeFields);
    m_toolsButton->setMenu(toolsMenu);
    m_view->addCustomTool(m_toolsButton);

    QObject::connect(m_header, &AutoHeaderView::stateRestored, this, [this]() { m_firstReset = false; });
    QObject::connect(m_model, &QAbstractItemModel::rowsInserted, m_view, &QTableView::resizeRowsToContents);
    QObject::connect(
        m_model, &QAbstractItemModel::modelReset, this,
        [this]() {
            m_view->resizeRowsToContents();

            if(m_firstReset) {
                m_firstReset = false;
                QTimer::singleShot(0, this, [this]() { m_header->resizeColumnToContents(0); });
            }
        },
        Qt::QueuedConnection);
    QObject::connect(m_model, &QAbstractItemModel::dataChanged, this, &TagEditorEditor::updatePendingScopeState);
    QObject::connect(m_model, &QAbstractItemModel::rowsInserted, this, &TagEditorEditor::updatePendingScopeState);
    QObject::connect(m_model, &QAbstractItemModel::rowsRemoved, this, &TagEditorEditor::updatePendingScopeState);
    QObject::connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        const QModelIndexList selected = m_view->selectionModel()->selectedIndexes();
        m_view->removeRowAction()->setEnabled(!m_readOnly && !selected.empty());
    });
    QObject::connect(m_autoTrackNum, &QAction::triggered, m_model, &TagEditorModel::autoNumberTracks);
    QObject::connect(m_autoFillValuesAction, &QAction::triggered, this, &TagEditorEditor::autoFillValuesRequested);
    QObject::connect(m_changeFields, &QAction::triggered, this,
                     [this]() { m_settings->settingsDialog()->openAtPage(Constants::Page::TagEditorFields); });
}

void TagEditorEditor::setTracks(const TrackList& tracks)
{
    const auto items = m_registry->items();
    configureDelegates(items);

    m_tracks = tracks;

    refreshModel();

    const auto refreshEditor = [this]() {
        const auto fieldItems = m_registry->items();
        configureDelegates(fieldItems);
        this->refreshModel();
    };

    QObject::disconnect(m_registry, nullptr, this, nullptr);
    QObject::connect(m_registry, &TagEditorFieldRegistry::itemAdded, this, refreshEditor);
    QObject::connect(m_registry, &TagEditorFieldRegistry::itemChanged, this, refreshEditor);
    QObject::connect(m_registry, &TagEditorFieldRegistry::itemRemoved, this, refreshEditor);
}

void TagEditorEditor::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;

    m_view->setTagEditTriggers(readOnly ? QAbstractItemView::NoEditTriggers
                                        : (QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked
                                           | QAbstractItemView::EditKeyPressed));
    m_view->addRowAction()->setDisabled(readOnly);
    m_view->removeRowAction()->setDisabled(readOnly);
    m_autoTrackNum->setDisabled(readOnly);
    m_autoFillValuesAction->setDisabled(readOnly);
}

void TagEditorEditor::configureDelegates(const std::vector<TagEditorField>& items)
{
    for(const int row : m_delegateRows) {
        m_view->setItemDelegateForRow(row, nullptr);
    }
    m_delegateRows.clear();
    m_view->setRatingRow(-1);
    m_model->setRatingRow(-1);

    for(int row{0}; const auto& item : items) {
        if(item.scriptField.compare(QLatin1String{Fooyin::Constants::MetaData::RatingEditor}, Qt::CaseInsensitive)
           == 0) {
            if(!m_starDelegate) {
                m_starDelegate = new StarDelegate(this);
            }
            m_view->setItemDelegateForRow(row, m_starDelegate);
            m_view->setRatingRow(row);
            m_model->setRatingRow(row);
            m_delegateRows.emplace(row);
        }
        else if(item.multiline) {
            if(!m_multilineDelegate) {
                m_multilineDelegate = new MultiLineEditDelegate(this);
            }
            m_view->setItemDelegateForRow(row, m_multilineDelegate);
            m_delegateRows.emplace(row);
        }
        ++row;
    }
}

void TagEditorEditor::refreshModel()
{
    const auto items = m_registry->items();
    m_model->reset(m_tracks, items);
    m_autocompleteDelegate->setTracks(m_tracks);
    updatePendingScopeState();
}

bool TagEditorEditor::hasChanges() const
{
    return m_model->haveChanges();
}

bool TagEditorEditor::hasOnlyStatChanges() const
{
    return m_model->haveOnlyStatChanges();
}

TrackList TagEditorEditor::tracks() const
{
    return m_model->tracks();
}

TrackList TagEditorEditor::applyChanges()
{
    if(!m_model->haveChanges()) {
        return {};
    }

    m_model->applyChanges();
    updatePendingScopeState();
    return m_model->tracks();
}

void TagEditorEditor::addTool(QWidget* widget)
{
    m_view->addCustomTool(widget);
}

AutoHeaderView* TagEditorEditor::header() const
{
    return m_header;
}

void TagEditorEditor::updatePendingScopeState()
{
    Q_EMIT pendingChangesStateChanged();
}
} // namespace Fooyin::TagEditor

#include "moc_tageditoreditor.cpp"
