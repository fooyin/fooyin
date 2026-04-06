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

#include "tageditorwidget.h"

#include "settings/tageditorfieldregistry.h"
#include "tageditorautocompletedelegate.h"
#include "tageditorconstants.h"
#include "tageditormodel.h"
#include "tageditorview.h"

#include <core/constants.h>
#include <core/coresettings.h>
#include <core/library/libraryutils.h>
#include <gui/widgets/multilinedelegate.h>
#include <utils/actions/actionmanager.h>
#include <utils/settings/settingsdialogcontroller.h>
#include <utils/settings/settingsmanager.h>
#include <utils/stardelegate.h>

#include <QCheckBox>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

constexpr auto DontAskAgain    = "TagEditor/DontAskAgain";
constexpr auto NameColumnWidth = "TagEditor/NameColumnWidth";

namespace Fooyin::TagEditor {
TagEditorWidget::TagEditorWidget(ActionManager* actionManager, TagEditorFieldRegistry* registry,
                                 SettingsManager* settings, QWidget* parent)
    : PropertiesTabWidget{parent}
    , m_registry{registry}
    , m_settings{settings}
    , m_session{nullptr}
    , m_readOnly{false}
    , m_view{new TagEditorView(actionManager, this)}
    , m_model{new TagEditorModel(settings, this)}
    , m_firstReset{true}
    , m_activeTrackIndex{-1}
    , m_hasPendingScopeChanges{false}
    , m_hasPendingMetadataChanges{false}
    , m_hasPendingStatChanges{false}
    , m_autocompleteDelegate{new TagEditorAutocompleteDelegate(this)}
    , m_multilineDelegate{nullptr}
    , m_starDelegate{nullptr}
    , m_autoTrackNum{new QAction(tr("Auto &track number"), this)}
    , m_changeFields{new QAction(tr("&Change default fields…"), this)}
{
    setObjectName(TagEditorWidget::name());
    setWindowFlags(Qt::Dialog);
    resize(600, 720);
    setMinimumSize(300, 400);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_view);

    m_view->setExtendableModel(m_model);
    m_view->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_view->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_view->setItemDelegateForColumn(1, m_autocompleteDelegate);
    m_view->setupActions();

    QObject::connect(m_model, &QAbstractItemModel::rowsInserted, m_view, &QTableView::resizeRowsToContents);
    QObject::connect(
        m_model, &QAbstractItemModel::modelReset, this,
        [this]() {
            m_view->resizeColumnsForContents();
            m_view->resizeRowsToContents();
            restoreState();
        },
        Qt::QueuedConnection);
    QObject::connect(m_model, &QAbstractItemModel::dataChanged, this, [this]() { updatePendingScopeState(); });
    QObject::connect(m_model, &QAbstractItemModel::rowsInserted, this, [this]() { updatePendingScopeState(); });
    QObject::connect(m_model, &QAbstractItemModel::rowsRemoved, this, [this]() { updatePendingScopeState(); });
    QObject::connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        const QModelIndexList selected = m_view->selectionModel()->selectedIndexes();
        m_view->removeRowAction()->setEnabled(!m_readOnly && !selected.empty());
    });
    QObject::connect(m_autoTrackNum, &QAction::triggered, m_model, &TagEditorModel::autoNumberTracks);
    QObject::connect(m_changeFields, &QAction::triggered, this,
                     [this]() { m_settings->settingsDialog()->openAtPage(Constants::Page::TagEditorFields); });
}

TagEditorWidget::~TagEditorWidget()
{
    saveState();
}

void TagEditorWidget::setTracks(const TrackList& tracks)
{
    const auto items = m_registry->items();
    configureDelegates(items);

    m_tracks = tracks;
    m_pendingTracks.clear();
    m_activeTrackIndex          = -1;
    m_hasPendingScopeChanges    = false;
    m_hasPendingMetadataChanges = false;
    m_hasPendingStatChanges     = false;

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

void TagEditorWidget::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;

    m_view->setTagEditTriggers(readOnly ? QAbstractItemView::NoEditTriggers
                                        : (QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked
                                           | QAbstractItemView::EditKeyPressed));
    m_view->addRowAction()->setDisabled(readOnly);
    m_view->removeRowAction()->setDisabled(readOnly);
    m_autoTrackNum->setDisabled(readOnly);
}

void TagEditorWidget::configureDelegates(const std::vector<TagEditorField>& items)
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

void TagEditorWidget::refreshModel()
{
    const auto items       = m_registry->items();
    const TrackList tracks = activeTracks();
    m_model->reset(tracks, items);
    m_autocompleteDelegate->setTracks(tracks);
    updatePendingScopeState();
}

TrackList TagEditorWidget::commitCurrentScopeEdits()
{
    if(!m_model->haveChanges()) {
        return {};
    }

    const bool statOnly = m_model->haveOnlyStatChanges();

    m_model->applyChanges();

    const TrackList changedTracks = m_model->tracks();
    mergeTracks(m_tracks, changedTracks);
    mergeTracks(m_pendingTracks, changedTracks);

    if(statOnly) {
        m_hasPendingStatChanges = true;
    }
    else {
        m_hasPendingMetadataChanges = true;
    }

    return changedTracks;
}

