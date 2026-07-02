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

#include "lyricspropertiestab.h"
#include "lyricseditor.h"
#include "lyricsfinder.h"
#include "lyricssaver.h"
#include "settings/lyricssettings.h"

#include <core/player/playercontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QVBoxLayout>

#include <ranges>

using namespace Qt::StringLiterals;

namespace {
bool hasSameTagLyrics(const Fooyin::Track& lhs, const Fooyin::Track& rhs, const Fooyin::SettingsManager* settings)
{
    const QStringList searchTags
        = settings->fileValue(Fooyin::Lyrics::Settings::SearchTags, Fooyin::Lyrics::Defaults::searchTags())
              .toStringList();
    return std::ranges::all_of(searchTags,
                               [&lhs, &rhs](const QString& tag) { return lhs.extraTag(tag) == rhs.extraTag(tag); });
}
} // namespace

namespace Fooyin::Lyrics {
LyricsPropertiesTab::LyricsPropertiesTab(const Track& track, std::shared_ptr<NetworkAccessManager> networkAccess,
                                         LyricsSaver* lyricsSaver, PlayerController* playerController,
                                         SettingsManager* settings, TrackEditable canEditTrack, QWidget* parent)
    : PropertiesTabWidget{parent}
    , m_track{track}
    , m_networkAccess{std::move(networkAccess)}
    , m_lyricsSaver{lyricsSaver}
    , m_playerController{playerController}
    , m_settings{settings}
    , m_canEditTrack{std::move(canEditTrack)}
    , m_lyricsFinder{new LyricsFinder(m_networkAccess, m_settings, this)}
    , m_editor{new LyricsEditor(playerController, this)}
    , m_hasPendingScopeChanges{false}
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_editor);

    setupConnections();
    updateTrack(m_track);
}

void LyricsPropertiesTab::setTrackScope(const TrackList& tracks)
{
    if(tracks.size() == 1) {
        updateTrack(tracks.front());
    }
}

bool LyricsPropertiesTab::isAvailableForScope(const TrackList& tracks) const
{
    return tracks.size() == 1 && tracks.front().isValid() && (!m_canEditTrack || m_canEditTrack(tracks.front()));
}

bool LyricsPropertiesTab::hasPendingScopeChanges() const
{
    return m_hasPendingScopeChanges;
}

bool LyricsPropertiesTab::commitPendingChanges()
{
    if(!m_track.isValid()) {
        return true;
    }

    storeCurrentDraftText();

    auto* draft = currentDraft();
    if(!draft) {
        updatePendingState();
        return true;
    }

    if(draft->dirty) {
        if(const auto updatedTrack = m_lyricsSaver->updateLyricsTag(draft->workingLyrics, m_track)) {
            m_track = *updatedTrack;
            m_pendingTagTracks.emplace(m_currentTrackKey, *updatedTrack);
            Q_EMIT tracksChanged({*updatedTrack});
        }
    }
    else {
        m_pendingTagTracks.erase(m_currentTrackKey);

        if(const auto restoredTrack = m_lyricsSaver->restoreLyricsTags(draft->originalTagTrack, m_track);
           restoredTrack != m_track) {
            m_track = restoredTrack;
            Q_EMIT tracksChanged({restoredTrack});
        }
    }

    updatePendingState();
    return true;
}

void LyricsPropertiesTab::apply()
{
    commitPendingChanges();

    Lyrics appliedLyrics;
    if(const auto* draft = currentDraft()) {
        appliedLyrics = draft->workingLyrics;
    }
    else {
        appliedLyrics = m_editor->editedLyrics();
    }

    if(!m_pendingTagTracks.empty()) {
        TrackList tracks;
        tracks.reserve(m_pendingTagTracks.size());
        for(const auto& track : m_pendingTagTracks | std::views::values) {
            tracks.emplace_back(track);
        }
        m_lyricsSaver->writeLyricsToTags(tracks);
    }

    for(auto& [key, draft] : m_drafts) {
        if(!draft.dirty) {
            continue;
        }

        draft.originalLyrics       = draft.workingLyrics;
        draft.originalLyricsLoaded = true;
        draft.dirty                = false;

        if(m_pendingTagTracks.contains(key)) {
            draft.originalTagTrack = m_pendingTagTracks.at(key);
        }
    }

    m_pendingTagTracks.clear();
    loadCurrentDraft();
    updatePendingState();

    if(appliedLyrics.isValid()) {
        appliedLyrics.isLocal = true;
    }
    Q_EMIT lyricsEdited(appliedLyrics);
}

void LyricsPropertiesTab::finish()
{
    m_drafts.clear();
    m_pendingTagTracks.clear();
    m_currentTrackKey.clear();
    m_hasPendingScopeChanges = false;
}

QSize LyricsPropertiesTab::sizeHint() const
{
    return m_editor->sizeHint();
}

QString LyricsPropertiesTab::trackKey(const Track& track)
{
    if(track.isInDatabase()) {
        return u"id:%1"_s.arg(track.id());
    }

    return u"%1|%2"_s.arg(track.uniqueFilepath()).arg(track.subsong());
}

