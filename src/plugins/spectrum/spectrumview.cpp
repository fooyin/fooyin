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

#include "spectrumview.h"

#include <core/engine/enginecontroller.h>
#include <core/player/playercontroller.h>
#include <gui/widgets/gradienteditor.h>

#include <QContextMenuEvent>
#include <QFont>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRectF>
#include <QRegion>

#include <cmath>
#include <span>
#include <utility>

using namespace Qt::StringLiterals;

constexpr auto A4NoteIndex = 57;

namespace Fooyin::Spectrum {
namespace {
VisualisationSession::SpectrumWindowFunction toBackendWindowFunction(WindowFunction windowFunction)
{
    using BackendWindow = VisualisationSession::SpectrumWindowFunction;

    switch(windowFunction) {
        case WindowFunction::BlackmanHarris:
            return BackendWindow::BlackmanHarris;
        case WindowFunction::Hann:
            return BackendWindow::Hann;
        case WindowFunction::None:
            return BackendWindow::None;
    }

    return BackendWindow::BlackmanHarris;
}

float magnitudeToScale(float magnitude, int amplitudeMinDb, int amplitudeMaxDb)
{
    const float db    = 20.0F * std::log10(std::max(magnitude, 1.0e-9F));
    const float minDb = static_cast<float>(std::min(amplitudeMinDb, amplitudeMaxDb - 1));
    const float maxDb = static_cast<float>(std::max(amplitudeMaxDb, amplitudeMinDb + 1));
    return std::clamp((db - minDb) / (maxDb - minDb), 0.0F, 1.0F);
}

float sampleMagnitudeAtBin(std::span<const float> bins, double binPosition)
{
    if(bins.empty()) {
        return 0.0F;
    }

    const double clamped  = std::clamp(binPosition, 1.0, static_cast<double>(bins.size() - 1));
    const auto leftBin    = static_cast<size_t>(std::floor(clamped));
    const size_t rightBin = std::min(leftBin + 1, bins.size() - 1);
    const auto t          = static_cast<float>(clamped - static_cast<double>(leftBin));
    return bins[leftBin] + ((bins[rightBin] - bins[leftBin]) * t);
}

// Hermite-style interpolation
float interpolate(std::span<const float> values, size_t index, float t)
{
    if(values.empty()) {
        return 0.0F;
    }
    if(values.size() == 1) {
        return values.front();
    }

    const auto valueAt = [values](std::ptrdiff_t valueIndex) {
        if(valueIndex < 0) {
            return values.front() - (values[1] - values.front());
        }
        if(std::cmp_greater_equal(valueIndex, values.size())) {
            return values.back() + (values.back() - values[values.size() - 2]);
        }
        return values[valueIndex];
    };

    const auto start = static_cast<int>(index);
    const float y0   = valueAt(start - 1);
    const float y1   = valueAt(start);
    const float y2   = valueAt(start + 1);
    const float y3   = valueAt(start + 2);

    static constexpr float Tension = 0.35F;
    static constexpr float Bias    = 0.0F;

    const float t2 = t * t;
    const float t3 = t2 * t;
    float m0       = (y1 - y0) * (1.0F + Bias) * (1.0F - Tension) * 0.5F;
    m0 += (y2 - y1) * (1.0F - Bias) * (1.0F - Tension) * 0.5F;
    float m1 = (y2 - y1) * (1.0F + Bias) * (1.0F - Tension) * 0.5F;
    m1 += (y3 - y2) * (1.0F - Bias) * (1.0F - Tension) * 0.5F;

    const float a0 = (2.0F * t3) - (3.0F * t2) + 1.0F;
    const float a1 = t3 - (2.0F * t2) + t;
    const float a2 = t3 - t2;
    const float a3 = (-2.0F * t3) + (3.0F * t2);
    return (a0 * y1) + (a1 * m0) + (a2 * m1) + (a3 * y2);
}

int updateIntervalFromFps(int fps)
{
    const int clampedFps = std::clamp(fps, MinUpdateFps, MaxUpdateFps);
    return std::max(1, 1000 / clampedFps);
}

QFont fontFromString(const QString& fontString)
{
    QFont font;
    if(!fontString.isEmpty()) {
        font.fromString(fontString);
    }
    return font;
}

void drawBarOutline(QPainter& painter, const QRectF& barRect, int barHeight, const QPen& pen)
{
    barHeight = std::clamp(barHeight, 0, static_cast<int>(std::round(barRect.height())));
    if(barHeight <= 0) {
        return;
    }

    const QRectF rect{barRect.left(), barRect.bottom() - static_cast<double>(barHeight), barRect.width(),
                      static_cast<double>(barHeight)};
    painter.save();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(pen);
    painter.drawRect(rect.adjusted(0.5, 0.5, -0.5, -0.5));
    painter.restore();
}
} // namespace

SpectrumView::SpectrumView(EngineController* engine, PlayerController* playerController, QWidget* parent)
    : QWidget(parent)
    , m_session{engine->visualisationService()->createSession()}
    , m_geometryDpr{1.0}
    , m_paused{false}
    , m_stopped{true}
    , m_toolTipBand{-1}
    , m_staticLayerDpr{1.0}
    , m_bandMapFftSize{0}
    , m_bandMapSampleRate{0}
    , m_bandMapBandCount{0}
    , m_bandMapMinFrequencyHz{0}
    , m_bandMapMaxFrequencyHz{0}
    , m_lowResEnd{-1}
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true);

    m_axisRenderer.setConfig(m_config);

    m_session->requestBacklog(1000);

    m_elapsedTimer.start();
    playStateChanged(playerController ? playerController->playState() : Player::PlayState::Stopped);

    QObject::connect(playerController, &PlayerController::playStateChanged, this,
                     [this](const Player::PlayState state) { playStateChanged(state); });
    QObject::connect(playerController, &PlayerController::currentTrackChanged, this,
                     &SpectrumView::currentTrackChanged);
}

