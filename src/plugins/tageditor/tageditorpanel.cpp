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

#include "tageditorwidget.h"

#include <core/engine/audioloader.h>
#include <core/library/musiclibrary.h>
#include <core/track.h>
#include <gui/trackselectioncontroller.h>

#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin::TagEditor {

TagEditorPanel::TagEditorPanel(ActionManager* actionManager, TagEditorFieldRegistry* registry,
                               MusicLibrary* library, std::shared_ptr<AudioLoader> audioLoader,
                               TrackSelectionController* selectionController,
                               SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_library{library}
    , m_audioLoader{std::move(audioLoader)}
    , m_selectionController{selectionController}
    , m_editor{new TagEditorWidget(actionManager, registry, settings, this)}
{
    setObjectName(TagEditorPanel::name());

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_editor);

    auto* applyButton = new QPushButton(tr("Apply"), this);
    layout->addWidget(applyButton);

    QObject::connect(applyButton, &QPushButton::clicked, m_editor, &TagEditorWidget::apply);
    QObject::connect(m_editor, &TagEditorWidget::trackMetadataChanged,
                     m_library, &MusicLibrary::writeTrackMetadata);
    QObject::connect(m_editor, &TagEditorWidget::trackStatsChanged, m_library,
                     [this](const TrackList& tracks) { m_library->updateTrackStats(tracks); });
    QObject::connect(m_selectionController, &TrackSelectionController::selectionChanged,
                     this, &TagEditorPanel::selectionChanged);

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

    if(tracksActuallyChanged && m_editor->hasPendingScopeChanges()) {
        const int result = QMessageBox::question(
            this, tr("Unsaved Changes"),
            tr("There are unsaved tag changes. Save before switching tracks?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);

        if(result == QMessageBox::Cancel) {
            return;
        }
        if(result == QMessageBox::Save) {
            m_editor->apply();
        }
    }

    updateForTracks(newTracks);
}

void TagEditorPanel::updateForTracks(const TrackList& tracks)
{
    m_currentTracks = tracks;

    const bool canWrite = !tracks.empty()
                          && std::ranges::all_of(tracks, [this](const Track& track) {
                                 return !track.hasCue() && !track.isInArchive()
                                        && m_audioLoader->canWriteMetadata(track);
                             });
    m_editor->setReadOnly(!canWrite);
    m_editor->setTracks(tracks);
}

} // namespace Fooyin::TagEditor

#include "moc_tageditorpanel.cpp"
