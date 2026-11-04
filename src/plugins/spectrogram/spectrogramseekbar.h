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

#pragma once

#include "spectrogramgenerator.h"
#include "spectrogramview.h"

#include <gui/widgets/tooltip.h>

#include <QBasicTimer>
#include <QPointer>
#include <QThread>
#include <QWidget>

#include <optional>

namespace Fooyin {
class ABRepeatControls;
class PlayerController;
class SettingsManager;
class TrackSelectionController;

namespace Spectrogram {
class SpectrogramSeekBar : public SpectrogramView
{
    Q_OBJECT

public:
    SpectrogramSeekBar(std::shared_ptr<AudioLoader> audioLoader, PlayerController* playerController,
                       TrackSelectionController* trackSelection, SettingsManager* settings, QWidget* parent);
    ~SpectrogramSeekBar() override;

    void applyConfig(const SpectrogramConfig& config) override;
    bool cancelSeeking() override;

    [[nodiscard]] ABRepeatControls* repeatControls() const;
    [[nodiscard]] bool isSeeking() const;
    void stopSeeking();

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    bool refreshTrack();
    bool setTrack(const Track& track);
    [[nodiscard]] Track preferredTrack() const;
    [[nodiscard]] bool displayingCurrentTrack() const;

    void generate();
    void rebuildImage(int firstColumn = 0);
    void scheduleImageResize();
    [[nodiscard]] int targetImageHeight() const;

    void drawAxes(QPainter& painter, const QRect& plot);
    void drawTimeAxes(QPainter& painter, const QRect& plot);
    void drawFrequencyAxes(QPainter& painter, const QRect& plot);
    void previewPosition(const QPoint& position);

    void showSeekTip(const QPoint& position);
    void hideSeekTip();
    void seekStep(bool forward);

    [[nodiscard]] QRect plotRect() const;
    [[nodiscard]] uint64_t positionAt(int x) const;
    [[nodiscard]] bool canSeek() const;

    ABRepeatControls* m_repeatControls;
    PlayerController* m_playerController;
    SettingsManager* m_settings;
    TrackSelectionController* m_trackSelection;

    SpectrogramGenerator m_generator;
    QThread m_generatorThread;
    SpectrogramConfig m_config;
    Track m_track;
    std::vector<SpectrogramColumn> m_columns;
    QImage m_image;
    QBasicTimer m_resizeTimer;
    qreal m_imageDpr;
    uint64_t m_request;
    uint64_t m_position;
    std::optional<uint64_t> m_previewPosition;
    QPoint m_pressPosition;
    QPoint m_mousePosition;
    QPointer<ToolTip> m_seekTip;
    int m_channels;
    int m_completedColumns;
};
} // namespace Spectrogram
} // namespace Fooyin