void SpectrumView::setConfig(const SpectrumWidget::ConfigData& config)
{
    m_config = config;
    m_axisRenderer.setConfig(m_config);

    const size_t bandCount = effectiveBandCount();
    m_levels.assign(bandCount, 0.0F);
    m_peakLevels.assign(bandCount, 0.0F);
    m_barHeights.assign(bandCount, 0);
    m_peakHeights.assign(bandCount, 0);
    m_barHoldRemainingMs.assign(bandCount, 0);
    m_peakHoldRemainingMs.assign(bandCount, 0);

    updateBarGeometry();
    invalidateBandMap();
    invalidateStaticLayer();

    m_session->requestBacklog(1000);

    if(m_updateTimer.isActive() && !m_paused) {
        startUpdateTimer();
    }
    if(!m_config.showTooltip) {
        hideToolTip();
    }

    update();
}

const SpectrumWidget::ConfigData& SpectrumView::currentConfig() const
{
    return m_config;
}

void SpectrumView::refreshStyleColours()
{
    invalidateStaticLayer();
    update();
}

void SpectrumView::paintEvent(QPaintEvent* /*event*/)
{
    // TODO: Use QEvent::DevicePixelRatioChange when we bump min Qt version to >= 6.6
    const qreal dpr = devicePixelRatioF();
    if(!qFuzzyCompare(m_geometryDpr, dpr)) {
        updateBarGeometry();
        invalidateStaticLayer();
    }

    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing, false);
    paint(painter, rect(), palette());
}

void SpectrumView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateBarGeometry();
    invalidateStaticLayer();
    update();
}

void SpectrumView::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_updateTimer.timerId()) {
        tick();
        return;
    }

    QWidget::timerEvent(event);
}

void SpectrumView::mouseMoveEvent(QMouseEvent* event)
{
    QWidget::mouseMoveEvent(event);
    showToolTip(event->pos());
}

void SpectrumView::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    hideToolTip();
}

void SpectrumView::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);

    switch(event->type()) {
        case QEvent::ApplicationPaletteChange:
        case QEvent::PaletteChange:
        case QEvent::StyleChange:
            refreshStyleColours();
            break;
        default:
            break;
    }
}

void SpectrumView::contextMenuEvent(QContextMenuEvent* event)
{
    Q_EMIT contextMenuRequested(event->globalPos());
}

void SpectrumView::playStateChanged(Player::PlayState state)
{
    m_paused  = state != Player::PlayState::Playing;
    m_stopped = state == Player::PlayState::Stopped;

    if(state == Player::PlayState::Stopped) {
        hideToolTip();
        clearLevels();
        return;
    }

    if(state == Player::PlayState::Playing) {
        startUpdateTimer();
    }
    else {
        m_updateTimer.stop();
    }
}

void SpectrumView::currentTrackChanged()
{
    if(m_paused || m_stopped) {
        clearLevels();
    }
}

void SpectrumView::startUpdateTimer()
{
    m_elapsedTimer.restart();
    m_updateTimer.start(updateIntervalFromFps(m_config.updateFps), Qt::PreciseTimer, this);
}

void SpectrumView::clearLevels()
{
    std::ranges::fill(m_levels, 0.0F);
    std::ranges::fill(m_peakLevels, 0.0F);
    std::ranges::fill(m_barHeights, 0);
    std::ranges::fill(m_peakHeights, 0);
    std::ranges::fill(m_barHoldRemainingMs, 0);
    std::ranges::fill(m_peakHoldRemainingMs, 0);

    if(m_paused) {
        m_updateTimer.stop();
    }
    else {
        startUpdateTimer();
    }

    update();
}

