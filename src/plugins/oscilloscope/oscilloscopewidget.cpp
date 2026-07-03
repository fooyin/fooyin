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

#include "oscilloscopewidget.h"

#include "oscilloscopecolours.h"

#include <core/engine/enginecontroller.h>
#include <core/engine/visualisationservice.h>
#include <core/player/playercontroller.h>
#include <core/player/playerdefs.h>
#include <gui/configdialog.h>
#include <gui/framerate.h>
#include <utils/settings/settingsmanager.h>

#include <QActionGroup>
#include <QBasicTimer>
#include <QContextMenuEvent>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QMenu>
#include <QPainter>
#include <QPolygonF>

using namespace Qt::StringLiterals;

constexpr auto CurveDurationKey = u"Oscilloscope/CurveDurationMs";
constexpr auto ZoomKey          = u"Oscilloscope/ZoomPercent";
constexpr auto UpdateFpsKey     = u"Oscilloscope/UpdateFps";
constexpr auto DownmixModeKey   = u"Oscilloscope/DownmixMode";
constexpr auto ShowZeroLineKey  = u"Oscilloscope/ShowZeroLine";
constexpr auto ColoursKey       = u"Oscilloscope/Colours";

constexpr auto StereoLaneGapPx  = 4.0F;
constexpr auto MinLanePaddingPx = 2.0F;
constexpr auto LanePaddingRatio = 0.05F;
constexpr auto ClockSettleMs    = 150;
constexpr auto ClockResetMs     = 300.0;

#include "oscilloscopeconfigwidget.h"

namespace Fooyin::Oscilloscope {
OscilloscopeWidget::OscilloscopeWidget(EngineController* engine, PlayerController* playerController,
                                       SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_settings{settings}
    , m_session{engine->visualisationService()->createSession()}
    , m_window{}
    , m_active{false}
    , m_paused{false}
    , m_clockSettling{false}
    , m_presentationTimeMs{0.0}
{
    m_session->requestBacklog(static_cast<uint64_t>(MaxCurveDurationMs));

    handlePlayStateChanged(playerController->playState());

    QObject::connect(playerController, &PlayerController::playStateChanged, this,
                     &OscilloscopeWidget::handlePlayStateChanged);
    QObject::connect(playerController, &PlayerController::currentTrackChanged, this, [this]() { clearWindow(false); });
    QObject::connect(playerController, &PlayerController::positionMoved, this, [this]() { clearWindow(false); });

    m_config = defaultConfig();
    applyConfig(m_config);
}

QString OscilloscopeWidget::name() const
{
    return tr("Oscilloscope");
}

QString OscilloscopeWidget::layoutName() const
{
    return u"Oscilloscope"_s;
}

void OscilloscopeWidget::saveLayoutData(QJsonObject& layout)
{
    saveConfigToLayout(m_config, layout);
}

void OscilloscopeWidget::loadLayoutData(const QJsonObject& layout)
{
    applyConfig(configFromLayout(layout));
}

OscilloscopeWidget::ConfigData OscilloscopeWidget::factoryConfig() const
{
    return {};
}

OscilloscopeWidget::ConfigData OscilloscopeWidget::defaultConfig() const
{
    auto config{factoryConfig()};

    config.curveDurationMs = m_settings->fileValue(CurveDurationKey, config.curveDurationMs).toInt();
    config.zoomPercent     = m_settings->fileValue(ZoomKey, config.zoomPercent).toInt();
    config.updateFps       = m_settings->fileValue(UpdateFpsKey, config.updateFps).toInt();
    config.downmixMode
        = static_cast<DownmixMode>(m_settings->fileValue(DownmixModeKey, static_cast<int>(config.downmixMode)).toInt());
    config.showZeroLine = m_settings->fileValue(ShowZeroLineKey, config.showZeroLine).toBool();
    config.colours      = m_settings->fileValue(ColoursKey, config.colours);

    return config;
}

const OscilloscopeWidget::ConfigData& OscilloscopeWidget::currentConfig() const
{
    return m_config;
}

void OscilloscopeWidget::saveDefaults(const ConfigData& config) const
{
    auto validated{config};

    validated.curveDurationMs = std::clamp(validated.curveDurationMs, MinCurveDurationMs, MaxCurveDurationMs);
    validated.zoomPercent     = std::clamp(validated.zoomPercent, MinZoomPercent, MaxZoomPercent);
    validated.updateFps       = Gui::FrameRate::nearestPresetFps(validated.updateFps);

    if(!validated.colours.canConvert<Colours>()
       || (validated.colours.isValid() && validated.colours.value<Colours>().isEmpty())) {
        validated.colours = QVariant{};
    }

    m_settings->fileSet(CurveDurationKey, validated.curveDurationMs);
    m_settings->fileSet(ZoomKey, validated.zoomPercent);
    m_settings->fileSet(UpdateFpsKey, validated.updateFps);
    m_settings->fileSet(DownmixModeKey, static_cast<int>(validated.downmixMode));
    m_settings->fileSet(ShowZeroLineKey, validated.showZeroLine);
    m_settings->fileSet(ColoursKey, validated.colours);
}

void OscilloscopeWidget::clearSavedDefaults() const
{
    m_settings->fileRemove(CurveDurationKey);
    m_settings->fileRemove(ZoomKey);
    m_settings->fileRemove(UpdateFpsKey);
    m_settings->fileRemove(DownmixModeKey);
    m_settings->fileRemove(ShowZeroLineKey);
    m_settings->fileRemove(ColoursKey);
}

void OscilloscopeWidget::applyConfig(const ConfigData& config)
{
    auto validated{config};

    validated.curveDurationMs = std::clamp(validated.curveDurationMs, MinCurveDurationMs, MaxCurveDurationMs);
    validated.zoomPercent     = std::clamp(validated.zoomPercent, MinZoomPercent, MaxZoomPercent);
    validated.updateFps       = Gui::FrameRate::nearestPresetFps(validated.updateFps);

    if(!validated.colours.canConvert<Colours>()
       || (validated.colours.isValid() && validated.colours.value<Colours>().isEmpty())) {
        validated.colours = QVariant{};
    }

    m_config = validated;
    updateSessionConfig();
    update();
}

QSize OscilloscopeWidget::minimumSizeHint() const
{
    return {24, 18};
}

void OscilloscopeWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing, false);
    paint(painter, rect(), palette());
}

