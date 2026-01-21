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
#include <core/scripting/scriptparser.h>
#include <gui/fywidget.h>

#include <QBasicTimer>
#include <QPointer>

class QPropertyAnimation;

namespace Fooyin {
class EngineController;
class PlayerController;
class SettingsManager;
class Track;

namespace Lyrics {
class LyricsView;
class LyricsDelegate;
class LyricsFinder;
class LyricsModel;
class LyricsSaver;

class LyricsWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit LyricsWidget(PlayerController* playerController, EngineController* engine, LyricsFinder* lyricsFinder,
                          LyricsSaver* lyricsSaver, SettingsManager* settings, QWidget* parent = nullptr);

    static QString defaultNoLyricsScript();

    void updateLyrics(const Track& track, bool force = false);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

protected:
    void timerEvent(QTimerEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void loadLyrics(const Lyrics& lyrics);
    void changeLyrics(const Lyrics& lyrics);
    void openEditor(const Lyrics& lyrics);
    void playStateChanged(AudioEngine::PlaybackState state);

    void setCurrentTime(uint64_t time);
    void seekTo(const QModelIndex& index, const QPoint& pos);

    void highlightCurrentLine();
    void scrollToCurrentLine(int scrollValue);
    void updateScrollMode(ScrollMode mode);

    void checkStartAutoScrollPos(uint64_t pos);
    void checkStartAutoScroll(int startValue = -1);
    void updateAutoScroll(int startValue);

    PlayerController* m_playerController;
    EngineController* m_engine;
    SettingsManager* m_settings;

    LyricsView* m_lyricsView;
    LyricsModel* m_model;
    LyricsDelegate* m_delegate;

    LyricsFinder* m_lyricsFinder;
    LyricsSaver* m_lyricsSaver;

    Lyrics m_currentLyrics;
    uint64_t m_currentTime;
    int m_currentLine;

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