Colours SpectrumView::colours() const
{
    return m_config.colours.isValid() && m_config.colours.canConvert<Colours>() ? m_config.colours.value<Colours>()
                                                                                : Colours{};
}

QFont SpectrumView::axisFont() const
{
    return m_config.axisFont.isEmpty() ? SpectrumAxisRenderer::defaultAxisFont(font())
                                       : fontFromString(m_config.axisFont);
}

size_t SpectrumView::effectiveBandCount() const
{
    if(m_config.labelMode == LabelMode::Notes) {
        return static_cast<size_t>(std::max(1, m_config.maxNote - m_config.minNote + 1));
    }
    return static_cast<size_t>(std::max(1, m_config.bandCount));
}

void SpectrumView::tick()
{
    if(m_paused) {
        m_updateTimer.stop();
        return;
    }

    const int elapsedMs          = std::max(1, static_cast<int>(m_elapsedTimer.restart()));
    const float dbRange          = static_cast<float>(std::max(1, m_config.amplitudeMaxDb - m_config.amplitudeMinDb));
    const float barFalloffPerMs  = static_cast<float>(std::max(0, m_config.amplitudeGravity)) / dbRange / 1000.0F;
    const float peakFalloffPerMs = static_cast<float>(std::max(0, m_config.peakGravity)) / dbRange / 1000.0F;

    QRegion dirtyRegion;
    bool changed{false};

    for(size_t index{0}; index < m_levels.size(); ++index) {
        float& current = m_levels[index];
        int& barHoldMs = m_barHoldRemainingMs[index];
        float next{current};

        if(m_config.amplitudesEnabled && barHoldMs > 0) {
            barHoldMs = std::max(0, barHoldMs - elapsedMs);
        }
        else if(m_config.amplitudesEnabled) {
            next = std::max(0.0F, current - (barFalloffPerMs * static_cast<float>(elapsedMs)));
        }
        if(next != current) {
            current = next;
            changed = true;
        }

        if(m_config.peaksEnabled && index < m_peakLevels.size()) {
            float& peak = m_peakLevels[index];
            int& holdMs = m_peakHoldRemainingMs[index];

            if(holdMs > 0) {
                holdMs = std::max(0, holdMs - elapsedMs);
            }
            else {
                const float nextPeak = std::max(current, peak - (peakFalloffPerMs * static_cast<float>(elapsedMs)));
                if(nextPeak != peak) {
                    peak    = nextPeak;
                    changed = true;
                }
            }
        }
    }

    changed |= renderLatestSpectrum();

    if(changed) {
        updateDirtyRegion(dirtyRegion);
        repaintDirtyRegion(dirtyRegion);
        refreshToolTip();
    }
}

bool SpectrumView::renderLatestSpectrum()
{
    VisualisationSession::SpectrumWindow spectrum;
    const auto windowFunction = toBackendWindowFunction(m_config.windowFunction);
    const uint64_t endTimeMs  = m_session->currentTimeMs();
    const bool gotSpectrum
        = endTimeMs > 0 && m_session->getSpectrumWindowEndingAt(spectrum, endTimeMs, m_config.fftSize, windowFunction);

    if(!gotSpectrum) {
        return false;
    }

    return updateLevelsFromSpectrum(spectrum);
}