TrackList TagEditorWidget::activeTracks() const
{
    if(m_session) {
        return m_session->activeTracks();
    }

    if(m_activeTrackIndex < 0 || std::cmp_greater_equal(m_activeTrackIndex, m_tracks.size())) {
        return m_tracks;
    }

    return {m_tracks.at(static_cast<TrackList::size_type>(m_activeTrackIndex))};
}

bool TagEditorWidget::hasPendingScopeChanges() const
{
    return m_hasPendingScopeChanges;
}

void TagEditorWidget::mergeTracks(TrackList& destination, const TrackList& source)
{
    for(const Track& updatedTrack : source) {
        const auto destinationIt = std::ranges::find_if(
            destination, [&updatedTrack](const Track& track) { return track.sameIdentityAs(updatedTrack); });

        if(destinationIt != destination.end()) {
            *destinationIt = updatedTrack;
            continue;
        }

        destination.emplace_back(updatedTrack);
    }
}

QString TagEditorWidget::name() const
{
    return u"Tag Editor"_s;
}

QString TagEditorWidget::layoutName() const
{
    return u"TagEditor"_s;
}

bool TagEditorWidget::hasTools() const
{
    return true;
}

void TagEditorWidget::addTools(QMenu* menu)
{
    menu->addAction(m_autoTrackNum);
    menu->addAction(m_changeFields);
}

void TagEditorWidget::setSession(PropertiesDialogSession* session)
{
    m_session = session;
}

void TagEditorWidget::setTrackScope(const TrackList& tracks)
{
    if(!m_session) {
        m_tracks           = tracks;
        m_activeTrackIndex = tracks.size() == 1 ? 0 : -1;
    }

    refreshModel();
}

void TagEditorWidget::updatePendingScopeState()
{
    const bool hasPendingChanges = m_model->haveChanges();
    if(m_hasPendingScopeChanges == hasPendingChanges) {
        return;
    }

    m_hasPendingScopeChanges = hasPendingChanges;
    emit pendingChangesStateChanged();
}

bool TagEditorWidget::commitPendingChanges()
{
    const TrackList changedTracks = commitCurrentScopeEdits();
    if(!changedTracks.empty()) {
        emit tracksChanged(changedTracks);
    }
    return true;
}

void TagEditorWidget::apply()
{
    const TrackList changedTracks = commitCurrentScopeEdits();
    if(!changedTracks.empty()) {
        emit tracksChanged(changedTracks);
    }

    if(m_pendingTracks.empty()) {
        return;
    }

    const bool updateStats = m_hasPendingStatChanges && !m_hasPendingMetadataChanges;

    auto applyChanges = [this, updateStats]() {
        if(updateStats) {
            emit trackStatsChanged(m_pendingTracks);
        }
        else {
            emit trackMetadataChanged(m_pendingTracks);
        }
        m_pendingTracks.clear();
        m_hasPendingMetadataChanges = false;
        m_hasPendingStatChanges     = false;
    };

    if(m_settings->fileValue(DontAskAgain).toBool()) {
        applyChanges();
        return;
    }

    QMessageBox message;
    message.setIcon(QMessageBox::Warning);
    message.setText(tr("Are you sure?"));
    message.setInformativeText(tr("Metadata in the associated files will be overwritten."));

    auto* dontAskAgain = new QCheckBox(tr("Don't ask again"), &message);
    message.setCheckBox(dontAskAgain);

    message.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    message.setDefaultButton(QMessageBox::No);

    const int buttonClicked = message.exec();

    if(buttonClicked == QMessageBox::Yes) {
        if(dontAskAgain->isChecked()) {
            m_settings->fileSet(DontAskAgain, true);
        }
        applyChanges();
    }
}

void TagEditorWidget::updateTracks(const TrackList& tracks)
{
    if(!m_session) {
        const TrackList changedTracks = commitCurrentScopeEdits();
        if(!changedTracks.empty()) {
            emit tracksChanged(changedTracks);
        }
        mergeTracks(m_tracks, tracks);
    }

    refreshModel();
}

void TagEditorWidget::saveState() const
{
    FyStateSettings stateSettings;
    stateSettings.setValue(NameColumnWidth, m_view->horizontalHeader()->sectionSize(0));
}

void TagEditorWidget::restoreState() const
{
    const FyStateSettings stateSettings;
    const int nameColumnWidth        = stateSettings.value(NameColumnWidth).toInt();
    const int minimumRestorableWidth = m_view->horizontalHeader()->sectionSizeHint(0);

    if(nameColumnWidth > minimumRestorableWidth) {
        m_view->horizontalHeader()->resizeSection(0, nameColumnWidth);
    }

    m_view->normaliseColumnWidths();
}
} // namespace Fooyin::TagEditor

#include "moc_tageditorwidget.cpp"
