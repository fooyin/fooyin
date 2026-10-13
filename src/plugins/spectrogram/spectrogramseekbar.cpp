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

#include "spectrogramseekbar.h"

#include "spectrogramutils.h"

#include <core/player/playercontroller.h>
#include <gui/guisettings.h>
#include <gui/trackselectioncontroller.h>
#include <utils/settings/settingsmanager.h>
#include <utils/signalthrottler.h>
#include <utils/stringutils.h>

#include <QApplication>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLoggingCategory>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTimerEvent>
#include <QWheelEvent>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <span>

Q_LOGGING_CATEGORY(SPECTROGRAM_SEEKBAR, "fy.spectrogram")

using namespace Qt::StringLiterals;

constexpr auto ToolTipDelay    = 5;
constexpr size_t MaxImageBytes = 128ULL * 1024 * 1024;

namespace Fooyin::Spectrogram {
namespace {
int selectAxisStep(std::span<const int> steps, double scale, double minSpacing)
{
    for(const int step : steps) {
        if(scale * step >= minSpacing) {
            return step;
        }
    }
    return 0;
}
} // namespace

SpectrogramSeekBar::SpectrogramSeekBar(std::shared_ptr<AudioLoader> audioLoader, PlayerController* playerController,
                                       TrackSelectionController* trackSelection, SettingsManager* settings,
                                       QWidget* parent)
    : SpectrogramView{parent}
    , m_playerController{playerController}
    , m_settings{settings}
    , m_trackSelection{trackSelection}
    , m_generator{std::move(audioLoader)}
    , m_track{playerController->currentTrack()}
    , m_imageDpr{1.0}
    , m_request{0}
    , m_position{playerController->currentPosition()}
    , m_channels{0}
    , m_completedColumns{0}
{
    setMouseTracking(true);

    const auto setMouseFocus = [this](bool enabled) {
        setFocusPolicy(enabled ? Qt::StrongFocus : Qt::TabFocus);
    };
    setMouseFocus(m_settings->value<Settings::Gui::SeekBarMouseFocus>());
    m_settings->subscribe<Settings::Gui::SeekBarMouseFocus>(this, setMouseFocus);

    m_generator.moveToThread(&m_generatorThread);

    QObject::connect(&m_generator, &SpectrogramGenerator::columnsReady, this,
                     [this](uint64_t request, const SpectrogramBatch& batch) {
                         if(request != m_request) {
                             return;
                         }
                         if(m_columns.empty()) {
                             m_columns.resize(static_cast<size_t>(batch.totalColumns));
                             m_channels = batch.channels;
                         }
                         std::ranges::copy(batch.columns, m_columns.begin() + batch.firstColumn);
                         m_completedColumns = batch.firstColumn + static_cast<int>(batch.columns.size());
                         rebuildImage(batch.firstColumn);
                         update();
                     });
    QObject::connect(&m_generator, &SpectrogramGenerator::finished, this,
                     [this](uint64_t request, const QString& error) {
                         if(request == m_request && !error.isEmpty()) {
                             qCWarning(SPECTROGRAM_SEEKBAR)
                                 << "Unable to generate spectrogram for track:" << m_track.filepath() << error;
                         }
                     });

    auto* throttler = new SignalThrottler(this);
    throttler->setTimeout(std::chrono::milliseconds{150});
    QObject::connect(playerController, &PlayerController::currentTrackChanged, throttler, &SignalThrottler::throttle);
    QObject::connect(m_trackSelection, &TrackSelectionController::displaySelectionChanged, throttler,
                     &SignalThrottler::throttle);
    QObject::connect(throttler, &SignalThrottler::triggered, this, &SpectrogramSeekBar::refreshTrack);
    QObject::connect(playerController, &PlayerController::currentTrackUpdated, this, &SpectrogramSeekBar::refreshTrack);
    QObject::connect(playerController, &PlayerController::positionChanged, this, [this](uint64_t position) {
        m_position = displayingCurrentTrack() ? position : 0;
        if(isSeeking() && !QApplication::mouseButtons().testFlag(Qt::LeftButton)) {
            stopSeeking();
        }
        if(isSeeking() && m_seekTip) {
            showSeekTip(m_mousePosition);
        }
        update();
    });
    QObject::connect(playerController, &PlayerController::positionMoved, this, [this](uint64_t position) {
        m_position = displayingCurrentTrack() ? position : 0;
        update();
    });
    QObject::connect(playerController, &PlayerController::playStateChanged, this, [this](Player::PlayState) {
        stopSeeking();
        refreshTrack();
    });
    QObject::connect(playerController, &PlayerController::currentTrackSeekableChanged, this, [this](bool seekable) {
        if(!seekable) {
            stopSeeking();
        }
        update();
    });
}

SpectrogramSeekBar::~SpectrogramSeekBar()
{
    hideSeekTip();

    m_generator.cancel();
    m_generator.closeThread();
    m_generatorThread.quit();
    m_generatorThread.wait();
}

void SpectrogramSeekBar::applyConfig(const SpectrogramConfig& config)
{
    const bool analysisChanged = m_config.fftSize != config.fftSize || m_config.windowFunction != config.windowFunction
                              || m_config.channelMode != config.channelMode
                              || m_config.timeColumns != config.timeColumns;

    m_config = config;
    m_resizeTimer.stop();

    if(refreshTrack()) {
        return;
    }

    if(analysisChanged || m_request == 0) {
        generate();
    }
    else {
        rebuildImage();
        update();
    }
}

bool SpectrogramSeekBar::cancelSeeking()
{
    const bool wasSeeking = isSeeking();
    stopSeeking();
    return wasSeeking;
}

bool SpectrogramSeekBar::isSeeking() const
{
    return m_previewPosition.has_value();
}

void SpectrogramSeekBar::stopSeeking()
{
    m_previewPosition.reset();
    hideSeekTip();
    update();
}

bool SpectrogramSeekBar::event(QEvent* event)
{
    if(isSeeking()
       && (event->type() == QEvent::UngrabMouse || event->type() == QEvent::WindowDeactivate
           || event->type() == QEvent::Hide)) {
        stopSeeking();
    }

    return QWidget::event(event);
}

void SpectrogramSeekBar::paintEvent(QPaintEvent* /*event*/)
{
    if(!m_image.isNull() && !qFuzzyCompare(m_imageDpr, devicePixelRatioF()) && !m_resizeTimer.isActive()) {
        scheduleImageResize();
    }

    QPainter painter{this};
    painter.fillRect(rect(), palette().window());

    const QRect plot = plotRect();
    painter.fillRect(plot, m_config.colours.front());

    if(!m_image.isNull()) {
        painter.drawImage(plot, m_image);
    }

    if(m_track.duration() > 0) {
        const double ratio
            = displayingCurrentTrack()
                ? std::clamp(static_cast<double>(m_position) / static_cast<double>(m_track.duration()), 0.0, 1.0)
                : 0.0;
        const double playedWidth = ratio * plot.width();

        if(m_config.playedOverlayColour.isValid()) {
            painter.fillRect(QRectF{static_cast<double>(plot.left()), static_cast<double>(plot.top()), playedWidth,
                                    static_cast<double>(plot.height())},
                             m_config.playedOverlayColour);
        }

        if(m_config.unplayedOverlayColour.isValid()) {
            painter.fillRect(QRectF{plot.left() + playedWidth, static_cast<double>(plot.top()),
                                    plot.width() - playedWidth, static_cast<double>(plot.height())},
                             m_config.unplayedOverlayColour);
        }
    }

    if(plot != rect()) {
        drawAxes(painter, plot);
    }

    if(displayingCurrentTrack() && m_track.duration() > 0
       && m_playerController->playState() != Player::PlayState::Stopped) {
        painter.save();
        painter.setClipRect(plot);

        const auto drawCursor = [&](uint64_t position, const QColor& colour) {
            const double ratio
                = std::clamp(static_cast<double>(position) / static_cast<double>(m_track.duration()), 0.0, 1.0);
            const double x = plot.left() + (ratio * std::max(0, plot.width() - 1));

            QPen pen{colour, static_cast<double>(m_config.cursorWidth), Qt::SolidLine, Qt::FlatCap};
            pen.setCosmetic(true);
            painter.setPen(pen);

            painter.drawLine(QPointF{x, static_cast<double>(plot.top())},
                             QPointF{x, static_cast<double>(plot.bottom())});
        };

        const QColor highlight = palette().color(QPalette::Active, QPalette::Highlight);
        if(m_config.showCursor) {
            drawCursor(m_position, m_config.cursorColour.isValid() ? m_config.cursorColour : highlight);
        }

        if(m_previewPosition) {
            drawCursor(*m_previewPosition,
                       m_config.seekingCursorColour.isValid() ? m_config.seekingCursorColour : highlight.darker(150));
        }

        painter.restore();
    }
}

void SpectrogramSeekBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    scheduleImageResize();
}