bool SpectrumView::updateLevelsFromSpectrum(const VisualisationSession::SpectrumWindow& spectrum)
{
    if(!spectrum.isValid() || spectrum.sampleRate <= 0 || spectrum.fftSize <= 0 || m_levels.empty()) {
        return false;
    }

    const auto bins = spectrum.bins();
    if(bins.empty()) {
        return false;
    }

    ensureBandMap(spectrum);
    std::vector<float> nextLevels(m_levels.size(), 0.0F);
    bool changed{false};

    for(size_t band{0}; band < m_levels.size() && band < m_bandBinRanges.size(); ++band) {
        const BandBinRange& range = m_bandBinRanges[band];
        float maxMagnitude{0.0F};
        for(size_t bin{range.startBin}; bin < range.endBin; ++bin) {
            maxMagnitude = std::max(maxMagnitude, bins[bin]);
        }

        nextLevels[band] = magnitudeToScale(maxMagnitude, m_config.amplitudeMinDb, m_config.amplitudeMaxDb);
    }

    if(m_lowResEnd >= 0 && m_config.interpolate && m_lowResIndices.size() >= 2) {
        std::vector<float> anchorLevels;
        anchorLevels.reserve(m_lowResIndices.size());

        for(const int anchor : m_lowResIndices) {
            const size_t band     = static_cast<size_t>(std::clamp(anchor, 0, static_cast<int>(m_levels.size()) - 1));
            const size_t key      = band < m_bandKeys.size() ? m_bandKeys[band] : 1;
            const float magnitude = key < bins.size() ? bins[key] : sampleMagnitudeAtBin(bins, m_bandCenterBins[band]);
            anchorLevels.push_back(magnitudeToScale(magnitude, m_config.amplitudeMinDb, m_config.amplitudeMaxDb));
        }

        const int limit = std::min(m_lowResEnd, static_cast<int>(nextLevels.size()) - 1);
        for(size_t anchorIndex{0}; anchorIndex + 1 < m_lowResIndices.size(); ++anchorIndex) {
            const int startBand = std::clamp(m_lowResIndices[anchorIndex], 0, limit + 1);
            const int endBand   = std::clamp(m_lowResIndices[anchorIndex + 1], startBand + 1, limit + 1);
            const int distance  = std::max(1, endBand - startBand);

            for(int band = startBand; band < endBand && band <= limit; ++band) {
                const float t    = static_cast<float>(band - startBand) / static_cast<float>(distance);
                nextLevels[band] = std::clamp(interpolate(anchorLevels, anchorIndex, t), 0.0F, 1.0F);
            }

            if(endBand > limit) {
                break;
            }
        }
    }
    else if(m_lowResEnd >= 0) {
        const int limit = std::min(m_lowResEnd, static_cast<int>(nextLevels.size()) - 1);
        for(int band{0}; band <= limit; ++band) {
            const double centerBin       = m_bandCenterBins[band];
            const float sampledMagnitude = sampleMagnitudeAtBin(bins, centerBin);
            nextLevels[band] = magnitudeToScale(sampledMagnitude, m_config.amplitudeMinDb, m_config.amplitudeMaxDb);
        }
    }

    const auto raisePeak = [this, &changed](size_t peakBand, float level) {
        if(!m_config.peaksEnabled || peakBand >= m_peakLevels.size() || level <= m_peakLevels[peakBand]) {
            return;
        }

        m_peakLevels[peakBand]          = level;
        m_peakHoldRemainingMs[peakBand] = m_config.peakHoldTimeMs;
        changed                         = true;
    };

    for(size_t band{0}; band < m_levels.size() && band < m_bandBinRanges.size(); ++band) {
        const float next = nextLevels[band];
        float& current   = m_levels[band];

        if(!m_config.amplitudesEnabled) {
            if(next != current) {
                current = next;
                changed = true;
            }

            if(band < m_barHoldRemainingMs.size()) {
                m_barHoldRemainingMs[band] = 0;
            }

            raisePeak(band, next);
            continue;
        }

        if(next <= current) {
            raisePeak(band, next);
            continue;
        }

        current = next;

        if(band < m_barHoldRemainingMs.size()) {
            m_barHoldRemainingMs[band] = m_config.amplitudeHoldTimeMs;
        }

        raisePeak(band, next);
        changed = true;
    }

    return changed;
}

void SpectrumView::paint(QPainter& painter, const QRect& rect, const QPalette& palette) const
{
    if(rect.width() <= 0 || rect.height() <= 0 || m_barRects.empty()) {
        return;
    }

    ensureStaticLayer(palette);
    if(!m_staticLayer.isNull()) {
        painter.drawPixmap(rect.topLeft(), m_staticLayer, staticLayerSourceRect(rect));
    }

    const Colours customColours{colours()};

    painter.setPen(Qt::NoPen);

    const QLinearGradient gradient = linearGradient(
        m_plotRect, m_config.gradientOrientation == GradientOrientation::Vertical ? Qt::Vertical : Qt::Horizontal,
        customColours.gradient(palette));

    if(m_config.drawStyle == DrawStyle::Curve) {
        drawCurveSpectrum(painter, QBrush{gradient});
    }
    else {
        for(size_t index{0}; index < m_barRects.size() && index < m_barHeights.size(); ++index) {
            const QRectF& barRect = m_barRects[index];
            const int barHeight   = m_barHeights[index];

            if(m_config.fillSpectrum) {
                drawBar(painter, barRect, barHeight, QBrush{gradient});
            }
            else {
                drawBarOutline(painter, barRect, barHeight, QPen{QBrush{gradient}, 1.0});
            }
        }
    }

    if(m_config.peaksEnabled) {
        const QColor peaksColour = customColours.colour(Colours::Type::Peaks, palette);
        for(size_t index{0}; index < m_barRects.size() && index < m_peakHeights.size(); ++index) {
            const QRectF& barRect = m_barRects[index];
            const int peakHeight  = m_peakHeights[index];
            drawPeak(painter, barRect, peakHeight, peaksColour);
        }
    }
}

