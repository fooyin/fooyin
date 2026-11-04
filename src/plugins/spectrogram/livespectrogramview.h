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

#include "spectrogramview.h"

#include <core/engine/enginedefs.h>
#include <core/player/playerdefs.h>
#include <core/track.h>

#include <QBasicTimer>
#include <QElapsedTimer>
#include <QImage>
#include <QTimer>

namespace Fooyin {
class EngineController;
class PlayerController;

namespace Spectrogram {
class LiveSpectrogramView : public SpectrogramView
{
    Q_OBJECT

public:
    LiveSpectrogramView(PlayerController* playerController, EngineController* engine, QWidget* parent = nullptr);

    void applyConfig(const SpectrogramConfig& config) override;

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void updateTimerState();
    void resetAnalysis();
    void resizeBuffers();
    void normaliseHistoryForPresentationMode();
    void syncStreams();
    void setTrack(const Track& track);
    void setEngineState(Engine::PlaybackState state);
    void setPauseDrainActive(bool active);
    void handlePositionMoved(uint64_t positionMs);
    void handlePositionChanged(uint64_t positionMs);
    void renderFrame();
    bool analyseColumn(uint64_t endTimeMs);
    [[nodiscard]] QImage linearHistoryImage() const;
    [[nodiscard]] double oldestColumnTimeMs() const;

    PlayerController* m_playerController;
    VisualisationService* m_visualisationService;
    VisualisationSessionPtr m_monoSession;
    VisualisationSessionPtr m_leftSession;
    VisualisationSessionPtr m_rightSession;

    Track m_track;
    SpectrogramConfig m_config;
    QBasicTimer m_timer;

    QImage m_image;
    QImage m_resizeSource;
    int m_resizeSourceWritePixel;
    int m_resizeSourceHistoryPixelCount;
    QTimer m_resizeSettleTimer;
    qreal m_imageDpr;
    bool m_renderSizeReady;

    double m_pixelAdvanceRemainder;
    int m_writePixel;
    int m_historyPixelCount;
    int m_historyColumnCount;
    double m_newestColumnTimeMs;

    Player::PlayState m_playState;
    Engine::PlaybackState m_engineState;

    bool m_pauseDrainActive;
    uint64_t m_lastAnalysisTimeMs;
    bool m_analysisTimelineReady;
    uint64_t m_lastObservedAudioTimeMs;
    QElapsedTimer m_presentationClock;
    double m_presentationTimeMs;

    QElapsedTimer m_seekSettleClock;
    bool m_seekPending;
    bool m_seekAwaitingPosition;
    uint64_t m_requestedSeekPositionMs;
};
} // namespace Spectrogram
} // namespace Fooyin
