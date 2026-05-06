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

#include "tageditorpropertiestab.h"

#include "core/coresettings.h"
#include "tageditoreditor.h"
#include "tagfilldialog.h"

#include <gui/widgets/autoheaderview.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QDialog>
#include <QMessageBox>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

constexpr auto DontAskAgain = "TagEditor/DontAskAgain";
constexpr auto HeaderState  = "TagEditor/HeaderState";

namespace Fooyin::TagEditor {
TagEditorPropertiesTab::TagEditorPropertiesTab(ActionManager* actionManager, TagEditorFieldRegistry* registry,
                                               SettingsManager* settings, QWidget* parent)
    : PropertiesTabWidget{parent}
    , m_settings{settings}
    , m_session{nullptr}
    , m_activeTrackIndex{-1}
    , m_hasPendingMetadataChanges{false}
    , m_hasPendingStatChanges{false}
    , m_editor{new TagEditorEditor(actionManager, registry, settings, this)}
{
    setObjectName(u"TagEditorPropertiesTab"_s);
    setMinimumSize(300, 400);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_editor);

    QObject::connect(m_editor, &TagEditorEditor::pendingChangesStateChanged, this,
                     &TagEditorPropertiesTab::pendingChangesStateChanged);
    QObject::connect(m_editor, &TagEditorEditor::autoFillValuesRequested, this,
                     &TagEditorPropertiesTab::autoFillValues);

    restoreState();
}

TagEditorPropertiesTab::~TagEditorPropertiesTab()
{
    saveState();
}

void TagEditorPropertiesTab::setTracks(const TrackList& tracks)
{
    m_tracks = tracks;
    m_pendingTracks.clear();
    m_activeTrackIndex          = tracks.size() == 1 ? 0 : -1;
    m_hasPendingMetadataChanges = false;
    m_hasPendingStatChanges     = false;
    refreshTrackScope();
}

void TagEditorPropertiesTab::setReadOnly(bool readOnly)
{
    m_editor->setReadOnly(readOnly);
}

void TagEditorPropertiesTab::setSession(PropertiesDialogSession* session)
{
    m_session = session;
}

void TagEditorPropertiesTab::setTrackScope(const TrackList& tracks)
{
    if(!m_session) {
        m_activeTrackIndex = tracks.size() == 1 ? 0 : -1;
    }

    refreshTrackScope();
}

bool TagEditorPropertiesTab::hasPendingScopeChanges() const
{
    return m_editor->hasChanges();
}

bool TagEditorPropertiesTab::commitPendingChanges()
{
    const TrackList changedTracks = commitCurrentScopeEdits();
    if(!changedTracks.empty()) {
        Q_EMIT tracksChanged(changedTracks);
    }
    return true;
}

void TagEditorPropertiesTab::apply()
{
    const TrackList changedTracks = commitCurrentScopeEdits();
    if(!changedTracks.empty()) {
        Q_EMIT tracksChanged(changedTracks);
    }

    if(m_pendingTracks.empty()) {
        return;
    }

    const bool updateStats = m_hasPendingStatChanges && !m_hasPendingMetadataChanges;

    auto applyChanges = [this, updateStats]() {
        if(updateStats) {
            Q_EMIT trackStatsChanged(m_pendingTracks);
        }
        else {
            Q_EMIT trackMetadataChanged(m_pendingTracks);
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

    if(message.exec() == QMessageBox::Yes) {
        if(dontAskAgain->isChecked()) {
            m_settings->fileSet(DontAskAgain, true);
        }
        applyChanges();
    }
}

void TagEditorPropertiesTab::updateTracks(const TrackList& tracks)
{
    if(!m_session) {
        const TrackList changedTracks = commitCurrentScopeEdits();
        if(!changedTracks.empty()) {
            Q_EMIT tracksChanged(changedTracks);
        }
        mergeTracks(m_tracks, tracks);
    }

    refreshTrackScope();
}

void TagEditorPropertiesTab::autoFillValues()
{
    if(m_fillDialog) {
        m_fillDialog->raise();
        m_fillDialog->activateWindow();
        return;
    }

    const TrackList committedTracks = commitCurrentScopeEdits();
    if(!committedTracks.empty()) {
        Q_EMIT tracksChanged(committedTracks);
    }

    const TrackList tracks = activeTracks();
    if(tracks.empty()) {
        return;
    }

    m_fillDialog
        = openFillDialog(tracks, this, [this](const FillValuesResult& result) { handleFillDialogAccepted(result); });
}

void TagEditorPropertiesTab::handleFillDialogAccepted(const FillValuesResult& result)
{
    if(result.matchedTracks == 0 && result.unmatchedTracks == 0 && result.changedTracks == 0) {
        return;
    }

    if(!result.tracks.empty()) {
        stageTrackChanges(result.tracks, true);
        refreshTrackScope();
    }
}

TrackList TagEditorPropertiesTab::commitCurrentScopeEdits()
{
    if(!m_editor->hasChanges()) {
        return {};
    }

    const bool statOnly           = m_editor->hasOnlyStatChanges();
    const TrackList changedTracks = m_editor->applyChanges();

    if(!m_session) {
        mergeTracks(m_tracks, changedTracks);
    }
    mergeTracks(m_pendingTracks, changedTracks);

    if(statOnly) {
        m_hasPendingStatChanges = true;
    }
    else {
        m_hasPendingMetadataChanges = true;
    }

    return changedTracks;
}

void TagEditorPropertiesTab::stageTrackChanges(const TrackList& tracks, bool metadataChanges)
{
    if(tracks.empty()) {
        return;
    }

    mergeTracks(m_tracks, tracks);
    mergeTracks(m_pendingTracks, tracks);

    if(metadataChanges) {
        m_hasPendingMetadataChanges = true;
    }
    else {
        m_hasPendingStatChanges = true;
    }

    Q_EMIT tracksChanged(tracks);
}

void TagEditorPropertiesTab::refreshTrackScope()
{
    m_editor->setTracks(activeTracks());
}

TrackList TagEditorPropertiesTab::activeTracks() const
{
    if(m_session) {
        return m_session->activeTracks();
    }

    if(m_activeTrackIndex < 0 || std::cmp_greater_equal(m_activeTrackIndex, m_tracks.size())) {
        return m_tracks;
    }

    return {m_tracks.at(static_cast<TrackList::size_type>(m_activeTrackIndex))};
}

void TagEditorPropertiesTab::mergeTracks(TrackList& destination, const TrackList& source)
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

void TagEditorPropertiesTab::saveState() const
{
    FyStateSettings settings;
    QByteArray state = m_editor->header()->saveHeaderState();
    state            = qCompress(state, 9);
    settings.setValue(HeaderState, state.toBase64());
}

void TagEditorPropertiesTab::restoreState() const
{
    const FyStateSettings settings;

    if(settings.contains(HeaderState)) {
        const auto headerState = settings.value(HeaderState).toString().toUtf8();

        if(!headerState.isEmpty() && headerState.isValidUtf8()) {
            auto state = QByteArray::fromBase64(headerState);
            state      = qUncompress(state);
            m_editor->header()->restoreHeaderState(state);
            return;
        }
    }
}
} // namespace Fooyin::TagEditor

#include "moc_tageditorpropertiestab.cpp"
