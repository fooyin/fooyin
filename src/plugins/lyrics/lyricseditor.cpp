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

#include "lyricseditor.h"

#include "lyricsfinder.h"
#include "lyricsparser.h"
#include "lyricssaver.h"
#include "settings/lyricssettings.h"

#include <core/player/playercontroller.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QTextBlock>
#include <QTextEdit>

#include <algorithm>
#include <ranges>

using namespace Qt::StringLiterals;

constexpr auto TimestampRegex = R"(\[\d{2}:\d{2}\.\d{2,3}\])";

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
QString LyricsEditor::trackKey(const Track& track)
{
    if(track.isInDatabase()) {
        return u"id:%1"_s.arg(track.id());
    }

    return u"%1|%2"_s.arg(track.uniqueFilepath()).arg(track.subsong());
}

LyricsEditor::LyricsEditor(const Track& track, std::shared_ptr<NetworkAccessManager> networkAccess,
                           LyricsSaver* lyricsSaver, PlayerController* playerController, SettingsManager* settings,
                           TrackEditable canEditTrack, QWidget* parent)
    : PropertiesTabWidget{parent}
    , m_track{track}
    , m_lyricsSaver{lyricsSaver}
    , m_playerController{playerController}
    , m_settings{settings}
    , m_networkAccess{std::move(networkAccess)}
    , m_lyricsFinder{new LyricsFinder(m_networkAccess, m_settings, this)}
    , m_canEditTrack{std::move(canEditTrack)}
    , m_hasPendingScopeChanges{false}
{
    setupUi();
    setupConnections();
    updateTrack(m_track);
}

LyricsEditor::LyricsEditor(Lyrics lyrics, PlayerController* playerController, LyricsSaver* lyricsSaver,
                           SettingsManager* settings, QWidget* parent)
    : PropertiesTabWidget{parent}
    , m_lyricsSaver{lyricsSaver}
    , m_playerController{playerController}
    , m_settings{settings}
    , m_lyricsFinder{nullptr}
    , m_lyrics{std::move(lyrics)}
    , m_hasPendingScopeChanges{false}
{
    setupUi();
    setupConnections();
    updateTrack(m_playerController->currentTrack());

    if(auto* draft = currentDraft()) {
        draft->originalLyrics       = m_lyrics;
        draft->workingLyrics        = m_lyrics;
        draft->originalLyricsLoaded = true;
        draft->dirty                = false;
        loadCurrentDraft();
        updatePendingState();
    }
}

void LyricsEditor::updateTrack(const Track& track)
{
    const bool preserveLyrics = m_lyrics.isValid() && hasSameTagLyrics(m_track, track, m_settings);

    m_track           = track;
    m_currentTrackKey = track.isValid() ? trackKey(track) : QString{};

    Draft* draft{nullptr};

    if(track.isValid()) {
        draft = &ensureDraft(track);
        if(!draft->originalTagTrack.isValid()) {
            draft->originalTagTrack = track;
        }

        if(preserveLyrics && !draft->originalLyricsLoaded) {
            draft->originalLyrics       = m_lyrics;
            draft->workingLyrics        = m_lyrics;
            draft->originalLyricsLoaded = true;
            draft->dirty                = false;
        }
    }

    loadCurrentDraft();
    setControlsEnabled();
    updatePendingState();

    if(m_lyricsFinder && draft && !draft->originalLyricsLoaded && !preserveLyrics) {
        m_lyricsFinder->findLocalLyrics(m_track);
    }
}

QString LyricsEditor::name() const
{
    return tr("Lyrics Editor");
}

QString LyricsEditor::layoutName() const
{
    return u"LyricsEditor"_s;
}

void LyricsEditor::setTrackScope(const TrackList& tracks)
{
    if(tracks.size() == 1) {
        updateTrack(tracks.front());
    }
}

bool LyricsEditor::isAvailableForScope(const TrackList& tracks) const
{
    return tracks.size() == 1 && tracks.front().isValid() && (!m_canEditTrack || m_canEditTrack(tracks.front()));
}

