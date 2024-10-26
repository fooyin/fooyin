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

#include <QDialog>

class QTextEdit;

namespace Fooyin {
class PlayerController;
class SettingsManager;

namespace Lyrics {
class LyricsEditor : public QDialog
{
    Q_OBJECT

public:
    explicit LyricsEditor(Lyrics lyrics, PlayerController* playerController, SettingsManager* settings,
                          QWidget* parent = nullptr);

    void accept() override;
    void apply();

    [[nodiscard]] QSize sizeHint() const override;

signals:
    void lyricsEdited(const Fooyin::Lyrics::Lyrics& lyrics);

private:
    void reset();
    void seek();
    void updateButtons();
    void highlightCurrentLine();
    void insertOrUpdateTimestamp();
    void removeTimestamp();
    void removeAllTimestamps();

    PlayerController* m_playerController;
    SettingsManager* m_settings;
    Lyrics m_lyrics;

    QPushButton* m_playPause;
    QPushButton* m_seek;
    QPushButton* m_reset;

    QPushButton* m_insert;
    QPushButton* m_remove;
    QPushButton* m_removeAll;

    QTextEdit* m_lyricsText;
    QColor m_currentLineColour;
};
} // namespace Lyrics
} // namespace Fooyin
