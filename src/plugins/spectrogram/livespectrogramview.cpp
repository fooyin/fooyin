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

#include "livespectrogramview.h"

#include "spectrogramutils.h"

#include <core/engine/enginecontroller.h>
#include <core/player/playercontroller.h>

#include <QLoggingCategory>
#include <QPainter>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimerEvent>

#include <algorithm>
#include <cmath>
#include <utility>

Q_LOGGING_CATEGORY(SPECTROGRAM, "fy.spectrogram")

constexpr auto BacklogMs                     = 5000;
constexpr uint64_t AnalysisHopMs             = 25;
constexpr auto PresentationIntervalMs        = 8;
constexpr auto MaximumCatchUpColumns         = 16;
constexpr uint64_t BackwardsClockToleranceMs = 250;
constexpr auto PresentationResetThresholdMs  = 300.0;
constexpr auto SeekSettleMs                  = 150;
constexpr auto SeekPositionTimeoutMs         = 1000;

namespace Fooyin::Spectrogram {
LiveSpectrogramView::LiveSpectrogramView(PlayerController* playerController, EngineController* engine, QWidget* parent)
    : SpectrogramView{parent}
    , m_playerController{playerController}
    , m_visualisationService{engine->visualisationService()}
    , m_track{playerController->currentTrack()}
    , m_resizeSourceWritePixel{0}
    , m_resizeSourceHistoryPixelCount{0}
    , m_imageDpr{1.0}
    , m_renderSizeReady{false}
    , m_pixelAdvanceRemainder{0.0}
    , m_writePixel{0}
    , m_historyPixelCount{0}
    , m_historyColumnCount{0}
    , m_newestColumnTimeMs{0.0}
    , m_playState{playerController->playState()}
    , m_engineState{engine->engineState()}
    , m_pauseDrainActive{false}
    , m_lastAnalysisTimeMs{0}
    , m_analysisTimelineReady{false}
    , m_lastObservedAudioTimeMs{0}
    , m_presentationTimeMs{0.0}
    , m_seekPending{false}
    , m_seekAwaitingPosition{false}
    , m_requestedSeekPositionMs{0}
{
    m_resizeSettleTimer.setSingleShot(true);
    m_resizeSettleTimer.setInterval(150);
    QObject::connect(&m_resizeSettleTimer, &QTimer::timeout, this, [this]() { m_resizeSource = {}; });

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, &LiveSpectrogramView::setTrack);
    QObject::connect(m_playerController, &PlayerController::playStateChanged, this,
                     [this](Player::PlayState state) { m_playState = state; });
    QObject::connect(m_playerController, &PlayerController::positionMoved, this,
                     &LiveSpectrogramView::handlePositionMoved);
    QObject::connect(m_playerController, &PlayerController::positionChanged, this,
                     &LiveSpectrogramView::handlePositionChanged);

    QObject::connect(engine, &EngineController::engineStateChanged, this, &LiveSpectrogramView::setEngineState);
    QObject::connect(engine, &EngineController::audiblePauseDrainStarted, this,
                     [this]() { setPauseDrainActive(true); });
    QObject::connect(engine, &EngineController::audiblePauseDrainCompleted, this,
                     [this]() { setPauseDrainActive(false); });
}

void LiveSpectrogramView::applyConfig(const SpectrogramConfig& config)
{
    const auto previousMode{m_config.presentationMode};
    m_config = config;

    if(previousMode != m_config.presentationMode) {
        normaliseHistoryForPresentationMode();
    }

    syncStreams();
    updateTimerState();
    renderFrame();
    update();
}

void LiveSpectrogramView::showEvent(QShowEvent* event)
{
    SpectrogramView::showEvent(event);

    QMetaObject::invokeMethod(
        this,
        [this]() {
            if(isVisible()) {
                m_renderSizeReady = true;
                renderFrame();
            }
        },
        Qt::QueuedConnection);
}

void LiveSpectrogramView::resizeEvent(QResizeEvent* event)
{
    SpectrogramView::resizeEvent(event);

    if(!isVisible()) {
        return;
    }

    if(m_resizeSource.isNull()) {
        if(m_config.presentationMode == PresentationMode::Stationary) {
            m_resizeSource                  = m_image;
            m_resizeSourceWritePixel        = m_writePixel;
            m_resizeSourceHistoryPixelCount = m_historyPixelCount;
        }
        else {
            m_resizeSource = linearHistoryImage();
        }
    }

    m_resizeSettleTimer.start();
    resizeBuffers();
    update();
}

