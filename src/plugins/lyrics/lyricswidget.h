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

#include "lyricsparser.h"

#include <gui/fywidget.h>

class QScrollArea;
class QTextEdit;

namespace Fooyin {
class PlayerController;
class SettingsManager;
class Track;

namespace Lyrics {
class LyricsArea;

class LyricsWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit LyricsWidget(PlayerController* playerController, SettingsManager* settings, QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void updateLyrics(const Track& track);
    void scrollToCurrentLine(int scrollValue);

    PlayerController* m_playerController;
    SettingsManager* m_settings;

    QScrollArea* m_scrollArea;
    LyricsArea* m_lyricsArea;
};
} // namespace Lyrics
} // namespace Fooyin