void SpectrogramSeekBar::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_resizeTimer.timerId()) {
        m_resizeTimer.stop();
        rebuildImage();
        update();
        return;
    }

    SpectrogramView::timerEvent(event);
}

void SpectrogramSeekBar::mousePressEvent(QMouseEvent* event)
{
    if((event->button() == Qt::RightButton && isSeeking())
       || (event->button() == Qt::LeftButton && event->buttons() != Qt::LeftButton)) {
        stopSeeking();
        event->accept();
        return;
    }

    if(canSeek() && event->button() == Qt::LeftButton && plotRect().contains(event->position().toPoint())) {
        hideSeekTip();
        m_pressPosition = event->position().toPoint();
        previewPosition(m_pressPosition);
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void SpectrogramSeekBar::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint position = event->position().toPoint();
    if(canSeek() && (isSeeking() && event->buttons().testFlag(Qt::LeftButton))) {
        previewPosition(position);
        if(m_seekTip || std::abs(m_pressPosition.x() - position.x()) > ToolTipDelay) {
            showSeekTip(position);
        }
        event->accept();
        return;
    }

    hideSeekTip();
    QWidget::mouseMoveEvent(event);
}

void SpectrogramSeekBar::mouseReleaseEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton && isSeeking()) {
        const uint64_t position = positionAt(static_cast<int>(event->position().x()));
        stopSeeking();
        if(canSeek()) {
            m_playerController->seek(position);
        }
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void SpectrogramSeekBar::wheelEvent(QWheelEvent* event)
{
    if(canSeek() && event->angleDelta().y() != 0) {
        seekStep(event->angleDelta().y() > 0);
        event->accept();
        return;
    }

    event->ignore();
}

void SpectrogramSeekBar::keyPressEvent(QKeyEvent* event)
{
    if(canSeek()) {
        switch(event->key()) {
            case Qt::Key_Right:
            case Qt::Key_Up:
                seekStep(true);
                event->accept();
                return;
            case Qt::Key_Left:
            case Qt::Key_Down:
                seekStep(false);
                event->accept();
                return;
            case Qt::Key_Escape:
                stopSeeking();
                event->accept();
                return;
            default:
                break;
        }
    }

    QWidget::keyPressEvent(event);
}

void SpectrogramSeekBar::leaveEvent(QEvent* event)
{
    if(!isSeeking()) {
        hideSeekTip();
    }

    QWidget::leaveEvent(event);
}

void SpectrogramSeekBar::hideEvent(QHideEvent* event)
{
    stopSeeking();

    QWidget::hideEvent(event);
}

bool SpectrogramSeekBar::refreshTrack()
{
    return setTrack(preferredTrack());
}

bool SpectrogramSeekBar::setTrack(const Track& track)
{
    const bool unchanged = m_track.isValid() && track.isValid() && m_track.sameIdentityAs(track)
                        && m_track.uniqueFilepath() == track.uniqueFilepath() && m_track.offset() == track.offset()
                        && m_track.duration() == track.duration() && m_track.sampleRate() == track.sampleRate()
                        && m_track.channels() == track.channels();

    m_track    = track;
    m_position = displayingCurrentTrack() ? m_playerController->currentPosition() : 0;

    if(unchanged) {
        if(!canSeek()) {
            stopSeeking();
        }
        update();
        return false;
    }

    stopSeeking();
    generate();
    return true;
}

Track SpectrogramSeekBar::preferredTrack() const
{
    const Track playingTrack  = m_playerController->currentTrack();
    const Track selectedTrack = m_trackSelection->displayTrack();
    const auto preference     = static_cast<TrackDisplayPreference>(m_config.trackPreference);
    switch(preferredTrackSource(preference, m_playerController->playState(), m_playerController->playbackStarted())) {
        case PreferredTrackSource::Playing:
            return playingTrack.isValid() ? playingTrack : selectedTrack;
        case PreferredTrackSource::Selected:
            return selectedTrack.isValid() ? selectedTrack : playingTrack;
        case PreferredTrackSource::None:
            return {};
    }
    return {};
}

bool SpectrogramSeekBar::displayingCurrentTrack() const
{
    const auto currentTrack = m_playerController->currentTrack();
    return m_track.isValid() && currentTrack.isValid() && m_track.sameIdentityAs(currentTrack);
}

void SpectrogramSeekBar::generate()
{
    m_resizeTimer.stop();
    m_request = m_generator.cancel();
    m_columns.clear();
    m_image            = {};
    m_channels         = 0;
    m_completedColumns = 0;

    update();

    if(!m_track.isValid()) {
        return;
    }

    if(!m_generatorThread.isRunning()) {
        m_generatorThread.start();
    }

    const bool mono        = m_config.channelMode == ChannelMode::Mono;
    const int firstChannel = m_config.channelMode == ChannelMode::BackOnly ? 1 : 0;
    const int channels     = m_config.channelMode == ChannelMode::Unchanged ? 2 : 1;

    QMetaObject::invokeMethod(
        &m_generator, [this, request = m_request, track = m_track, config = m_config, mono, firstChannel, channels]() {
            m_generator.generate(request, track, config.fftSize, config.timeColumns,
                                 static_cast<VisualisationSession::SpectrumWindowFunction>(config.windowFunction), mono,
                                 firstChannel, channels);
        });
}

void SpectrogramSeekBar::rebuildImage(int firstColumn)
{
    if(m_columns.empty()) {
        return;
    }

    const bool resizing   = m_resizeTimer.isActive() && !m_image.isNull();
    const qreal dpr       = resizing ? m_imageDpr : devicePixelRatioF();
    const int imageHeight = resizing ? m_image.height() : targetImageHeight();
    const QSize size{static_cast<int>(m_columns.size()), imageHeight};

    if(m_image.size() != size || !qFuzzyCompare(m_imageDpr, dpr) || firstColumn == 0) {
        m_image = QImage{size, QImage::Format_RGB32};
        m_image.fill(m_config.colours.front());
        firstColumn = 0;
    }
    m_imageDpr = dpr;

    const int spacing   = m_channels == 2 && imageHeight >= 2
                            ? std::min(static_cast<int>(std::lround(m_config.channelSpacing * dpr)), imageHeight - 2)
                            : 0;
    const int topHeight = m_channels == 2 ? (imageHeight - spacing) / 2 : imageHeight;
    const int bottomTop = topHeight + spacing;

    std::vector levels(static_cast<size_t>(imageHeight), -1.0F);
    std::array<QRgb, 256> colours;

    for(size_t i{0}; i < colours.size(); ++i) {
        colours[i] = colourForLevel(static_cast<double>(i) / 255.0, m_config.colours).rgb();
    }

    for(int column{firstColumn}; column < m_completedColumns; ++column) {
        std::ranges::fill(levels, -1.0F);
        const auto& spectra = m_columns[column];
        fillSpectrumSection(levels, 0, topHeight, spectra[0], m_config);

        if(m_channels == 2) {
            fillSpectrumSection(levels, bottomTop, imageHeight - bottomTop, spectra[1], m_config);
        }

        for(int row{0}; row < imageHeight; ++row) {
            const float level = levels[row];
            auto* pixels      = reinterpret_cast<QRgb*>(m_image.scanLine(row));
            pixels[column]    = level < 0.0F ? m_config.colours.front().rgb()
                                             : colours[std::clamp(std::lround(level * 255.0F), 0L, 255L)];
        }
    }
}

void SpectrogramSeekBar::scheduleImageResize()
{
    if(m_image.isNull()
       || (m_image.height() == targetImageHeight() && qFuzzyCompare(m_imageDpr, devicePixelRatioF()))) {
        m_resizeTimer.stop();
        return;
    }

    m_resizeTimer.start(150, this);
}

int SpectrogramSeekBar::targetImageHeight() const
{
    const size_t columns    = std::max(size_t{1}, m_columns.size());
    const int maximumHeight = static_cast<int>(MaxImageBytes / (columns * sizeof(QRgb)));
    const qreal height      = std::ceil(plotRect().height() * devicePixelRatioF());
    return static_cast<int>(std::clamp(height, 1.0, static_cast<qreal>(maximumHeight)));
}

void SpectrogramSeekBar::drawAxes(QPainter& painter, const QRect& plot)
{
    if(plot.isEmpty()) {
        return;
    }

    painter.setPen(palette().windowText().color());
    if(m_config.showTopLabels || m_config.showBottomLabels) {
        drawTimeAxes(painter, plot);
    }
    if(m_config.showLeftLabels || m_config.showRightLabels) {
        drawFrequencyAxes(painter, plot);
    }
}

void SpectrogramSeekBar::drawTimeAxes(QPainter& painter, const QRect& plot)
{
    const auto metrics    = fontMetrics();
    const int labelHeight = metrics.height();

    static constexpr std::array timeSteps{1,   2,    5,    10,   20,   30,    60,    120,  300,
                                          600, 1200, 1800, 3600, 7200, 18000, 36000, 86400};

    const auto duration = m_track.duration();

    const auto timeText = [duration](uint64_t position) {
        return duration >= 1000 ? Utils::msToString(position)
                                : Utils::msToString(std::chrono::milliseconds{position}, true);
    };

    const int endLabelWidth      = metrics.horizontalAdvance(timeText(duration));
    const int timeLabelWidth     = std::max(metrics.horizontalAdvance(timeText(0)), endLabelWidth);
    const double durationSeconds = static_cast<double>(duration) / 1000.0;
    const int timeStep
        = selectAxisStep(timeSteps, (plot.width() - 1) / std::max(0.001, durationSeconds), timeLabelWidth * 1.5);

    const bool showEndLabel = duration > 0 && endLabelWidth <= plot.width();
    const int endLabelLeft  = plot.right() - endLabelWidth + 1;
    int lastLabelRight      = plot.left() - 8;

    const auto drawTimeTick = [&](uint64_t position) {
        const int x = plot.left()
                    + static_cast<int>((plot.width() - 1) * static_cast<double>(position)
                                       / std::max(1.0, static_cast<double>(duration)));

        const QString text  = timeText(position);
        const int textWidth = metrics.horizontalAdvance(text);
        if(textWidth > plot.width()) {
            return;
        }

        const int labelX
            = std::clamp(x - (textWidth / 2), plot.left(), std::max(plot.left(), plot.right() - textWidth + 1));
        const bool endLabel = showEndLabel && position == duration;
        if(!endLabel && (labelX < lastLabelRight + 8 || (showEndLabel && labelX + textWidth + 8 > endLabelLeft))) {
            return;
        }

        for(const bool top : {true, false}) {
            if(!(top ? m_config.showTopLabels : m_config.showBottomLabels)) {
                continue;
            }
            const int edge      = top ? plot.top() - 1 : plot.bottom() + 1;
            const int direction = top ? -1 : 1;
            const int labelY    = top ? plot.top() - labelHeight - 6 : plot.bottom() + 5;

            painter.drawLine(x, edge, x, edge + (direction * 3));
            painter.drawText(QRect{labelX, labelY, textWidth, labelHeight}, Qt::AlignCenter, text);
        }

        lastLabelRight = labelX + textWidth;
    };

    drawTimeTick(0);

    if(timeStep > 0) {
        const auto stepMs = static_cast<uint64_t>(timeStep) * 1000;
        for(uint64_t position{stepMs}; position < duration; position += stepMs) {
            drawTimeTick(position);
        }
    }

    if(showEndLabel) {
        drawTimeTick(duration);
    }
}

void SpectrogramSeekBar::drawFrequencyAxes(QPainter& painter, const QRect& plot)
{
    const int labelHeight = fontMetrics().height();

    const int sampleRate = m_completedColumns > 0 ? m_columns.front()[0].sampleRate : m_track.sampleRate();
    if(sampleRate <= 0) {
        return;
    }

    const double maximumFrequency = static_cast<double>(sampleRate) / 2.0;
    const double minimumFrequency
        = m_config.logFrequency
            ? std::min(maximumFrequency, std::max(25.0, static_cast<double>(sampleRate) / m_config.fftSize))
            : 0.0;
    const int spacing       = m_channels == 2 ? std::min(m_config.channelSpacing, std::max(0, plot.height() - 2)) : 0;
    const int channelHeight = m_channels == 2 ? (plot.height() - spacing) / 2 : plot.height();

    if(channelHeight <= 0) {
        return;
    }

    static constexpr std::array frequencySteps{10,   20,    50,    100,   200,    500,    1000,  2000,
                                               5000, 10000, 20000, 50000, 100000, 200000, 500000};

    for(int channel{0}; channel < std::max(1, m_channels); ++channel) {
        const int top    = plot.top() + (channel * (channelHeight + spacing));
        const int bottom = top + channelHeight - 1;

        const auto frequencyY = [&](double frequency) {
            const double ratio
                = m_config.logFrequency && maximumFrequency > minimumFrequency
                    ? std::log(frequency / minimumFrequency) / std::log(maximumFrequency / minimumFrequency)
                    : frequency / maximumFrequency;
            return bottom - static_cast<int>(std::lround(ratio * (channelHeight - 1)));
        };

        const auto drawFrequencyTick = [&](double frequency) {
            const int y = frequencyY(frequency);
            const double frequencyKhz
                = frequency == maximumFrequency ? std::floor(frequency / 1000.0) : frequency / 1000.0;
            const QString text = frequency >= 1000.0 ? u"%1 kHz"_s.arg(frequencyKhz, 0, 'g', 3)
                                                     : u"%1 Hz"_s.arg(std::lround(frequency));

            for(const bool left : {true, false}) {
                if(!(left ? m_config.showLeftLabels : m_config.showRightLabels)) {
                    continue;
                }

                const int edge      = left ? plot.left() - 1 : plot.right() + 1;
                const int direction = left ? -1 : 1;
                painter.drawLine(edge, y, edge + (direction * 3), y);
                const int labelY
                    = std::clamp(y - (labelHeight / 2), top, std::max(top, top + channelHeight - labelHeight));
                const int labelX     = left ? 0 : plot.right() + 6;
                const int labelWidth = left ? plot.left() - 6 : width() - labelX;
                const auto alignment = (left ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter;
                painter.drawText(QRect{labelX, labelY, labelWidth, labelHeight}, static_cast<int>(alignment), text);
            }
        };

        drawFrequencyTick(minimumFrequency);

        if(channelHeight < labelHeight * 2) {
            continue;
        }

        drawFrequencyTick(maximumFrequency);

        int lastY{bottom};
        const auto drawInteriorTick = [&](double frequency) {
            const int y              = frequencyY(frequency);
            const int minimumSpacing = m_config.logFrequency ? labelHeight * 3 : labelHeight * 2;
            if(lastY - y >= minimumSpacing && y - top >= labelHeight * 1.5) {
                drawFrequencyTick(frequency);
                lastY = y;
            }
        };

        if(m_config.logFrequency) {
            for(int64_t base{1}; static_cast<double>(base) < maximumFrequency; base *= 10) {
                for(const int factor : {1, 2, 5}) {
                    const auto frequency = static_cast<double>(base * factor);
                    if(frequency > minimumFrequency && frequency < maximumFrequency) {
                        drawInteriorTick(frequency);
                    }
                }
            }
        }
        else {
            const int step = selectAxisStep(frequencySteps, (channelHeight - 1) / maximumFrequency, labelHeight * 3.0);
            if(step > 0) {
                for(int64_t frequency{step}; static_cast<double>(frequency) < maximumFrequency; frequency += step) {
                    drawInteriorTick(static_cast<double>(frequency));
                }
            }
        }
    }
}

void SpectrogramSeekBar::previewPosition(const QPoint& position)
{
    m_mousePosition   = position;
    m_previewPosition = positionAt(position.x());
    update();
}

void SpectrogramSeekBar::showSeekTip(const QPoint& position)
{
    if(!m_seekTip) {
        m_seekTip = new ToolTip(this);
        m_seekTip->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
        m_seekTip->setAttribute(Qt::WA_ShowWithoutActivating);
        m_seekTip->setPalette(palette());
    }

    const uint64_t seekPosition = m_previewPosition.value_or(positionAt(position.x()));
    QString subtext;
    if(isSeeking()) {
        const uint64_t delta = std::max(m_position, seekPosition) - std::min(m_position, seekPosition);
        subtext              = (seekPosition >= m_position ? u"+"_s : u"-"_s) + Utils::msToString(delta);
    }
    m_seekTip->setContent(Utils::msToString(seekPosition), subtext);

    QPoint tipPosition{position};
    static constexpr int offset = 8;

    if(position.x() > width() / 2) {
        tipPosition.rx() -= m_seekTip->width() + m_config.cursorWidth + offset;
    }
    else {
        tipPosition.rx() += m_config.cursorWidth + offset;
    }
    tipPosition.ry() += m_seekTip->height() / 2;
    tipPosition.setY(std::clamp(tipPosition.y(), m_seekTip->height() / 2, std::max(height(), m_seekTip->height() / 2)));

    m_seekTip->setPosition(mapToGlobal(tipPosition));
    m_seekTip->show();
}

void SpectrogramSeekBar::hideSeekTip()
{
    if(m_seekTip) {
        m_seekTip->hide();
        m_seekTip->deleteLater();
    }
}

void SpectrogramSeekBar::seekStep(bool forward)
{
    const auto step = static_cast<uint64_t>(m_settings->value<Settings::Gui::SeekStepSmall>());
    if(forward) {
        m_playerController->seekForward(step);
    }
    else {
        m_playerController->seekBackward(step);
    }
}

QRect SpectrogramSeekBar::plotRect() const
{
    if(width() < 200 || height() < 80) {
        return rect();
    }

    const int frequencyMargin = fontMetrics().horizontalAdvance(u"999.9 kHz"_s) + 8;
    const int timeMargin      = fontMetrics().height() + 6;
    const int left            = m_config.showLeftLabels ? frequencyMargin : 0;
    const int right           = m_config.showRightLabels ? frequencyMargin : 0;
    const int top             = m_config.showTopLabels ? timeMargin : 0;
    const int bottom          = m_config.showBottomLabels ? timeMargin : 0;
    return rect().adjusted(left, top, -right, -bottom);
}

uint64_t SpectrogramSeekBar::positionAt(int x) const
{
    const QRect plot   = plotRect();
    const double ratio = std::clamp(static_cast<double>(x - plot.left()) / std::max(1, plot.width() - 1), 0.0, 1.0);
    return static_cast<uint64_t>(ratio * static_cast<double>(m_track.duration()));
}

bool SpectrogramSeekBar::canSeek() const
{
    return displayingCurrentTrack() && m_track.duration() > 0 && m_playerController->currentTrackSeekable()
        && m_playerController->playState() != Player::PlayState::Stopped;
}
} // namespace Fooyin::Spectrogram

#include "moc_spectrogramseekbar.cpp"
