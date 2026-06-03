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
#include "settings/lyricssettings.h"

#include <core/engine/enginedefs.h>
#include <core/scripting/scriptparser.h>
#include <gui/fywidget.h>

#include <QBasicTimer>
#include <QMargins>
#include <QPointer>
#include <QVariant>

class QJsonObject;
class QPropertyAnimation;

namespace Fooyin {
class EngineController;
class PlayerController;
class PlaylistHandler;
class SettingsManager;
class Track;
struct RichText;

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
    explicit LyricsWidget(PlayerController* playerController, PlaylistHandler* playlistHandler,
                          LyricsFinder* lyricsFinder, LyricsSaver* lyricsSaver, SettingsManager* settings,
                          QWidget* parent = nullptr);

    static QString defaultNoLyricsScript();

    void updateLyrics(const Track& track, bool force = false);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

    struct ConfigData
    {
        bool seekOnClick{true};
        QString noLyricsScript{defaultNoLyricsScript()};
        int scrollDuration{500};
        int scrollMode{static_cast<int>(ScrollMode::Synced)};
        int edgeFadeMode{static_cast<int>(EdgeFadeMode::Off)};
        int edgeFadeSize{10};
        bool showScrollbar{true};
        int alignment{Qt::AlignCenter};
        int lineSpacing{5};
        bool centreFirstSyncedLine{false};
        bool centreLastSyncedLine{true};
        QMargins margins{Defaults::margins()};
        QVariant colours;
        QString baseFont;
        QString lineFont;
        QString wordLineFont;
        QString wordFont;
    };

    [[nodiscard]] ConfigData factoryConfig() const;
    [[nodiscard]] ConfigData defaultConfig() const;
    [[nodiscard]] const ConfigData& currentConfig() const;
    void saveDefaults(const ConfigData& config) const;
    void clearSavedDefaults() const;
    void applyConfig(const ConfigData& config);

protected:
    void changeEvent(QEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

    void openConfigDialog() override;

private:
    void loadLyrics(const Lyrics& lyrics);
    void handleLyricsSearchFinished(const Track& track, bool foundAny);
    void handleSavedLyrics(const Track& track, const Lyrics& lyrics);
    void changeLyrics(const Lyrics& lyrics, const Track* sourceTrack = nullptr);
    void openEditor(const Lyrics& lyrics);
    void openSearchDialog();

    [[nodiscard]] ConfigData configFromLayout(const QJsonObject& layout) const;
    void saveConfigToLayout(const ConfigData& config, QJsonObject& layout) const;

    void playStateChanged(Player::PlayState state);

    void setCurrentTime(uint64_t time);
    [[nodiscard]] RichText noLyricsDisplayText(const Track& track);
    void updateViewportPadding();
    void seekTo(const QModelIndex& index, const QPoint& pos);

    void highlightCurrentLine();
    void scrollToCurrentLine(int scrollValue);
    void updateScrollMode(ScrollMode mode);

    void checkStartAutoScrollPos(uint64_t pos);
    void checkStartAutoScroll(int startValue = -1);
    void updateAutoScroll(int startValue);
    void updateEdgeFadeState();
    [[nodiscard]] bool shouldEnableEdgeFade() const;

    PlayerController* m_playerController;
    PlaylistHandler* m_playlistHandler;
    SettingsManager* m_settings;

    LyricsView* m_lyricsView;
    LyricsModel* m_model;
    LyricsDelegate* m_delegate;

    LyricsFinder* m_lyricsFinder;
    LyricsSaver* m_lyricsSaver;

    Lyrics m_currentLyrics;
    uint64_t m_currentTime;
    int m_currentLineStart;
    int m_currentLineEnd;

    Track m_currentTrack;
    std::vector<Lyrics> m_lyrics;
    QMetaObject::Connection m_finderConnection;
    ScriptParser m_parser;

    ConfigData m_config;
    ScrollMode m_scrollMode;
    QPointer<QPropertyAnimation> m_scrollAnim;
    bool m_isUserScrolling;
    QBasicTimer m_scrollTimer;
};
} // namespace Lyrics
} // namespace Fooyin
