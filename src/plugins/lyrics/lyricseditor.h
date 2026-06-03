/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

class QTextEdit;

namespace Fooyin {
class PlayerController;

namespace Lyrics {
class LyricsEditor : public QWidget
{
    Q_OBJECT

public:
    explicit LyricsEditor(PlayerController* playerController, QWidget* parent = nullptr);

    void setTrack(const Track& track);
    void setLyrics(const Lyrics& lyrics);
    void setControlsEnabled(bool enabled);

    [[nodiscard]] QString text() const;
    [[nodiscard]] const Lyrics& currentLyrics() const;
    [[nodiscard]] Lyrics editedLyrics() const;

    void reset();

    [[nodiscard]] QSize sizeHint() const override;

Q_SIGNALS:
    void textEdited();
    void lyricsChanged();
    void resetClicked();

private:
    [[nodiscard]] Lyrics lyricsFromText(const QString& text) const;
    void setupUi();
    void setupConnections();
    void seek();
    void updateButtons();
    void highlightCurrentLine();
    void insertOrUpdateTimestamp();
    void insertOrUpdateTimestampAndGotoNextLine();
    void adjustTimestamp();
    void removeTimestamp();
    void removeAllTimestamps();

    Track m_track;
    Lyrics m_lyrics;
    PlayerController* m_playerController;

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
} // namespace Lyrics
} // namespace Fooyin