void SpectrumView::updateBarGeometry()
{
    const int bandCount = static_cast<int>(std::max<size_t>(1, m_levels.size()));
    m_geometryDpr       = devicePixelRatioF();
    m_geometry          = m_axisRenderer.layout(rect(), axisFont(), bandCount, m_geometryDpr);
    m_barRects          = m_geometry.barRects;
    m_barSourceBands    = m_geometry.sourceBands;
    m_spectrumRect      = m_geometry.spectrumRect;
    m_plotRect          = m_geometry.plotRect;

    m_barHeights.assign(m_barRects.size(), 0);
    m_peakHeights.assign(m_barRects.size(), 0);
}

int SpectrumView::barPixelHeight(float level) const
{
    return std::clamp(static_cast<int>(std::round(level * static_cast<float>(m_plotRect.height()))), 0,
                      std::max(0, m_plotRect.height()));
}

float SpectrumView::effectiveMinFrequencyHz() const
{
    return m_axisRenderer.effectiveMinFrequencyHz();
}

float SpectrumView::effectiveMaxFrequencyHz() const
{
    return m_axisRenderer.effectiveMaxFrequencyHz();
}

std::vector<QRectF> SpectrumView::sectionRects(const QRectF& barRect) const
{
    if(barRect.isEmpty()) {
        return {};
    }

    int sectionCount  = std::clamp(m_config.barSections, MinBarSections, MaxBarSections);
    const int spacing = std::clamp(m_config.sectionSpacing, MinSectionSpacing, MaxSectionSpacing);

    while(sectionCount > 1
          && (barRect.height() - static_cast<double>(spacing * (sectionCount - 1)))
                 < static_cast<double>(sectionCount)) {
        --sectionCount;
    }

    std::vector<QRectF> rects;
    rects.reserve(sectionCount);

    if(sectionCount <= 1) {
        rects.emplace_back(barRect);
        return rects;
    }

    const auto totalSpacing    = static_cast<double>(spacing * (sectionCount - 1));
    const double sectionHeight = std::max(1.0, (barRect.height() - totalSpacing) / sectionCount);

    for(int section{0}; section < sectionCount; ++section) {
        const double endOffset
            = (static_cast<double>(section + 1) * sectionHeight) + static_cast<double>(section * spacing);
        const double y = barRect.bottom() - endOffset;
        rects.emplace_back(barRect.left(), y, barRect.width(), sectionHeight);
    }

    return rects;
}

void SpectrumView::drawBar(QPainter& painter, const QRectF& barRect, int barHeight, const QBrush& brush) const
{
    barHeight = std::clamp(barHeight, 0, static_cast<int>(std::round(barRect.height())));
    if(barHeight <= 0) {
        return;
    }

    const QRectF fillRect{barRect.left(), barRect.bottom() - static_cast<double>(barHeight), barRect.width(),
                          static_cast<double>(barHeight)};
    const auto sections = sectionRects(barRect);
    for(const QRectF& section : sections) {
        const QRectF visibleSection = section.intersected(fillRect);
        if(!visibleSection.isEmpty()) {
            painter.fillRect(visibleSection, brush);
        }
    }
}

void SpectrumView::drawPeak(QPainter& painter, const QRectF& barRect, int peakHeight, const QColor& colour) const
{
    peakHeight = std::clamp(peakHeight, 0, static_cast<int>(std::round(barRect.height())));
    if(peakHeight <= 0) {
        return;
    }

    const double peakY = std::clamp(barRect.bottom() - static_cast<double>(peakHeight),
                                    static_cast<double>(m_plotRect.top()), static_cast<double>(m_plotRect.bottom()));
    painter.fillRect(QRectF{barRect.left(), peakY, barRect.width(), 1.0}, colour);
}

void SpectrumView::drawCurveSpectrum(QPainter& painter, const QBrush& brush) const
{
    const auto count = static_cast<int>(std::min<>(m_barRects.size(), m_barHeights.size()));
    if(count <= 0 || m_plotRect.isEmpty()) {
        return;
    }

    QPainterPath path;
    path.moveTo(m_barRects.front().left(), m_plotRect.bottom());

    for(int index{0}; index < count; ++index) {
        const QRectF& barRect = m_barRects[index];
        const int height      = m_barHeights[index];
        path.lineTo(barRect.center().x(), m_plotRect.bottom() - static_cast<double>(height));
    }

    if(m_config.fillSpectrum) {
        path.lineTo(m_barRects[count - 1].right(), m_plotRect.bottom());
        path.closeSubpath();
        painter.fillPath(path, brush);
    }
    else {
        painter.save();
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen{brush, 1.0});
        painter.drawPath(path);
        painter.restore();
    }
}