void OscilloscopeWidget::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_updateTimer.timerId()) {
        tick();
    }
    FyWidget::timerEvent(event);
}

void OscilloscopeWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* zeroLineAction = menu->addAction(tr("Show zero line"));
    zeroLineAction->setCheckable(true);
    zeroLineAction->setChecked(m_config.showZeroLine);
    QObject::connect(zeroLineAction, &QAction::triggered, this, [this](bool checked) {
        auto config{m_config};
        config.showZeroLine = checked;
        applyConfig(config);
    });

    auto* downmixMenu  = menu->addMenu(tr("Downmix mode"));
    auto* downmixGroup = new QActionGroup(downmixMenu);
    downmixGroup->setExclusive(true);

    const auto addDownmixAction = [this, downmixMenu, downmixGroup](const QString& text, DownmixMode mode) {
        auto* action = downmixMenu->addAction(text);
        action->setActionGroup(downmixGroup);
        action->setCheckable(true);
        action->setChecked(m_config.downmixMode == mode);
        QObject::connect(action, &QAction::triggered, this, [this, mode]() {
            auto config{m_config};
            config.downmixMode = mode;
            applyConfig(config);
        });
    };

    addDownmixAction(tr("Stereo"), DownmixMode::Stereo);
    addDownmixAction(tr("Mono"), DownmixMode::Mono);

    auto* durationMenu  = menu->addMenu(tr("Curve duration"));
    auto* durationGroup = new QActionGroup(durationMenu);
    durationGroup->setExclusive(true);

    for(const int durationMs : CurveDurationPresets) {
        auto* action = durationMenu->addAction(tr("%1 ms").arg(durationMs));
        action->setActionGroup(durationGroup);
        action->setCheckable(true);
        action->setChecked(m_config.curveDurationMs == durationMs);
        QObject::connect(action, &QAction::triggered, this, [this, durationMs]() {
            auto config{m_config};
            config.curveDurationMs = durationMs;
            applyConfig(config);
        });
    }

    auto* zoomMenu  = menu->addMenu(tr("Zoom"));
    auto* zoomGroup = new QActionGroup(zoomMenu);
    zoomGroup->setExclusive(true);

    for(const int zoomPercent : ZoomPresets) {
        auto* action = zoomMenu->addAction(tr("%1%").arg(zoomPercent));
        action->setActionGroup(zoomGroup);
        action->setCheckable(true);
        action->setChecked(m_config.zoomPercent == zoomPercent);
        QObject::connect(action, &QAction::triggered, this, [this, zoomPercent]() {
            auto config{m_config};
            config.zoomPercent = zoomPercent;
            applyConfig(config);
        });
    }

    menu->addSeparator();
    addConfigureAction(menu, false);
    menu->popup(event->globalPos());
}