bool LyricsEditor::hasPendingScopeChanges() const
{
    return m_hasPendingScopeChanges;
}

bool LyricsEditor::commitPendingChanges()
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

    const auto saveMethod = static_cast<SaveMethod>(
        m_settings->fileValue(Settings::SaveMethod, static_cast<int>(SaveMethod::Directory)).toInt());

    if(saveMethod == SaveMethod::Tag) {
        if(draft->dirty) {
            if(const auto updatedTrack = m_lyricsSaver->updateLyricsTag(draft->workingLyrics, m_track)) {
                m_track = *updatedTrack;
                m_pendingTagTracks.emplace(m_currentTrackKey, *updatedTrack);
                emit tracksChanged({*updatedTrack});
            }
        }
        else {
            m_pendingTagTracks.erase(m_currentTrackKey);
            if(const auto restoredTrack = m_lyricsSaver->restoreLyricsTags(draft->originalTagTrack, m_track);
               restoredTrack != m_track) {
                m_track = restoredTrack;
                emit tracksChanged({restoredTrack});
            }
        }
    }

    updatePendingState();
    return true;
}

void LyricsEditor::apply()
{
    commitPendingChanges();

    if(!m_lyricsSaver) {
        return;
    }

    const auto saveMethod = static_cast<SaveMethod>(
        m_settings->fileValue(Settings::SaveMethod, static_cast<int>(SaveMethod::Directory)).toInt());

    if(saveMethod == SaveMethod::Tag) {
        if(!m_pendingTagTracks.empty()) {
            TrackList tracks;
            tracks.reserve(m_pendingTagTracks.size());
            for(const auto& track : m_pendingTagTracks | std::views::values) {
                tracks.emplace_back(track);
            }
            m_lyricsSaver->writeLyricsToTags(tracks);
        }
    }
    else {
        for(const auto& draft : m_drafts | std::views::values) {
            if(draft.dirty && draft.originalTagTrack.isValid()) {
                m_lyricsSaver->saveLyricsToFile(draft.workingLyrics, draft.originalTagTrack);
            }
        }
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
}

void LyricsEditor::finish()
{
    m_drafts.clear();
    m_pendingTagTracks.clear();
    m_currentTrackKey.clear();
    m_hasPendingScopeChanges = false;
}

QSize LyricsEditor::sizeHint() const
{
    return Utils::proportionateSize(this, 0.25, 0.5);
}

void LyricsEditor::updateLyrics(const Track& track, const Lyrics& lyrics)
{
    const QString key = trackKey(track);
    if(!m_drafts.contains(key)) {
        return;
    }

    Draft& draft              = m_drafts.at(key);
    const bool isCurrentDraft = (key == m_currentTrackKey);
    const QString currentText = isCurrentDraft ? m_lyricsText->toPlainText() : draft.workingLyrics.data;

    draft.originalLyrics       = lyrics;
    draft.originalLyricsLoaded = true;

    if(!draft.dirty) {
        draft.workingLyrics = lyrics;
    }
    else {
        draft.dirty = (currentText != draft.originalLyrics.data);
    }

    if(isCurrentDraft) {
        loadCurrentDraft();
        updatePendingState();
    }
}

LyricsEditor::Draft& LyricsEditor::ensureDraft(const Track& track)
{
    auto& draft = m_drafts[trackKey(track)];
    if(!draft.originalTagTrack.isValid()) {
        draft.originalTagTrack = track;
    }
    return draft;
}

LyricsEditor::Draft* LyricsEditor::currentDraft()
{
    if(m_currentTrackKey.isEmpty()) {
        return nullptr;
    }

    auto it = m_drafts.find(m_currentTrackKey);
    return it != m_drafts.end() ? &it->second : nullptr;
}

const LyricsEditor::Draft* LyricsEditor::currentDraft() const
{
    if(m_currentTrackKey.isEmpty()) {
        return nullptr;
    }

    const auto it = m_drafts.find(m_currentTrackKey);
    return it != m_drafts.cend() ? &it->second : nullptr;
}

Lyrics LyricsEditor::lyricsFromText(const QString& text) const
{
    Lyrics lyrics = parse(text);
    lyrics.data   = text;
    return lyrics;
}

void LyricsEditor::loadCurrentDraft()
{
    if(const auto* draft = currentDraft()) {
        m_lyrics = draft->workingLyrics;
    }
    else {
        m_lyrics = {};
    }

    const QSignalBlocker blocker{m_lyricsText};
    m_lyricsText->setPlainText(m_lyrics.data);
}

void LyricsEditor::storeCurrentDraftText()
{
    auto* draft = currentDraft();
    if(!draft) {
        return;
    }

    const QString currentText = m_lyricsText->toPlainText();
    draft->workingLyrics      = lyricsFromText(currentText);
    draft->dirty              = (currentText != draft->originalLyrics.data);
    m_lyrics                  = draft->workingLyrics;
}

void LyricsEditor::setControlsEnabled()
{
    const bool canControlPlayback = m_lyricsSaver && m_track.isValid() && m_track == m_playerController->currentTrack();

    m_playPause->setEnabled(canControlPlayback);
    m_seek->setEnabled(canControlPlayback);
    m_insert->setEnabled(canControlPlayback);
    m_insertNext->setEnabled(canControlPlayback);
    m_rewind->setEnabled(canControlPlayback);
    m_forward->setEnabled(canControlPlayback);
}

void LyricsEditor::updatePendingState()
{
    bool hasPendingChanges{false};

    if(const auto* draft = currentDraft()) {
        hasPendingChanges = (m_lyricsText->toPlainText() != draft->workingLyrics.data);
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
    emit pendingChangesStateChanged();
}

void LyricsEditor::setupUi()
{
    setWindowTitle(tr("Lyrics Editor"));

    m_playPause  = new QPushButton(this);
    m_seek       = new QPushButton(tr("Seek"), this);
    m_reset      = new QPushButton(tr("Reset Changes"), this);
    m_insert     = new QPushButton(tr("Insert/Update"), this);
    m_insertNext = new QPushButton(tr("Update and Next Line"), this);
    m_rewind     = new QPushButton(tr("Rewind line (-100ms)"), this);
    m_forward    = new QPushButton(tr("Forward line (+100ms)"), this);
    m_remove     = new QPushButton(tr("Remove"), this);
    m_removeAll  = new QPushButton(tr("Remove All"), this);
    m_lyricsText = new QTextEdit(this);

    m_currentLineColour = palette().highlight().color();
    m_currentLineColour.setAlpha(60);

    auto* layout = new QGridLayout(this);

    auto* operationGroup  = new QGroupBox(tr("Operation"), this);
    auto* operationLayout = new QGridLayout(operationGroup);

    int row{0};
    operationLayout->addWidget(m_playPause, row, 0);
    operationLayout->addWidget(m_seek, row++, 1);
    operationLayout->addWidget(m_reset, row, 0, 1, 2);

    auto* timestampsGroup  = new QGroupBox(tr("Timestamps"), this);
    auto* timestampsLayout = new QGridLayout(timestampsGroup);

    row = 0;
    timestampsLayout->addWidget(m_insert, row, 0);
    timestampsLayout->addWidget(m_insertNext, row++, 1);
    timestampsLayout->addWidget(m_rewind, row, 0);
    timestampsLayout->addWidget(m_forward, row++, 1);
    timestampsLayout->addWidget(m_remove, row, 0);
    timestampsLayout->addWidget(m_removeAll, row, 1);

    row = 0;
    layout->addWidget(operationGroup, row, 0);
    layout->addWidget(timestampsGroup, row++, 1);
    layout->addWidget(m_lyricsText, row++, 0, 1, 2);
    layout->setRowStretch(1, 1);

    reset();
    updateButtons();

    if(m_lyricsSaver && m_track.isValid() && m_track != m_playerController->currentTrack()) {
        m_playPause->setDisabled(true);
        m_seek->setDisabled(true);
        m_insert->setDisabled(true);
        m_insertNext->setDisabled(true);
        m_rewind->setDisabled(true);
        m_forward->setDisabled(true);
    }
}

void LyricsEditor::setupConnections()
{
    QObject::connect(m_lyricsText, &QTextEdit::cursorPositionChanged, this, &LyricsEditor::highlightCurrentLine);
    QObject::connect(m_lyricsText, &QTextEdit::textChanged, this, &LyricsEditor::updatePendingState);
    QObject::connect(m_playPause, &QPushButton::clicked, m_playerController, &PlayerController::playPause);
    QObject::connect(m_seek, &QPushButton::clicked, this, &LyricsEditor::seek);
    QObject::connect(m_reset, &QPushButton::clicked, this, &LyricsEditor::reset);
    QObject::connect(m_insert, &QPushButton::clicked, this, &LyricsEditor::insertOrUpdateTimestamp);
    QObject::connect(m_insertNext, &QPushButton::clicked, this, &LyricsEditor::insertOrUpdateTimestampAndGotoNextLine);
    QObject::connect(m_rewind, &QPushButton::clicked, this, &LyricsEditor::adjustTimestamp);
    QObject::connect(m_forward, &QPushButton::clicked, this, &LyricsEditor::adjustTimestamp);
    QObject::connect(m_remove, &QPushButton::clicked, this, &LyricsEditor::removeTimestamp);
    QObject::connect(m_removeAll, &QPushButton::clicked, this, &LyricsEditor::removeAllTimestamps);

    QObject::connect(m_playerController, &PlayerController::positionChanged, this, &LyricsEditor::updateButtons);
    QObject::connect(m_playerController, &PlayerController::playStateChanged, this, &LyricsEditor::updateButtons);

    loadCurrentDraft();
    setControlsEnabled();

    if(m_lyricsFinder) {
        QObject::connect(m_playerController, &PlayerController::currentTrackUpdated, this, &LyricsEditor::updateTrack);
        QObject::connect(m_lyricsFinder, &LyricsFinder::lyricsFound, this, &LyricsEditor::updateLyrics);
    }
}

void LyricsEditor::reset()
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

void LyricsEditor::seek()
{
    QTextCursor cursor = m_lyricsText->textCursor();
    cursor.select(QTextCursor::LineUnderCursor);
    const QString currentLine = cursor.selectedText();

    static const QRegularExpression regex{QLatin1String{TimestampRegex}};
    const QRegularExpressionMatch match = regex.match(currentLine);
    if(match.hasMatch()) {
        const uint64_t pos = timestampToMs(match.captured(0)) + m_lyrics.offset;
        m_playerController->seek(pos);
    }
}

void LyricsEditor::updateButtons()
{
    const auto time = u" [%1]"_s.arg(formatTimestamp(m_playerController->currentPosition()));

    switch(m_playerController->playState()) {
        case Player::PlayState::Playing:
            m_playPause->setText(tr("Pause") + time);
            break;
        case Player::PlayState::Paused:
        case Player::PlayState::Stopped:
            m_playPause->setText(tr("Play") + time);
            break;
    }
}

void LyricsEditor::highlightCurrentLine()
{
    QTextBlock block = m_lyricsText->document()->firstBlock();
    while(block.isValid()) {
        QTextCursor tempCursor{block};
        QTextBlockFormat format;
        format.setBackground(Qt::NoBrush);
        tempCursor.setBlockFormat(format);
        block = block.next();
    }

    QTextCursor currentCursor{m_lyricsText->textCursor().block()};
    QTextBlockFormat currentBlockFormat;
    currentBlockFormat.setBackground(m_currentLineColour);
    currentCursor.setBlockFormat(currentBlockFormat);
}

void LyricsEditor::insertOrUpdateTimestamp()
{
    QTextCursor cursor = m_lyricsText->textCursor();
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.select(QTextCursor::LineUnderCursor);
    QString currentLine = cursor.selectedText();

    QString newTimestamp = u"[%1]"_s.arg(formatTimestamp(m_playerController->currentPosition()));

    static const QRegularExpression regex{QLatin1String{TimestampRegex}};
    if(regex.match(currentLine).hasMatch()) {
        currentLine.replace(regex, newTimestamp);
    }
    else {
        currentLine = newTimestamp + currentLine;
    }

    cursor.insertText(currentLine);
}

void LyricsEditor::insertOrUpdateTimestampAndGotoNextLine()
{
    LyricsEditor::insertOrUpdateTimestamp();

    QTextCursor cursor = m_lyricsText->textCursor();
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.movePosition(QTextCursor::Down);
    cursor.movePosition(QTextCursor::StartOfLine);
    m_lyricsText->setTextCursor(cursor);
}

void LyricsEditor::adjustTimestamp()
{
    QTextCursor cursor = m_lyricsText->textCursor();
    cursor.select(QTextCursor::LineUnderCursor);
    QString currentLine = cursor.selectedText();

    static const QRegularExpression regex{QLatin1String{TimestampRegex}};
    const QRegularExpressionMatch match = regex.match(currentLine);

    const uint64_t trackDuration = m_playerController->currentTrack().duration();

    if(match.hasMatch()) {
        uint64_t pos = timestampToMs(match.captured(0)) + m_lyrics.offset;
        if(sender() == m_rewind) {
            pos > 100 ? pos -= 100 : pos = 0;
        }
        else {
            pos < trackDuration - 100 ? pos += 100 : pos = trackDuration;
        }

        const QString newTimeStamp = u"[%1]"_s.arg(formatTimestamp(pos));
        currentLine.replace(regex, newTimeStamp);

        // to avoid skipping to next track in playlist during corrections near the end of a track.
        // alternative solutions are welcome.
        if(pos > trackDuration - 1000) {
            m_playerController->pause();
        }
        else {
            m_playerController->seek(pos);
        }
    }
    cursor.insertText(currentLine);
}

void LyricsEditor::removeTimestamp()
{
    QTextCursor cursor = m_lyricsText->textCursor();
    cursor.select(QTextCursor::LineUnderCursor);
    QString currentLine = cursor.selectedText();

    static const QRegularExpression regex{QLatin1String{TimestampRegex}};
    currentLine.remove(regex);

    cursor.insertText(currentLine);
}

void LyricsEditor::removeAllTimestamps()
{
    QString currentText = m_lyricsText->toPlainText();

    static const QRegularExpression regex{QLatin1String{TimestampRegex}};
    currentText.remove(regex);

    m_lyricsText->setPlainText(currentText);
}

LyricsEditorDialog::LyricsEditorDialog(Lyrics lyrics, PlayerController* playerController, LyricsSaver* lyricsSaver,
                                       SettingsManager* settings, QWidget* parent)
    : QDialog{parent}
    , m_editor{new LyricsEditor(std::move(lyrics), playerController, lyricsSaver, settings, this)}
{
    setWindowTitle(tr("Lyrics Editor"));

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_editor);

    auto* buttonBox
        = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel, this);

    QObject::connect(buttonBox->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, m_editor,
                     &PropertiesTabWidget::apply);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    layout->addWidget(buttonBox);
}

LyricsEditor* LyricsEditorDialog::editor() const
{
    return m_editor;
}

void LyricsEditorDialog::saveState()
{
    FyStateSettings stateSettings;
    Utils::saveState(this, stateSettings);
}

void LyricsEditorDialog::restoreState()
{
    const FyStateSettings stateSettings;
    Utils::restoreState(this, stateSettings);
}

void LyricsEditorDialog::accept()
{
    m_editor->apply();
    QDialog::accept();
}

QSize LyricsEditorDialog::sizeHint() const
{
    return m_editor->sizeHint();
}
} // namespace Fooyin::Lyrics