int SpectrumView::bandAtPosition(const QPoint& position) const
{
    if(m_stopped || !m_config.showTooltip || !m_spectrumRect.contains(position) || m_barRects.empty()) {
        return -1;
    }

    for(size_t index{0}; index < m_barRects.size(); ++index) {
        if(m_barRects[index].toAlignedRect().contains(position)) {
            return sourceBandForBar(static_cast<int>(index));
        }
    }

    if(position.x() < m_plotRect.left() || position.x() > m_plotRect.right() || m_plotRect.width() <= 0) {
        return -1;
    }

    const int barCount = static_cast<int>(m_barRects.size());
    const double t     = static_cast<double>(position.x() - m_plotRect.left())
                       / static_cast<double>(std::max(1, m_plotRect.width() - 1));
    const int bar = std::clamp(static_cast<int>(std::round(t * static_cast<double>(barCount - 1))), 0, barCount - 1);
    return sourceBandForBar(bar);
}

int SpectrumView::sourceBandForBar(int bar) const
{
    if(bar < 0 || std::cmp_greater_equal(bar, m_barSourceBands.size()) || m_levels.empty()) {
        return -1;
    }
    return std::clamp(m_barSourceBands[bar], 0, static_cast<int>(m_levels.size()) - 1);
}

float SpectrumView::frequencyForBand(int band) const
{
    return m_axisRenderer.frequencyForBand(band, static_cast<int>(m_levels.size()));
}

int SpectrumView::noteForBand(int band) const
{
    return m_axisRenderer.noteForBand(band, static_cast<int>(m_levels.size()));
}

QString SpectrumView::labelForBand(int band) const
{
    if(m_config.labelMode != LabelMode::Notes) {
        return SpectrumAxisRenderer::formatTooltipFrequency(frequencyForBand(band));
    }

    return SpectrumAxisRenderer::formatNoteLabel(noteForBand(band));
}

std::pair<QString, QString> SpectrumView::toolTipContentForBand(int band) const
{
    if(band < 0 || std::cmp_greater_equal(band, m_levels.size())) {
        return {};
    }

    const float level = std::clamp(m_levels[band], 0.0F, 1.0F);
    const int dbValue = static_cast<int>(
        std::round(static_cast<float>(m_config.amplitudeMinDb)
                   + (static_cast<float>(m_config.amplitudeMaxDb - m_config.amplitudeMinDb) * level)));
    const QString dbText = QString::number(dbValue) + u".00 dB"_s;

    if(m_config.labelMode == LabelMode::Notes) {
        return {SpectrumAxisRenderer::formatTooltipFrequency(frequencyForBand(band)) + u" ("_s + labelForBand(band)
                    + u")"_s,
                dbText};
    }

    return {labelForBand(band), dbText};
}

void SpectrumView::hideToolTip()
{
    if(m_toolTip) {
        m_toolTip->hide();
    }
    m_toolTipBand = -1;
}

void SpectrumView::refreshToolTip()
{
    if(m_toolTipBand < 0 || !m_toolTip || !m_toolTip->isVisible()) {
        return;
    }
    if(m_stopped) {
        hideToolTip();
        return;
    }

    const auto [text, subtext] = toolTipContentForBand(m_toolTipBand);
    if(text.isEmpty()) {
        hideToolTip();
        return;
    }

    m_toolTip->setContent(text, subtext);
}

void SpectrumView::showToolTip(const QPoint& position)
{
    const int band = bandAtPosition(position);
    if(band < 0) {
        hideToolTip();
        return;
    }

    const auto [text, subtext] = toolTipContentForBand(band);
    if(text.isEmpty()) {
        hideToolTip();
        return;
    }

    if(!m_toolTip) {
        m_toolTip = new ToolTip(window());
    }

    m_toolTipBand = band;
    m_toolTip->setContent(text, subtext);
    m_toolTip->show();
    m_toolTip->raise();

    static constexpr int Offset{8};

    QPoint toolTipPos     = mapTo(window(), position + QPoint{Offset, 0});
    const int windowWidth = window() ? window()->width() : width();

    if(toolTipPos.x() + m_toolTip->width() > windowWidth) {
        toolTipPos = mapTo(window(), position - QPoint{Offset, 0});
        m_toolTip->setPosition(toolTipPos, Qt::AlignRight);
    }
    else {
        m_toolTip->setPosition(toolTipPos, Qt::AlignLeft);
    }
}

QRect SpectrumView::dirtyRectForBar(int index) const
{
    if(index < 0 || std::cmp_greater_equal(index, m_barRects.size())) {
        return {};
    }

    QRect dirty = m_barRects[index].toAlignedRect().adjusted(-1, 0, 1, 0);
    dirty.setTop(m_spectrumRect.top());
    return dirty;
}

