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

#pragma once

#include "lyrics.h"

#include <core/network/networkaccessmanager.h>
#include <gui/propertiesdialog.h>

#include <QDialog>

#include <unordered_map>

class QTextEdit;

namespace Fooyin {
class PlayerController;
class SettingsManager;

namespace Lyrics {
class LyricsFinder;
class LyricsSaver;

class LyricsEditor : public PropertiesTabWidget
{
    Q_OBJECT

public:
    using TrackEditable = std::function<bool(const Track&)>;

    LyricsEditor(const Track& track, std::shared_ptr<NetworkAccessManager> networkAccess, LyricsSaver* lyricsSaver,
                 PlayerController* playerController, SettingsManager* settings, TrackEditable canEditTrack = {},
                 QWidget* parent = nullptr);
    LyricsEditor(Lyrics lyrics, PlayerController* playerController, LyricsSaver* lyricsSaver, SettingsManager* settings,
                 QWidget* parent = nullptr);

    void updateTrack(const Track& track);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void setTrackScope(const TrackList& tracks) override;
    [[nodiscard]] bool isAvailableForScope(const TrackList& tracks) const override;
    [[nodiscard]] bool hasPendingScopeChanges() const override;
    bool commitPendingChanges() override;

    void apply() override;
    void finish() override;

    [[nodiscard]] QSize sizeHint() const override;

signals:
    void lyricsEdited(const Fooyin::Lyrics::Lyrics& lyrics);

private:
    struct Draft
    {
        Track originalTagTrack;
        Lyrics originalLyrics;
        Lyrics workingLyrics;
        bool originalLyricsLoaded{false};
        bool dirty{false};
    };

    void updateLyrics(const Track& track, const Lyrics& lyrics);

    static QString trackKey(const Track& track);
    [[nodiscard]] Draft& ensureDraft(const Track& track);
    [[nodiscard]] Draft* currentDraft();
    [[nodiscard]] const Draft* currentDraft() const;
    [[nodiscard]] Lyrics lyricsFromText(const QString& text) const;

    void loadCurrentDraft();
    void storeCurrentDraftText();
    void setControlsEnabled();
    void updatePendingState();

    void setupUi();
    void setupConnections();
    void reset();
    void seek();
    void updateButtons();
    void highlightCurrentLine();
    void insertOrUpdateTimestamp();
    void insertOrUpdateTimestampAndGotoNextLine();
    void adjustTimestamp();
    void removeTimestamp();
    void removeAllTimestamps();

    Track m_track;
    LyricsSaver* m_lyricsSaver;
    PlayerController* m_playerController;
    SettingsManager* m_settings;
    std::shared_ptr<NetworkAccessManager> m_networkAccess;
    LyricsFinder* m_lyricsFinder;
    Lyrics m_lyrics;
    TrackEditable m_canEditTrack;
    std::unordered_map<QString, Draft> m_drafts;
    std::unordered_map<QString, Track> m_pendingTagTracks;
    QString m_currentTrackKey;
    bool m_hasPendingScopeChanges;

    QPushButton* m_playPause;
    QPushButton* m_seek;
    QPushButton* m_reset;

    QPushButton* m_insert;
    QPushButton* m_insertNext;
    QPushButton* m_rewind;
    QPushButton* m_forward;
    QPushButton* m_remove;
    QPushButton* m_removeAll;

    QTextEdit* m_lyricsText;
    QColor m_currentLineColour;
};

class LyricsEditorDialog : public QDialog
{
    Q_OBJECT

public:
    LyricsEditorDialog(Lyrics lyrics, PlayerController* playerController, LyricsSaver* lyricsSaver,
                       SettingsManager* settings, QWidget* parent = nullptr);

    [[nodiscard]] LyricsEditor* editor() const;

    void saveState();
    void restoreState();

    void accept() override;

    [[nodiscard]] QSize sizeHint() const override;

private:
    LyricsEditor* m_editor;
};
} // namespace Lyrics
} // namespace Fooyin