void LiveSpectrogramView::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_timer.timerId()) {
        renderFrame();
        return;
    }

    SpectrogramView::timerEvent(event);
}

void LiveSpectrogramView::paintEvent(QPaintEvent* /*event*/)
{
    if(!qFuzzyCompare(m_imageDpr, devicePixelRatioF())) {
        resizeBuffers();
    }

    QPainter painter{this};
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.fillRect(rect(), m_config.colours.front());

    if(m_config.presentationMode == PresentationMode::Stationary) {
        if(!m_image.isNull() && m_historyPixelCount > 0) {
            painter.drawImage(QRectF{rect()}, m_image, QRectF{m_image.rect()});
        }
        return;
    }

    if(!m_image.isNull() && m_historyPixelCount > 0) {
        const int imageWidth = m_image.width();
        const int oldest     = (m_writePixel - m_historyPixelCount + imageWidth) % imageWidth;
        const double offset
            = std::clamp((m_presentationTimeMs - m_newestColumnTimeMs) / static_cast<double>(AnalysisHopMs), 0.0, 1.0);
        double targetX = (static_cast<double>(imageWidth - m_historyPixelCount) / m_imageDpr) - offset;
        int remaining{m_historyPixelCount};
        int sourceX{oldest};

        while(remaining > 0) {
            const int pixels       = std::min(remaining, imageWidth - sourceX);
            const double drawWidth = static_cast<double>(pixels) / m_imageDpr;
            painter.drawImage(QRectF{targetX, 0.0, drawWidth, static_cast<double>(height())}, m_image,
                              QRectF{static_cast<double>(sourceX), 0.0, static_cast<double>(pixels),
                                     static_cast<double>(m_image.height())});
            targetX += drawWidth;
            remaining -= pixels;
            sourceX = 0;
        }

        if(offset > 0.0) {
            const int newest = (m_writePixel - 1 + imageWidth) % imageWidth;
            painter.drawImage(QRectF{static_cast<double>(width()) - offset, 0.0, offset, static_cast<double>(height())},
                              m_image,
                              QRectF{static_cast<double>(newest), 0.0, 1.0, static_cast<double>(m_image.height())});
        }
    }
}

void LiveSpectrogramView::updateTimerState()
{
    if(m_engineState != Engine::PlaybackState::Playing && !m_pauseDrainActive) {
        if(m_timer.isActive()) {
            m_timer.stop();
        }
        return;
    }

    m_timer.start(PresentationIntervalMs, Qt::PreciseTimer, this);
}

void LiveSpectrogramView::resetAnalysis()
{
    if(!m_image.isNull()) {
        m_image.fill(Qt::transparent);
    }

    m_writePixel              = 0;
    m_historyPixelCount       = 0;
    m_historyColumnCount      = 0;
    m_pixelAdvanceRemainder   = 0.0;
    m_newestColumnTimeMs      = 0.0;
    m_lastAnalysisTimeMs      = 0;
    m_analysisTimelineReady   = false;
    m_lastObservedAudioTimeMs = 0;
    m_presentationClock.invalidate();
    m_presentationTimeMs = 0.0;
    m_seekSettleClock.invalidate();
    m_seekPending             = false;
    m_seekAwaitingPosition    = false;
    m_requestedSeekPositionMs = 0;
}