QRect SpectrumView::dirtyRectForSpectrum() const
{
    return m_spectrumRect.adjusted(-1, 0, 1, 0);
}

void SpectrumView::invalidateStaticLayer()
{
    m_staticLayer = QPixmap{};
}

void SpectrumView::updateDirtyRegion(QRegion& dirtyRegion)
{
    if(m_config.drawStyle == DrawStyle::Curve) {
        dirtyRegion += dirtyRectForSpectrum();

        for(size_t index{0}; index < m_barHeights.size() && index < m_barRects.size(); ++index) {
            const int sourceBand = sourceBandForBar(static_cast<int>(index));
            if(sourceBand < 0) {
                continue;
            }

            m_barHeights[index]  = barPixelHeight(m_levels[sourceBand]);
            m_peakHeights[index] = std::cmp_less(sourceBand, m_peakLevels.size()) && m_config.peaksEnabled
                                     ? barPixelHeight(m_peakLevels[sourceBand])
                                     : 0;
        }
        return;
    }

    for(size_t index{0}; index < m_barHeights.size() && index < m_barRects.size(); ++index) {
        const int sourceBand = sourceBandForBar(static_cast<int>(index));
        if(sourceBand < 0) {
            continue;
        }

        const int newHeight     = barPixelHeight(m_levels[sourceBand]);
        const int newPeakHeight = std::cmp_less(sourceBand, m_peakLevels.size()) && m_config.peaksEnabled
                                    ? barPixelHeight(m_peakLevels[sourceBand])
                                    : 0;

        if(newHeight == m_barHeights[index] && newPeakHeight == m_peakHeights[index]) {
            continue;
        }

        m_barHeights[index]  = newHeight;
        m_peakHeights[index] = newPeakHeight;
        dirtyRegion += dirtyRectForBar(static_cast<int>(index));
    }
}

void SpectrumView::repaintDirtyRegion(const QRegion& dirtyRegion)
{
    if(!dirtyRegion.isEmpty()) {
        repaint(dirtyRegion.boundingRect());
    }
}

void SpectrumView::invalidateBandMap()
{
    m_bandBinRanges.clear();
    m_bandKeys.clear();
    m_bandCenterBins.clear();
    m_lowResIndices.clear();

    m_lowResEnd             = -1;
    m_bandMapFftSize        = 0;
    m_bandMapSampleRate     = 0;
    m_bandMapBandCount      = 0;
    m_bandMapMinFrequencyHz = 0;
    m_bandMapMaxFrequencyHz = 0;
}