void OscilloscopeWidget::openConfigDialog()
{
    showConfigDialog(new OscilloscopeConfigDialog(this, this));
}

void OscilloscopeWidget::handlePlayStateChanged(Player::PlayState state)
{
    m_paused = state == Player::PlayState::Paused;
    m_active = state != Player::PlayState::Stopped;

    if(state == Player::PlayState::Stopped) {
        clearWindow(true);
        return;
    }

    if(m_paused) {
        m_updateTimer.stop();
        m_presentationClock.invalidate();
        return;
    }

    if(m_active && !m_paused) {
        settlePresentationClock();
        startUpdateTimer();
    }
}

void OscilloscopeWidget::clearWindow(bool stopTimer)
{
    m_window = {};
    m_presentationClock.invalidate();
    m_presentationTimeMs = 0.0;

    if(stopTimer) {
        m_updateTimer.stop();
        m_clockSettling = false;
    }
    else if(m_active && !m_paused) {
        settlePresentationClock();
        startUpdateTimer();
    }

    update();
}

int OscilloscopeWidget::displayedLanes() const
{
    return m_config.downmixMode == DownmixMode::Mono ? 1 : 2;
}

Colours OscilloscopeWidget::colours() const
{
    return m_config.colours.isValid() && m_config.colours.canConvert<Colours>() ? m_config.colours.value<Colours>()
                                                                                : Colours{};
}

void OscilloscopeWidget::updateSessionConfig()
{
    m_session->requestBacklog(static_cast<uint64_t>(std::max(m_config.curveDurationMs, MaxCurveDurationMs)));
    m_session->setChannelMode(m_config.downmixMode == DownmixMode::Mono ? VisualisationSession::ChannelMode::Mono
                                                                        : VisualisationSession::ChannelMode::Default);

    if(m_active && !m_paused) {
        startUpdateTimer();
    }
}

void OscilloscopeWidget::startUpdateTimer()
{
    m_updateTimer.start(Gui::FrameRate::intervalMsForFps(m_config.updateFps), Qt::PreciseTimer, this);
}

void OscilloscopeWidget::settlePresentationClock()
{
    m_presentationClock.invalidate();
    m_clockSettling = true;
    m_clockSettleTimer.restart();
}