void LiveSpectrogramView::resizeBuffers()
{
    const qreal dpr = devicePixelRatioF();
    const QSize targetSize{static_cast<int>(std::ceil(static_cast<qreal>(width()) * dpr)),
                           static_cast<int>(std::ceil(static_cast<qreal>(height()) * dpr))};
    if(targetSize.width() <= 0 || targetSize.height() <= 0) {
        return;
    }

    if(m_image.size() == targetSize && qFuzzyCompare(m_imageDpr, dpr)) {
        return;
    }

    const bool stationary        = m_config.presentationMode == PresentationMode::Stationary;
    const bool haveResizeSource  = !m_resizeSource.isNull();
    const QImage previousHistory = haveResizeSource ? m_resizeSource : (stationary ? m_image : linearHistoryImage());
    const int sourceWritePixel   = haveResizeSource ? m_resizeSourceWritePixel : m_writePixel;
    const int sourceHistoryPixelCount = haveResizeSource ? m_resizeSourceHistoryPixelCount : m_historyPixelCount;
    const bool hadHistory
        = !previousHistory.isNull() && (stationary ? sourceHistoryPixelCount > 0 : m_historyPixelCount > 0);

    m_image = QImage{targetSize, QImage::Format_ARGB32_Premultiplied};
    m_image.fill(Qt::transparent);
    m_imageDpr              = dpr;
    m_writePixel            = 0;
    m_historyPixelCount     = 0;
    m_historyColumnCount    = 0;
    m_pixelAdvanceRemainder = 0.0;

    if(hadHistory) {
        m_image = previousHistory.scaled(targetSize, Qt::IgnoreAspectRatio, Qt::FastTransformation);

        if(stationary) {
            const auto horizontalScale
                = static_cast<double>(targetSize.width()) / static_cast<double>(previousHistory.width());
            m_writePixel        = static_cast<int>(std::lround(static_cast<double>(sourceWritePixel) * horizontalScale))
                                % targetSize.width();
            m_historyPixelCount = std::clamp(
                static_cast<int>(std::lround(static_cast<double>(sourceHistoryPixelCount) * horizontalScale)), 1,
                targetSize.width());
            m_historyColumnCount = std::clamp(
                static_cast<int>(std::lround(static_cast<double>(width()) * static_cast<double>(m_historyPixelCount)
                                             / static_cast<double>(targetSize.width()))),
                1, width());
        }
        else {
            m_historyPixelCount  = targetSize.width();
            m_historyColumnCount = width();
        }
    }
}

void LiveSpectrogramView::normaliseHistoryForPresentationMode()
{
    if(m_image.isNull() || m_historyPixelCount <= 0) {
        return;
    }

    const QImage linearHistory = linearHistoryImage();
    QImage compactHistory{m_image.size(), m_image.format()};
    compactHistory.fill(Qt::transparent);

    const int sourceX = linearHistory.width() - m_historyPixelCount;
    QPainter painter{&compactHistory};
    painter.drawImage(QPoint{}, linearHistory, QRect{sourceX, 0, m_historyPixelCount, linearHistory.height()});
    painter.end();

    m_image        = std::move(compactHistory);
    m_writePixel   = m_historyPixelCount % m_image.width();
    m_resizeSource = {};
    m_resizeSettleTimer.stop();
}

void LiveSpectrogramView::syncStreams()
{
    bool needsMono{false};
    bool needsLeft{false};
    bool needsRight{false};

    const bool stereoTrack = m_track.channels() >= 2;
    switch(m_config.channelMode) {
        case ChannelMode::Unchanged:
            needsMono  = !stereoTrack;
            needsLeft  = stereoTrack;
            needsRight = stereoTrack;
            break;
        case ChannelMode::Mono:
            needsMono = true;
            break;
        case ChannelMode::FrontOnly:
            needsLeft = true;
            break;
        case ChannelMode::BackOnly:
            needsMono  = !stereoTrack;
            needsRight = stereoTrack;
            break;
    }

    const auto ensureSession
        = [this](VisualisationSessionPtr& session, VisualisationSession::ChannelSelection channelSelection) {
              if(!session) {
                  session = m_visualisationService->createSession();
                  session->setChannelSelection(channelSelection);
              }
              session->requestBacklog(BacklogMs);
          };

    if(needsMono) {
        ensureSession(m_monoSession, {.mixMode = VisualisationSession::ChannelSelection::MixMode::MonoAverage});
    }
    if(needsLeft) {
        ensureSession(m_leftSession,
                      {.mixMode = VisualisationSession::ChannelSelection::MixMode::SingleChannel, .channelIndex = 0});
    }
    if(needsRight) {
        ensureSession(m_rightSession,
                      {.mixMode = VisualisationSession::ChannelSelection::MixMode::SingleChannel, .channelIndex = 1});
    }

    if(!needsMono) {
        m_monoSession.reset();
    }
    if(!needsLeft) {
        m_leftSession.reset();
    }
    if(!needsRight) {
        m_rightSession.reset();
    }
}

