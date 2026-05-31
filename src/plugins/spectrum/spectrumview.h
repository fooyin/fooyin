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

#include "spectrumaxisrenderer.h"
#include "spectrumcolours.h"
#include "spectrumwidget.h"

#include <core/engine/visualisationservice.h>
#include <core/player/playerdefs.h>
#include <gui/widgets/tooltip.h>

#include <QBasicTimer>
#include <QElapsedTimer>
#include <QPointer>
#include <QWidget>

#include <vector>

namespace Fooyin {
class EngineController;
class PlayerController;

namespace Spectrum {
class SpectrumView : public QWidget
{
    Q_OBJECT

public:
    explicit SpectrumView(EngineController* engine, PlayerController* playerController, QWidget* parent = nullptr);

    void setConfig(const SpectrumWidget::ConfigData& config);
    void refreshStyleColours();
    [[nodiscard]] const SpectrumWidget::ConfigData& currentConfig() const;

Q_SIGNALS:
    void contextMenuRequested(const QPoint& pos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void changeEvent(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    struct BandBinRange
    {
        size_t startBin{1};
        size_t endBin{1};
    };

    void playStateChanged(Player::PlayState state);
    void currentTrackChanged();

    void startUpdateTimer();

    void clearLevels();

    [[nodiscard]] Colours colours() const;
    [[nodiscard]] QFont axisFont() const;
    [[nodiscard]] size_t effectiveBandCount() const;

    void tick();
    bool renderLatestSpectrum();
    bool updateLevelsFromSpectrum(const VisualisationSession::SpectrumWindow& spectrum);

    void paint(QPainter& painter, const QRect& rect, const QPalette& palette) const;

    void updateBarGeometry();

    [[nodiscard]] int barPixelHeight(float level) const;
    [[nodiscard]] float effectiveMinFrequencyHz() const;
    [[nodiscard]] float effectiveMaxFrequencyHz() const;
    [[nodiscard]] std::vector<QRectF> sectionRects(const QRectF& barRect) const;

    void drawBar(QPainter& painter, const QRectF& barRect, int barHeight, const QBrush& brush) const;
    void drawPeak(QPainter& painter, const QRectF& barRect, int peakHeight, const QColor& colour) const;
    void drawCurveSpectrum(QPainter& painter, const QBrush& brush) const;

    [[nodiscard]] int bandAtPosition(const QPoint& position) const;
    [[nodiscard]] int sourceBandForBar(int bar) const;
    [[nodiscard]] float frequencyForBand(int band) const;
    [[nodiscard]] int noteForBand(int band) const;
    [[nodiscard]] QString labelForBand(int band) const;
    [[nodiscard]] std::pair<QString, QString> toolTipContentForBand(int band) const;

    void hideToolTip();
    void refreshToolTip();
    void showToolTip(const QPoint& position);

    [[nodiscard]] QRect dirtyRectForBar(int index) const;
    [[nodiscard]] QRect dirtyRectForSpectrum() const;

    void invalidateStaticLayer();
    void updateDirtyRegion(QRegion& dirtyRegion);
    void repaintDirtyRegion(const QRegion& dirtyRegion);

    void invalidateBandMap();
    void ensureBandMap(const VisualisationSession::SpectrumWindow& spectrum);
    void ensureStaticLayer(const QPalette& palette) const;
    [[nodiscard]] QRectF staticLayerSourceRect(const QRect& logicalRect) const;

    VisualisationSessionPtr m_session;
    SpectrumWidget::ConfigData m_config;
    SpectrumAxisRenderer m_axisRenderer;
    std::vector<float> m_levels;
    std::vector<float> m_peakLevels;
    std::vector<int> m_barHeights;
    std::vector<int> m_peakHeights;
    std::vector<int> m_barHoldRemainingMs;
    std::vector<int> m_peakHoldRemainingMs;
    SpectrumPlotGeometry m_geometry;
    std::vector<QRectF> m_barRects;
    std::vector<int> m_barSourceBands;
    QRect m_spectrumRect;
    QRect m_plotRect;
    qreal m_geometryDpr;
    std::vector<BandBinRange> m_bandBinRanges;
    std::vector<size_t> m_bandKeys;
    std::vector<double> m_bandCenterBins;
    std::vector<int> m_lowResIndices;
    bool m_paused;
    bool m_stopped;
    QBasicTimer m_updateTimer;
    QElapsedTimer m_elapsedTimer;
    QPointer<ToolTip> m_toolTip;
    int m_toolTipBand;
    mutable QPixmap m_staticLayer;
    mutable qreal m_staticLayerDpr;
    int m_bandMapFftSize;
    int m_bandMapSampleRate;
    int m_bandMapBandCount;
    int m_bandMapMinFrequencyHz;
    int m_bandMapMaxFrequencyHz;
    int m_lowResEnd;
};
} // namespace Spectrum
} // namespace Fooyin