void LyricsPropertiesTab::updateTrack(const Track& track)
{
    const Lyrics currentLyrics = m_editor->currentLyrics();
    const bool preserveLyrics  = currentLyrics.isValid() && hasSameTagLyrics(m_track, track, m_settings);

    m_track           = track;
    m_currentTrackKey = track.isValid() ? trackKey(track) : QString{};

    Draft* draft{nullptr};

    if(track.isValid()) {
        draft = &ensureDraft(track);

        if(!draft->originalTagTrack.isValid()) {
            draft->originalTagTrack = track;
        }

        if(preserveLyrics && !draft->originalLyricsLoaded) {
            draft->originalLyrics       = currentLyrics;
            draft->workingLyrics        = currentLyrics;
            draft->originalLyricsLoaded = true;
            draft->dirty                = false;
        }
    }

    m_editor->setTrack(track);
    loadCurrentDraft();
    updatePendingState();

    if(m_lyricsFinder && draft && !draft->originalLyricsLoaded && !preserveLyrics) {
        m_lyricsFinder->findTagLyrics(track);
    }
}

void LyricsPropertiesTab::updateLyrics(const Track& track, const Lyrics& lyrics)
{
    const QString key = trackKey(track);

    auto it = m_drafts.find(key);
    if(it == m_drafts.end()) {
        return;
    }

    Draft& draft              = it->second;
    const bool isCurrentDraft = (key == m_currentTrackKey);
    const QString currentText = isCurrentDraft ? m_editor->text() : draft.workingLyrics.data;

    draft.originalLyrics       = lyrics;
    draft.originalLyricsLoaded = true;

    if(!draft.dirty) {
        draft.workingLyrics = lyrics;
    }
    else {
        draft.dirty = (currentText != draft.originalLyrics.data);
    }

    if(isCurrentDraft) {
        if(!draft.dirty) {
            loadCurrentDraft();
        }
        updatePendingState();
    }
}

void LyricsPropertiesTab::loadCurrentDraft()
{
    if(const auto* draft = currentDraft()) {
        m_editor->setLyrics(draft->workingLyrics);
    }
    else {
        m_editor->setLyrics({});
    }
}

void LyricsPropertiesTab::storeCurrentDraftText()
{
    auto* draft = currentDraft();
    if(!draft) {
        return;
    }

    draft->workingLyrics = m_editor->editedLyrics();
    draft->dirty         = (draft->workingLyrics.data != draft->originalLyrics.data);
}

void LyricsPropertiesTab::updatePendingState()
{
    bool hasPendingChanges{false};

    if(const auto* draft = currentDraft()) {
        hasPendingChanges = (m_editor->text() != draft->workingLyrics.data);
    }

    if(!hasPendingChanges) {
        for(const auto& draft : m_drafts | std::views::values) {
            if(draft.dirty) {
                hasPendingChanges = true;
                break;
            }
        }
    }

    if(m_hasPendingScopeChanges == hasPendingChanges) {
        return;
    }

    m_hasPendingScopeChanges = hasPendingChanges;
    Q_EMIT pendingChangesStateChanged();
}

void LyricsPropertiesTab::setupConnections()
{
    QObject::connect(m_editor, &LyricsEditor::textEdited, this, &LyricsPropertiesTab::updatePendingState);
    QObject::connect(m_editor, &LyricsEditor::resetClicked, this, &LyricsPropertiesTab::reset);
    QObject::connect(m_playerController, &PlayerController::currentTrackUpdated, this,
                     &LyricsPropertiesTab::updateTrack);
    QObject::connect(m_lyricsFinder, &LyricsFinder::lyricsFound, this, &LyricsPropertiesTab::updateLyrics);
}

void LyricsPropertiesTab::reset()
{
    auto* draft = currentDraft();
    if(!draft) {
        return;
    }

    draft->workingLyrics = draft->originalLyrics;
    draft->dirty         = false;
    loadCurrentDraft();
    updatePendingState();
}

const LyricsPropertiesTab::Draft* LyricsPropertiesTab::currentDraft() const
{
    if(m_currentTrackKey.isEmpty()) {
        return nullptr;
    }

    const auto it = m_drafts.find(m_currentTrackKey);
    return it != m_drafts.cend() ? &it->second : nullptr;
}

LyricsPropertiesTab::Draft* LyricsPropertiesTab::currentDraft()
{
    if(m_currentTrackKey.isEmpty()) {
        return nullptr;
    }

    auto it = m_drafts.find(m_currentTrackKey);
    return it != m_drafts.end() ? &it->second : nullptr;
}

LyricsPropertiesTab::Draft& LyricsPropertiesTab::ensureDraft(const Track& track)
{
    auto& draft = m_drafts[trackKey(track)];
    if(!draft.originalTagTrack.isValid()) {
        draft.originalTagTrack = track;
    }
    return draft;
}
} // namespace Fooyin::Lyrics