void LiveSpectrogramView::setTrack(const Track& track)
{
    if(!track.isValid()) {
        m_presentationClock.invalidate();
        update();
        return;
    }

    const bool trackChanged = m_track.isValid() && !m_track.sameIdentityAs(track);

    m_track = track;
    syncStreams();
    m_presentationClock.invalidate();
    m_lastObservedAudioTimeMs = 0;

    if(trackChanged && m_config.clearOnTrackChange) {
        resetAnalysis();
    }

    renderFrame();
    update();
}

void LiveSpectrogramView::setEngineState(Engine::PlaybackState state)
{
    const auto prevState = std::exchange(m_engineState, state);

    if(state == Engine::PlaybackState::Playing || state == Engine::PlaybackState::Stopped
       || state == Engine::PlaybackState::Error) {
        m_pauseDrainActive = false;
    }

    updateTimerState();

    if(state == Engine::PlaybackState::Playing && prevState != Engine::PlaybackState::Playing) {
        m_presentationClock.invalidate();
        m_seekPending          = true;
        m_seekAwaitingPosition = false;
        m_seekSettleClock.restart();
    }
}

void LiveSpectrogramView::setPauseDrainActive(bool active)
{
    if(std::exchange(m_pauseDrainActive, active) == active) {
        return;
    }

    if(!active) {
        m_presentationClock.invalidate();
    }
    updateTimerState();
    update();
}

void LiveSpectrogramView::handlePositionMoved(uint64_t positionMs)
{
    m_seekPending             = true;
    m_seekAwaitingPosition    = true;
    m_requestedSeekPositionMs = positionMs;
    m_seekSettleClock.restart();
}

void LiveSpectrogramView::handlePositionChanged(uint64_t positionMs)
{
    if(!m_seekPending || !m_seekAwaitingPosition) {
        return;
    }

    const uint64_t deltaMs = positionMs > m_requestedSeekPositionMs ? positionMs - m_requestedSeekPositionMs
                                                                    : m_requestedSeekPositionMs - positionMs;

    if(deltaMs <= BackwardsClockToleranceMs) {
        m_seekAwaitingPosition = false;
        m_seekSettleClock.restart();
    }
}

