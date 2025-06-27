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
#include "settings/lyricssettings.h"

#include <core/engine/audioengine.h>
#include <core/player/playerdefs.h>
#include <core/scripting/scriptparser.h>
#include <gui/fywidget.h>

#include <QBasicTimer>
#include <QPointer>

class QPropertyAnimation;
class QScrollArea;
class QTextEdit;

namespace Fooyin {
class EngineController;
class PlayerController;
class SettingsManager;
class Track;

namespace Lyrics {
class LyricsArea;
class LyricsFinder;
class LyricsSaver;
class LyricsScrollArea;

class LyricsWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit LyricsWidget(PlayerController* playerController, EngineController* engine, LyricsFinder* lyricsFinder,
                          LyricsSaver* lyricsSaver, SettingsManager* settings, QWidget* parent = nullptr);

    static QString defaultNoLyricsScript();

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

protected:
    void timerEvent(QTimerEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void updateLyrics(const Track& track);
    void loadLyrics(const Lyrics& lyrics);
    void changeLyrics(const Lyrics& lyrics);
    void openEditor(const Lyrics& lyrics);
    void playStateChanged(AudioEngine::PlaybackState state);

    void scrollToCurrentLine(int scrollValue);
    void updateScrollMode(ScrollMode mode);

    void checkStartAutoScrollPos(uint64_t pos);
    void checkStartAutoScroll(int startValue = -1);
    void updateAutoScroll(int startValue);

    PlayerController* m_playerController;
    EngineController* m_engine;
    SettingsManager* m_settings;

    LyricsScrollArea* m_scrollArea;
    LyricsArea* m_lyricsArea;

    LyricsFinder* m_lyricsFinder;
    LyricsSaver* m_lyricsSaver;

    Lyrics::Type m_type;
    Track m_currentTrack;
    std::vector<Lyrics> m_lyrics;
    QMetaObject::Connection m_finderConnection;
    ScriptParser m_parser;

    ScrollMode m_scrollMode;
    QPointer<QPropertyAnimation> m_scrollAnim;
    bool m_isUserScrolling;
    QBasicTimer m_scrollTimer;
};
} // namespace Lyrics
} // namespace Fooyin
