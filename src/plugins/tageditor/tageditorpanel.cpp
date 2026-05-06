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

#include "tageditorpanel.h"

#include "tageditoreditor.h"

#include <core/engine/audioloader.h>
#include <core/library/musiclibrary.h>
#include <core/track.h>
#include <gui/trackselectioncontroller.h>
#include <gui/widgets/autoheaderview.h>
#include <utils/settings/settingsmanager.h>

#include <QCheckBox>
#include <QJsonObject>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

constexpr auto DontAskAgain = "TagEditor/DontAskAgain";

namespace Fooyin::TagEditor {
TagEditorPanel::TagEditorPanel(ActionManager* actionManager, TagEditorFieldRegistry* registry, MusicLibrary* library,
                               std::shared_ptr<AudioLoader> audioLoader, TrackSelectionController* selectionController,
                               SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_library{library}
    , m_audioLoader{std::move(audioLoader)}
    , m_selectionController{selectionController}
    , m_settings{settings}
    , m_editor{new TagEditorEditor(actionManager, registry, settings, this)}
    , m_applyButton{new QPushButton(tr("Apply"), this)}
{
    setObjectName(TagEditorPanel::name());

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_editor);
    m_editor->addTool(m_applyButton);

    QObject::connect(m_applyButton, &QPushButton::clicked, this, &TagEditorPanel::apply);
    QObject::connect(m_selectionController, &TrackSelectionController::selectionChanged, this,
                     &TagEditorPanel::selectionChanged);

    selectionChanged();
}

QString TagEditorPanel::name() const
{
    return u"Tag Editor"_s;
}

QString TagEditorPanel::layoutName() const
{
    return u"TagEditorPanel"_s;
}

void TagEditorPanel::saveLayoutData(QJsonObject& layout)
{
    QByteArray state         = m_editor->header()->saveHeaderState();
    state                    = qCompress(state, 9);
    layout["HeaderState"_L1] = QString::fromUtf8(state.toBase64());
}

void TagEditorPanel::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains("HeaderState"_L1)) {
        const auto headerState = layout.value("HeaderState"_L1).toString().toUtf8();

        if(!headerState.isEmpty() && headerState.isValidUtf8()) {
            auto state = QByteArray::fromBase64(headerState);
            state      = qUncompress(state);
            m_editor->header()->restoreHeaderState(state);
        }
    }
}

void TagEditorPanel::selectionChanged()
{
    const TrackList newTracks = m_selectionController->selectedTracks();

    // Ignore empty selections: focus moving to a widget with no registered track context
    // (e.g. the Apply button) makes selectedTracks() return empty. Keep displaying the
    // current tracks in that case rather than clearing the editor or showing a save dialog.
    if(newTracks.empty() && !m_currentTracks.empty()) {
        return;
    }

    const bool tracksActuallyChanged
        = newTracks.size() != m_currentTracks.size()
       || !std::ranges::equal(newTracks, m_currentTracks,
                              [](const Track& a, const Track& b) { return a.id() == b.id(); });

    if(tracksActuallyChanged && m_editor->hasChanges()) {
        const int result = QMessageBox::question(this, tr("Unsaved Changes"),
                                                 tr("There are unsaved tag changes. Save before switching tracks?"),
                                                 QMessageBox::Save | QMessageBox::Discard, QMessageBox::Save);

        if(result == QMessageBox::Save && !apply()) {
            return;
        }
    }

    updateForTracks(newTracks);
}

bool TagEditorPanel::apply()
{
    if(!m_editor->hasChanges()) {
        return true;
    }

    const bool statOnly = m_editor->hasOnlyStatChanges();
    if(!statOnly && !m_settings->fileValue(DontAskAgain).toBool()) {
        QMessageBox message;
        message.setIcon(QMessageBox::Warning);
        message.setText(tr("Are you sure?"));
        message.setInformativeText(tr("Metadata in the associated files will be overwritten."));

        auto* dontAskAgain = new QCheckBox(tr("Don't ask again"), &message);
        message.setCheckBox(dontAskAgain);

        message.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        message.setDefaultButton(QMessageBox::No);

        if(message.exec() != QMessageBox::Yes) {
            return false;
        }
        if(dontAskAgain->isChecked()) {
            m_settings->fileSet(DontAskAgain, true);
        }
    }

    const TrackList changedTracks = m_editor->applyChanges();
    if(changedTracks.empty()) {
        return true;
    }

    if(statOnly) {
        m_library->updateTrackStats(changedTracks);
    }
    else {
        m_library->writeTrackMetadata(changedTracks);
    }

    return true;
}

void TagEditorPanel::updateForTracks(const TrackList& tracks)
{
    m_currentTracks = tracks;

    const bool hasTracks = !tracks.empty();
    const bool canWrite  = hasTracks && std::ranges::all_of(tracks, [this](const Track& track) {
                              return !track.hasCue() && !track.isInArchive() && m_audioLoader->canWriteMetadata(track);
                           });
    m_applyButton->setEnabled(canWrite);
    m_editor->setReadOnly(!canWrite);
    m_editor->setTracks(tracks);
}

} // namespace Fooyin::TagEditor

#include "moc_tageditorpanel.cpp"