void LiveSpectrogramView::renderFrame()
{
    // Wait until the containing layout knows the first visible size
    if(!m_renderSizeReady) {
        return;
    }

    if(isVisible()) {
        resizeBuffers();
    }

    if(m_image.isNull() || !m_track.isValid()) {
        update();
        return;
    }

    if(m_engineState != Engine::PlaybackState::Playing && !m_pauseDrainActive) {
        return;
    }

    const auto& clockSession = m_monoSession ? m_monoSession : (m_leftSession ? m_leftSession : m_rightSession);
    if(!clockSession) {
        update();
        return;
    }

    const uint64_t rawCurrentTimeMs = clockSession->currentTimeMs();
    if(m_seekPending) {
        if(m_seekAwaitingPosition) {
            if(!m_seekSettleClock.isValid() || m_seekSettleClock.elapsed() < SeekPositionTimeoutMs) {
                return;
            }
            qCWarning(SPECTROGRAM) << "Seek position confirmation timed out; settling against audio clock:"
                                   << "targetPositionMs=" << m_requestedSeekPositionMs
                                   << "rawAudioTimeMs=" << rawCurrentTimeMs;
            m_seekAwaitingPosition = false;
            m_seekSettleClock.restart();
        }

        if(!m_seekSettleClock.isValid() || m_seekSettleClock.elapsed() < SeekSettleMs
           || rawCurrentTimeMs < AnalysisHopMs) {
            return;
        }

        m_seekPending          = false;
        m_seekAwaitingPosition = false;
        m_seekSettleClock.invalidate();
        m_presentationClock.invalidate();
        m_lastObservedAudioTimeMs = 0;

        const bool timelineRestarted
            = !m_analysisTimelineReady || rawCurrentTimeMs + BackwardsClockToleranceMs < m_lastAnalysisTimeMs;
        if(timelineRestarted && m_historyColumnCount > 0) {
            const double shiftMs = -m_newestColumnTimeMs;
            m_newestColumnTimeMs += shiftMs;
        }

        if(timelineRestarted) {
            m_lastAnalysisTimeMs    = 0;
            m_analysisTimelineReady = true;
        }
    }

    uint64_t observedAudioTimeMs{rawCurrentTimeMs};
    if(m_lastObservedAudioTimeMs > 0 && rawCurrentTimeMs < m_lastObservedAudioTimeMs) {
        const uint64_t backwardsByMs = m_lastObservedAudioTimeMs - rawCurrentTimeMs;
        if(backwardsByMs <= BackwardsClockToleranceMs) {
            observedAudioTimeMs = m_lastObservedAudioTimeMs;
        }
    }

    m_lastObservedAudioTimeMs = observedAudioTimeMs;

    if(!m_presentationClock.isValid()) {
        m_presentationClock.start();
        m_presentationTimeMs = static_cast<double>(observedAudioTimeMs);
    }
    else {
        const double elapsedMs = static_cast<double>(m_presentationClock.nsecsElapsed()) / 1'000'000.0;
        m_presentationClock.restart();
        const double predictedTimeMs = m_presentationTimeMs + std::max(0.0, elapsedMs);
        const double clockErrorMs    = static_cast<double>(observedAudioTimeMs) - predictedTimeMs;

        if(std::abs(clockErrorMs) > PresentationResetThresholdMs) {
            qCWarning(SPECTROGRAM) << "Presentation clock discontinuity; freezing for settle:"
                                   << "rawAudioTimeMs=" << observedAudioTimeMs << "predictedTimeMs=" << predictedTimeMs
                                   << "errorMs=" << clockErrorMs << "columns=" << m_historyColumnCount;
            m_seekPending          = true;
            m_seekAwaitingPosition = false;
            m_seekSettleClock.restart();
            return;
        }

        m_presentationTimeMs = predictedTimeMs;
    }

    const auto presentationTimeMs        = static_cast<uint64_t>(std::max(0.0, m_presentationTimeMs));
    const uint64_t alignedAnalysisTimeMs = (presentationTimeMs / AnalysisHopMs) * AnalysisHopMs;
    if(alignedAnalysisTimeMs > 0) {
        const uint64_t maximumCatchUpMs = AnalysisHopMs * static_cast<uint64_t>(MaximumCatchUpColumns);
        const bool analysisClockReset   = !m_analysisTimelineReady || alignedAnalysisTimeMs < m_lastAnalysisTimeMs
                                       || alignedAnalysisTimeMs - m_lastAnalysisTimeMs > maximumCatchUpMs;
        if(analysisClockReset) {
            const uint64_t nextAnalysisBaseMs = alignedAnalysisTimeMs - AnalysisHopMs;

            if(m_historyColumnCount > 0) {
                const double shiftMs = static_cast<double>(nextAnalysisBaseMs) - m_newestColumnTimeMs;
                m_newestColumnTimeMs += shiftMs;
            }
            m_lastAnalysisTimeMs    = nextAnalysisBaseMs;
            m_analysisTimelineReady = true;
        }

        while(m_lastAnalysisTimeMs + AnalysisHopMs <= alignedAnalysisTimeMs) {
            const uint64_t columnTimeMs = m_lastAnalysisTimeMs + AnalysisHopMs;
            if(!analyseColumn(columnTimeMs)) {
                break;
            }
            m_lastAnalysisTimeMs = columnTimeMs;
        }
    }

    update();
}