void OscilloscopeWidget::tick()
{
    if(m_paused) {
        m_updateTimer.stop();
        return;
    }

    if(m_clockSettling) {
        if(m_clockSettleTimer.elapsed() < ClockSettleMs) {
            return;
        }
        m_clockSettling = false;
    }

    const uint64_t rawTimeMs = m_session->currentTimeMs();
    if(rawTimeMs == 0) {
        return;
    }

    if(!m_presentationClock.isValid()) {
        m_presentationTimeMs = static_cast<double>(rawTimeMs);
        m_presentationClock.start();
    }
    else {
        const double elapsedMs = static_cast<double>(m_presentationClock.nsecsElapsed()) / 1'000'000.0;
        m_presentationClock.restart();

        const double predictedTimeMs = m_presentationTimeMs + std::max(0.0, elapsedMs);
        const double clockErrorMs    = static_cast<double>(rawTimeMs) - predictedTimeMs;
        m_presentationTimeMs = std::abs(clockErrorMs) > ClockResetMs ? static_cast<double>(rawTimeMs) : predictedTimeMs;
    }

    const auto visibleDurationMs  = static_cast<uint64_t>(m_config.curveDurationMs);
    const auto presentationTimeMs = static_cast<uint64_t>(std::max(0.0, m_presentationTimeMs));

    VisualisationSession::PcmWindow window;
    if(m_session->getPcmWindowEndingAt(window, presentationTimeMs, visibleDurationMs)) {
        const int requiredFrames = window.format.framesForDuration(visibleDurationMs);
        if(window.frameCount < requiredFrames) {
            return;
        }

        m_window = std::move(window);
        update();
    }
}

void OscilloscopeWidget::paint(QPainter& painter, const QRect& rect, const QPalette& palette) const
{
    const Colours customColours{colours()};
    const QColor background = customColours.colour(Colours::Type::Background, palette);
    const QColor waveform   = customColours.colour(Colours::Type::Waveform, palette);
    const QColor zeroLine   = customColours.colour(Colours::Type::ZeroLine, palette);

    painter.fillRect(rect, Qt::transparent);
    if(background.alpha() > 0) {
        painter.fillRect(rect, background);
    }

    const int laneCount = displayedLanes();
    if(rect.width() <= 0 || rect.height() <= 0 || laneCount <= 0) {
        return;
    }

    const float gap        = laneCount == 2 ? StereoLaneGapPx : 0.0F;
    const float laneHeight = std::max(1.0F, (static_cast<float>(rect.height()) - gap) / static_cast<float>(laneCount));

    QPen zeroPen{zeroLine};
    zeroPen.setCosmetic(true);

    for(int lane{0}; lane < laneCount; ++lane) {
        const float top     = static_cast<float>(rect.top()) + (static_cast<float>(lane) * (laneHeight + gap));
        const float centerY = top + (laneHeight * 0.5F);
        if(m_config.showZeroLine) {
            painter.setPen(zeroPen);
            painter.drawLine(
                QLineF{static_cast<float>(rect.left()), centerY, static_cast<float>(rect.right()), centerY});
        }

        if(!m_window.isValid() || m_window.format.channelCount() <= 0 || m_window.frameCount <= 0) {
            continue;
        }

        const float lanePadding = std::max(MinLanePaddingPx, laneHeight * LanePaddingRatio);
        const float amplitude   = std::max(1.0F, ((laneHeight - (lanePadding * 2.0F)) * 0.5F));
        const auto samples      = m_window.interleavedSamples();
        const int channels      = m_window.format.channelCount();
        const int frames        = m_window.frameCount;

        QPen waveformPen{waveform};
        waveformPen.setCosmetic(true);
        waveformPen.setCapStyle(Qt::RoundCap);
        painter.setPen(waveformPen);

        const int channelIndex
            = m_config.downmixMode == DownmixMode::Mono || channels == 1 ? 0 : std::min(lane, channels - 1);
        const float zoom    = static_cast<float>(m_config.zoomPercent) / 100.0F;
        const auto sampleAt = [&](size_t frameIndex) {
            return samples[(frameIndex * static_cast<size_t>(channels)) + static_cast<size_t>(channelIndex)] * zoom;
        };

        QPolygonF points;
        const int deviceWidth = std::max(1, qRound(static_cast<qreal>(rect.width()) * devicePixelRatioF()));
        const int pointCount  = std::min(frames, deviceWidth);
        points.reserve(pointCount);

        const float xScale
            = pointCount > 1 ? static_cast<float>(rect.width() - 1) / static_cast<float>(pointCount - 1) : 0.0F;
        for(int pointIndex{0}; pointIndex < pointCount; ++pointIndex) {
            float sampleValue{0.0F};
            if(pointCount == frames) {
                sampleValue = sampleAt(static_cast<size_t>(pointIndex));
            }
            else {
                const int beginFrame = static_cast<int>((static_cast<qint64>(pointIndex) * frames) / pointCount);
                const int endFrame   = static_cast<int>((static_cast<qint64>(pointIndex + 1) * frames) / pointCount);
                double sum{0.0};
                for(int frameIndex{beginFrame}; frameIndex < endFrame; ++frameIndex) {
                    sum += static_cast<double>(sampleAt(static_cast<size_t>(frameIndex)));
                }
                sampleValue = static_cast<float>(sum / static_cast<double>(endFrame - beginFrame));
            }

            const float drawX = static_cast<float>(rect.left()) + (static_cast<float>(pointIndex) * xScale);
            const float drawY = centerY - (sampleValue * amplitude);
            points.append(QPointF{drawX, drawY});
        }

        if(points.size() > 1) {
            painter.drawPolyline(points);
        }
        else if(!points.isEmpty()) {
            painter.drawPoint(points.constFirst());
        }
        painter.setPen(zeroPen);
    }
}