void SpectrumView::ensureBandMap(const VisualisationSession::SpectrumWindow& spectrum)
{
    const size_t bandCount = m_levels.size();
    if(bandCount == 0 || !spectrum.isValid() || spectrum.sampleRate <= 0 || spectrum.fftSize <= 0) {
        invalidateBandMap();
        return;
    }

    if(m_bandMapFftSize == spectrum.fftSize && m_bandMapSampleRate == spectrum.sampleRate
       && std::cmp_equal(m_bandMapBandCount, bandCount) && m_bandMapMinFrequencyHz == m_config.minFrequencyHz
       && m_bandMapMaxFrequencyHz == m_config.maxFrequencyHz && m_bandBinRanges.size() == bandCount) {
        return;
    }

    m_bandBinRanges.assign(bandCount, {});
    m_bandKeys.assign(bandCount, 1);
    m_bandCenterBins.assign(bandCount, 1.0);
    m_lowResIndices.clear();
    m_lowResEnd = -1;

    const auto bins      = spectrum.bins();
    const float nyquist  = static_cast<float>(spectrum.sampleRate) * 0.5F;
    const float minFreq  = std::clamp(effectiveMinFrequencyHz(), 1.0F, nyquist);
    const float maxFreq  = std::clamp(effectiveMaxFrequencyHz(), minFreq + 1.0F, nyquist);
    const float binFreq  = static_cast<float>(spectrum.sampleRate) / static_cast<float>(spectrum.fftSize);
    const size_t lastBin = bins.empty() ? 0 : (bins.size() - 1);

    if(m_config.labelMode == LabelMode::Notes) {
        const int noteCount   = std::max(1, m_config.maxNote - m_config.minNote + 1);
        const double noteSize = static_cast<double>(bandCount) / static_cast<double>(noteCount);
        const double a4Pos    = (static_cast<double>(A4NoteIndex + m_config.transpose - m_config.minNote)) * noteSize;
        const double octave   = 12.0 * noteSize;

        for(size_t band{0}; band < bandCount; ++band) {
            const double noteFrequency
                = static_cast<double>(m_config.pitchHz) * std::pow(2.0, (static_cast<double>(band) - a4Pos) / octave);
            const double centerBin = noteFrequency / static_cast<double>(binFreq);
            const size_t keyBin
                = std::clamp(static_cast<size_t>(std::llround(centerBin)), static_cast<size_t>(1), lastBin);
            m_bandCenterBins[band] = std::clamp(centerBin, 1.0, static_cast<double>(lastBin));
            m_bandKeys[band]       = keyBin;
        }
    }
    else {
        const double ratio = static_cast<double>(maxFreq) / static_cast<double>(minFreq);
        for(size_t band{0}; band < bandCount; ++band) {
            const double t = bandCount > 1 ? static_cast<double>(band) / static_cast<double>(bandCount - 1) : 0.0;
            const double centerFreq = static_cast<double>(minFreq) * std::pow(ratio, t);
            const double centerBin  = centerFreq / static_cast<double>(binFreq);
            const size_t keyBin
                = std::clamp(static_cast<size_t>(std::llround(centerBin)), static_cast<size_t>(1), lastBin);
            m_bandCenterBins[band] = std::clamp(centerBin, 1.0, static_cast<double>(lastBin));
            m_bandKeys[band]       = keyBin;
        }
    }

    for(size_t band{0}; band < bandCount; ++band) {
        const size_t k0 = m_bandKeys[band > 0 ? band - 1 : 0];
        const size_t k1 = m_bandKeys[band];
        const size_t k2 = m_bandKeys[std::min(band + 1, bandCount - 1)];

        auto startBin = static_cast<size_t>(std::ceil((static_cast<double>(k1 - k0) * 0.5) + static_cast<double>(k0)));
        auto endBin   = static_cast<size_t>(std::ceil((static_cast<double>(k2 - k1) * 0.5) + static_cast<double>(k1)));

        startBin = std::clamp(startBin, static_cast<size_t>(1), lastBin);
        endBin   = std::clamp(endBin, static_cast<size_t>(1), lastBin);

        if(startBin >= endBin) {
            endBin = std::min(lastBin + 1, startBin + 1);
        }
        else {
            endBin = std::min(lastBin + 1, endBin);
        }

        m_bandBinRanges[band] = {.startBin = startBin, .endBin = endBin};
    }

    for(size_t band{1}; band < bandCount; ++band) {
        if(m_bandKeys[band] == m_bandKeys[band - 1]) {
            m_lowResEnd = static_cast<int>(band);
        }
    }

    if(m_lowResEnd > 0) {
        size_t lastKey{0};
        bool haveLastKey{false};

        for(int band = 0; band <= m_lowResEnd; ++band) {
            const size_t key = m_bandKeys[band];
            if(!haveLastKey || key != lastKey) {
                m_lowResIndices.push_back(band);
                lastKey     = key;
                haveLastKey = true;
            }
        }

        for(int band = m_lowResEnd + 1; std::cmp_less(band, bandCount) && band < (m_lowResEnd + 4); ++band) {
            m_lowResIndices.push_back(band);
        }
    }

    m_bandMapFftSize        = spectrum.fftSize;
    m_bandMapSampleRate     = spectrum.sampleRate;
    m_bandMapBandCount      = static_cast<int>(bandCount);
    m_bandMapMinFrequencyHz = m_config.minFrequencyHz;
    m_bandMapMaxFrequencyHz = m_config.maxFrequencyHz;
}

void SpectrumView::ensureStaticLayer(const QPalette& palette) const
{
    const QSize size       = this->size();
    const qreal dpr        = devicePixelRatioF();
    const QSize targetSize = {static_cast<int>(std::ceil(static_cast<qreal>(size.width()) * dpr)),
                              static_cast<int>(std::ceil(static_cast<qreal>(size.height()) * dpr))};

    if(!m_staticLayer.isNull() && m_staticLayer.size() == targetSize
       && qFuzzyCompare(m_staticLayer.devicePixelRatio(), dpr)) {
        return;
    }

    m_staticLayer = QPixmap{targetSize};
    m_staticLayer.setDevicePixelRatio(dpr);
    m_staticLayerDpr = dpr;

    QPainter painter{&m_staticLayer};
    m_axisRenderer.paint(painter, m_geometry, size, palette, axisFont());
}

QRectF SpectrumView::staticLayerSourceRect(const QRect& logicalRect) const
{
    return {static_cast<qreal>(logicalRect.x()) * m_staticLayerDpr,
            static_cast<qreal>(logicalRect.y()) * m_staticLayerDpr,
            static_cast<qreal>(logicalRect.width()) * m_staticLayerDpr,
            static_cast<qreal>(logicalRect.height()) * m_staticLayerDpr};
}
} // namespace Fooyin::Spectrum

#include "moc_spectrumview.cpp"