bool LiveSpectrogramView::analyseColumn(uint64_t endTimeMs)
{
    if(m_image.isNull()) {
        return false;
    }

    VisualisationSession::SpectrumWindow monoSpectrum;
    VisualisationSession::SpectrumWindow leftSpectrum;
    VisualisationSession::SpectrumWindow rightSpectrum;

    const auto windowFunction = static_cast<VisualisationSession::SpectrumWindowFunction>(m_config.windowFunction);

    bool splitStereo{false};
    bool haveSingleSpectrum{false};

    if(m_config.channelMode == ChannelMode::Unchanged && m_track.channels() >= 2 && m_leftSession && m_rightSession) {
        splitStereo
            = m_leftSession->getSpectrumWindowEndingAt(leftSpectrum, endTimeMs, m_config.fftSize, windowFunction)
           && m_rightSession->getSpectrumWindowEndingAt(rightSpectrum, endTimeMs, m_config.fftSize, windowFunction);
    }
    else if(m_config.channelMode == ChannelMode::FrontOnly && m_leftSession) {
        haveSingleSpectrum
            = m_leftSession->getSpectrumWindowEndingAt(monoSpectrum, endTimeMs, m_config.fftSize, windowFunction);
    }
    else if(m_config.channelMode == ChannelMode::BackOnly && m_track.channels() >= 2 && m_rightSession) {
        haveSingleSpectrum
            = m_rightSession->getSpectrumWindowEndingAt(monoSpectrum, endTimeMs, m_config.fftSize, windowFunction);
    }

    if(!splitStereo && !haveSingleSpectrum) {
        if(!m_monoSession
           || !m_monoSession->getSpectrumWindowEndingAt(monoSpectrum, endTimeMs, m_config.fftSize, windowFunction)) {
            return false;
        }
    }

    std::vector<float> levels(static_cast<size_t>(m_image.height()), -1.0F);
    const auto& colours = m_config.colours;

    if(splitStereo && m_image.height() >= 2) {
        const int requestedSpacing
            = static_cast<int>(std::lround(static_cast<double>(m_config.channelSpacing) * devicePixelRatioF()));
        const int spacing      = std::min(requestedSpacing, m_image.height() - 2);
        const int topHeight    = (m_image.height() - spacing) / 2;
        const int bottomTop    = topHeight + spacing;
        const int bottomHeight = m_image.height() - bottomTop;

        fillSpectrumSection(levels, 0, topHeight, leftSpectrum, m_config);
        fillSpectrumSection(levels, bottomTop, bottomHeight, rightSpectrum, m_config);
    }
    else {
        fillSpectrumSection(levels, 0, m_image.height(), splitStereo ? leftSpectrum : monoSpectrum, m_config);
    }

    m_pixelAdvanceRemainder += static_cast<double>(m_imageDpr);
    const int pixelsToWrite = std::max(1, static_cast<int>(std::floor(m_pixelAdvanceRemainder)));
    m_pixelAdvanceRemainder -= static_cast<double>(pixelsToWrite);

    static constexpr QRgb Background = 0;

    std::array<QRgb, 256> colourTable;
    for(size_t index{0}; index < colourTable.size(); ++index) {
        colourTable[index] = qPremultiply(colourForLevel(static_cast<double>(index) / 255.0, colours).rgba());
    }

    for(int row{0}; row < m_image.height(); ++row) {
        const float level     = levels[static_cast<size_t>(row)];
        auto* scanLine        = reinterpret_cast<QRgb*>(m_image.scanLine(row));
        const int colourIndex = std::clamp(static_cast<int>(std::lround(static_cast<double>(level) * 255.0)), 0, 255);
        const QRgb colour     = level < 0.0F ? Background : colourTable[static_cast<size_t>(colourIndex)];

        for(int pixel{0}; pixel < pixelsToWrite; ++pixel) {
            scanLine[(m_writePixel + pixel) % m_image.width()] = colour;
        }
    }

    m_writePixel         = (m_writePixel + pixelsToWrite) % m_image.width();
    m_historyPixelCount  = std::min(m_historyPixelCount + pixelsToWrite, m_image.width());
    m_historyColumnCount = std::min(m_historyColumnCount + 1, width());
    m_newestColumnTimeMs = static_cast<double>(endTimeMs);

    return true;
}

QImage LiveSpectrogramView::linearHistoryImage() const
{
    if(m_image.isNull() || m_historyPixelCount <= 0) {
        return {};
    }

    QImage history{m_image.size(), m_image.format()};
    history.fill(Qt::transparent);

    const int oldest      = (m_writePixel - m_historyPixelCount + m_image.width()) % m_image.width();
    const int targetX     = m_image.width() - m_historyPixelCount;
    const int firstPixels = std::min(m_historyPixelCount, m_image.width() - oldest);

    QPainter painter{&history};
    painter.drawImage(QPoint{targetX, 0}, m_image, QRect{oldest, 0, firstPixels, m_image.height()});
    const int secondPixels = m_historyPixelCount - firstPixels;
    if(secondPixels > 0) {
        painter.drawImage(QPoint{targetX + firstPixels, 0}, m_image, QRect{0, 0, secondPixels, m_image.height()});
    }

    return history;
}

double LiveSpectrogramView::oldestColumnTimeMs() const
{
    return m_historyColumnCount > 0
             ? m_newestColumnTimeMs
                   - (static_cast<double>(m_historyColumnCount - 1) * static_cast<double>(AnalysisHopMs))
             : 0.0;
}
} // namespace Fooyin::Spectrogram