OscilloscopeWidget::ConfigData OscilloscopeWidget::configFromLayout(const QJsonObject& layout) const
{
    ConfigData config{defaultConfig()};

    if(layout.contains("CurveDurationMs"_L1)) {
        config.curveDurationMs = layout.value("CurveDurationMs"_L1).toInt();
    }
    if(layout.contains("ZoomPercent"_L1)) {
        config.zoomPercent = layout.value("ZoomPercent"_L1).toInt();
    }
    if(layout.contains("UpdateFps"_L1)) {
        config.updateFps = layout.value("UpdateFps"_L1).toInt();
    }
    if(layout.contains("DownmixMode"_L1)) {
        config.downmixMode = static_cast<DownmixMode>(layout.value("DownmixMode"_L1).toInt());
    }
    if(layout.contains("ShowZeroLine"_L1)) {
        config.showZeroLine = layout.value("ShowZeroLine"_L1).toBool();
    }

    if(layout.contains("UseCustomColours"_L1)) {
        if(layout.value("UseCustomColours"_L1).toBool()) {
            Colours colours;

            const auto setColour = [&layout, &colours](const QString& key, Colours::Type type) {
                if(!layout.contains(key)) {
                    return;
                }

                const QColor colour{layout.value(key).toString()};
                if(colour.isValid()) {
                    colours.setColour(type, colour);
                }
            };

            setColour(u"BackgroundColour"_s, Colours::Type::Background);
            setColour(u"WaveformColour"_s, Colours::Type::Waveform);
            setColour(u"ZeroLineColour"_s, Colours::Type::ZeroLine);

            if(!colours.isEmpty()) {
                config.colours = QVariant::fromValue(colours);
            }
        }
        else {
            config.colours = QVariant{};
        }
    }

    return config;
}

void OscilloscopeWidget::saveConfigToLayout(const ConfigData& config, QJsonObject& layout) const
{
    layout["CurveDurationMs"_L1] = config.curveDurationMs;
    layout["ZoomPercent"_L1]     = config.zoomPercent;
    layout["UpdateFps"_L1]       = config.updateFps;
    layout["DownmixMode"_L1]     = static_cast<int>(config.downmixMode);
    layout["ShowZeroLine"_L1]    = config.showZeroLine;

    const bool customColours      = config.colours.isValid() && config.colours.canConvert<Colours>()
                                 && !config.colours.value<Colours>().isEmpty();
    layout["UseCustomColours"_L1] = customColours;

    if(!customColours) {
        return;
    }

    const auto colours            = config.colours.value<Colours>();
    layout["BackgroundColour"_L1] = colours.colour(Colours::Type::Background, palette()).name(QColor::HexArgb);
    layout["WaveformColour"_L1]   = colours.colour(Colours::Type::Waveform, palette()).name(QColor::HexArgb);
    layout["ZeroLineColour"_L1]   = colours.colour(Colours::Type::ZeroLine, palette()).name(QColor::HexArgb);
}
} // namespace Fooyin::Oscilloscope

#include "moc_oscilloscopewidget.cpp"
