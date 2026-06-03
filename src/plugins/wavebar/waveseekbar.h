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

#include "settings/wavebarsettings.h"
#include "wavebarcolours.h"
#include "waveformdata.h"

#include <core/player/playerdefs.h>
#include <gui/widgets/tooltip.h>

#include <QImage>
#include <QPointer>
#include <QWidget>

namespace Fooyin::WaveBar {
class WaveSeekBar : public QWidget
{
    Q_OBJECT

public:
    explicit WaveSeekBar(QWidget* parent = nullptr);

    void processData(const WaveformData<float>& waveData);

    void setPlayState(Player::PlayState state);
    void setSeekable(bool seekable);
    void setPosition(uint64_t pos);
    void setShowCursor(bool show);
    void setCursorWidth(int width);
    void setChannelScale(double scale);
    void setBarWidth(int width);
    void setBarGap(int gap);
    void setMaxScale(double scale);
    void setCentreGap(int gap);
    void setMode(WaveModes mode);
    void setColours(const Colours& colours);
    void setSupersampleFactor(int factor);
    void setMouseFocusEnabled(bool enabled);

    [[nodiscard]] bool isSeeking() const;
    void stopSeeking();

Q_SIGNALS:
    void sliderMoved(uint64_t pos);
    void seekForward();
    void seekBackward();

protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    enum class PlaybackColourMode : uint8_t
    {
        Position = 0,
        Unplayed,
        Played
    };

    [[nodiscard]] double positionFromValue(double value) const;
    [[nodiscard]] double positionFromValue(double value, double renderWidth) const;
    [[nodiscard]] uint64_t valueFromPosition(int pos) const;
    void updateMousePosition(const QPoint& pos);
    void updateRange(double first, double last);

    void invalidateWaveformCache();
    void ensureWaveformCache();
    [[nodiscard]] int cachedRenderWidth() const;
    void drawCachedWaveform(QPainter& painter, const QRect& dirtyRect);
    void drawCachedSlice(QPainter& painter, const QImage& image, const QRectF& targetRect) const;
    void drawCachedTransition(QPainter& painter, double positionX, const QRect& targetRect);
    void drawCursors(QPainter& painter);
    void paintWaveform(QPainter& painter, const QRect& rect, double renderWidth,
                       PlaybackColourMode colourMode = PlaybackColourMode::Position);
    void drawChannel(QPainter& painter, int channel, double height, int first, int last, int y, double renderWidth,
                     PlaybackColourMode colourMode);
    void drawSilence(QPainter& painter, double first, double last, double y, double currentPosition,
                     PlaybackColourMode colourMode);
    void drawSeekTip();

    void invalidate();

    Player::PlayState m_playState;
    bool m_seekable;
    WaveformData<float> m_data;
    double m_scale;
    uint64_t m_position;
    QPoint m_pressPos;
    QPoint m_seekPos;
    QPointer<ToolTip> m_seekTip;

    bool m_showCursor;
    int m_cursorWidth;
    double m_channelScale;
    int m_barWidth;
    int m_barGap;
    int m_sampleWidth;
    int m_supersampleFactor;
    double m_maxScale;
    int m_centreGap;

    WaveModes m_mode;
    Colours m_colours;

    QImage m_unplayedWaveformCache;
    QImage m_playedWaveformCache;
    QSize m_waveformCacheSize;
    int m_waveformCacheRenderWidth;
    bool m_waveformCacheDirty;
};
} // namespace Fooyin::WaveBar
