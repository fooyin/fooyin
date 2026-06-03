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

#include "wavebarwidget.h"

#include "wavebarcolours.h"
#include "wavebarconfigwidget.h"
#include "wavebarconstants.h"
#include "waveformbuilder.h"
#include "waveseekbar.h"

#include <core/player/playercontroller.h>
#include <gui/guisettings.h>
#include <gui/widgets/seekcontainer.h>
#include <utils/settings/settingsmanager.h>
#include <utils/signalthrottler.h>
#include <utils/stringutils.h>

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QDialog>
#include <QFile>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

// Settings
constexpr auto ShowLabelsKey         = u"WaveBar/ShowLabels";
constexpr auto ShowRemainingTimeKey  = u"WaveBar/ShowRemainingTime";
constexpr auto LegacyElapsedTotalKey = u"WaveBar/ElapsedTotal";
constexpr auto ShowCursorKey         = u"WaveBar/ShowCursor";
constexpr auto CursorWidthKey        = u"WaveBar/CursorWidth";
constexpr auto ModeKey               = u"WaveBar/Mode";
constexpr auto DownmixKey            = u"WaveBar/Downmix";
constexpr auto BarWidthKey           = u"WaveBar/BarWidth";
constexpr auto BarGapKey             = u"WaveBar/BarGap";
constexpr auto SupersampleFactorKey  = u"WaveBar/SupersampleFactor";
constexpr auto PeakDisplayModeKey    = u"WaveBar/PeakDisplayMode";
constexpr auto NormaliseToPeakKey    = u"WaveBar/NormaliseToPeak";
constexpr auto DecibelScaleKey       = u"WaveBar/DecibelScale";
constexpr auto MaxScaleKey           = u"WaveBar/MaxScale";
constexpr auto CentreGapKey          = u"WaveBar/CentreGap";
constexpr auto ChannelScaleKey       = u"WaveBar/ChannelScale";
constexpr auto ColoursKey            = u"WaveBar/Colours";

namespace Fooyin::WaveBar {
namespace {
int normaliseSupersampleFactor(int factor)
{
    return std::clamp(factor, 1, 8);
}
} // namespace

WaveBarWidget::WaveBarWidget(std::shared_ptr<AudioLoader> audioLoader, DbConnectionPoolPtr dbPool,
                             PlayerController* playerController, SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_playerController{playerController}
    , m_settings{settings}
    , m_container{new SeekContainer(m_playerController, this)}
    , m_seekbar{new WaveSeekBar(this)}
    , m_builder{std::make_unique<WaveformBuilder>(std::move(audioLoader), std::move(dbPool), settings, this)}
{
    setMinimumSize(100, 20);
    resize(100, 100);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_container);
    m_container->insertWidget(1, m_seekbar);

    m_seekbar->setPlayState(m_playerController->playState());
    m_seekbar->setSeekable(m_playerController->currentTrackSeekable());
    m_seekbar->setPosition(m_playerController->currentPosition());
    m_seekbar->setMouseFocusEnabled(m_settings->value<Settings::Gui::SeekBarMouseFocus>());

    m_config = defaultConfig();
    applyConfig(m_config);

    QObject::connect(m_builder.get(), &WaveformBuilder::generatingWaveform, this,
                     [this]() { m_seekbar->processData({}); });
    QObject::connect(m_builder.get(), &WaveformBuilder::waveformRescaled, m_seekbar, &WaveSeekBar::processData);

    QObject::connect(playerController, &PlayerController::positionChanged, m_seekbar, &WaveSeekBar::setPosition);
    QObject::connect(playerController, &PlayerController::playStateChanged, m_seekbar, &WaveSeekBar::setPlayState);
    QObject::connect(playerController, &PlayerController::currentTrackSeekableChanged, m_seekbar,
                     &WaveSeekBar::setSeekable);
    QObject::connect(m_seekbar, &WaveSeekBar::sliderMoved, playerController, &PlayerController::seek);
    QObject::connect(m_seekbar, &WaveSeekBar::seekForward, playerController,
                     [this]() { m_playerController->seekForward(m_settings->value<Settings::Gui::SeekStepSmall>()); });
    QObject::connect(m_seekbar, &WaveSeekBar::seekBackward, playerController,
                     [this]() { m_playerController->seekBackward(m_settings->value<Settings::Gui::SeekStepSmall>()); });

    QObject::connect(m_container, &SeekContainer::totalClicked, this, [this]() {
        auto config              = m_config;
        config.showRemainingTime = m_container->showRemainingTime();
        applyConfig(config);
    });

    auto* throttler = new SignalThrottler(this);
    throttler->setTimeout(250ms);
    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, throttler, &SignalThrottler::throttle);
    QObject::connect(throttler, &SignalThrottler::triggered, this,
                     [this]() { changeTrack(m_playerController->currentTrack()); });

    const auto updateColours = [this]() {
        if(m_config.colourOptions.isValid() && m_config.colourOptions.canConvert<Colours>()
           && !m_config.colourOptions.value<Colours>().isEmpty()) {
            return;
        }

        m_seekbar->setColours(Colours{});
    };
    m_settings->subscribe<Settings::Gui::Theme>(this, updateColours);
    m_settings->subscribe<Settings::Gui::Style>(this, updateColours);
    m_settings->subscribe<Settings::Gui::SeekBarMouseFocus>(m_seekbar, &WaveSeekBar::setMouseFocusEnabled);
}

QString WaveBarWidget::name() const
{
    return u"Waveform Seekbar"_s;
}

QString WaveBarWidget::layoutName() const
{
    return u"WaveBar"_s;
}

void WaveBarWidget::saveLayoutData(QJsonObject& layout)
{
    saveConfigToLayout(m_config, layout);
}

void WaveBarWidget::loadLayoutData(const QJsonObject& layout)
{
    applyConfig(configFromLayout(layout));
}

WaveBarWidget::ConfigData WaveBarWidget::defaultConfig() const
{
    auto config{factoryConfig()};

    config.showLabels = m_settings->fileValue(ShowLabelsKey, config.showLabels).toBool();
    config.showRemainingTime
        = m_settings
              ->fileValue(m_settings->fileContains(ShowRemainingTimeKey) ? ShowRemainingTimeKey : LegacyElapsedTotalKey,
                          config.showRemainingTime)
              .toBool();
    config.showCursor        = m_settings->fileValue(ShowCursorKey, config.showCursor).toBool();
    config.cursorWidth       = m_settings->fileValue(CursorWidthKey, config.cursorWidth).toInt();
    config.mode              = m_settings->fileValue(ModeKey, config.mode).toInt();
    config.downmix           = m_settings->fileValue(DownmixKey, config.downmix).toInt();
    config.barWidth          = m_settings->fileValue(BarWidthKey, config.barWidth).toInt();
    config.barGap            = m_settings->fileValue(BarGapKey, config.barGap).toInt();
    config.supersampleFactor = m_settings->fileValue(SupersampleFactorKey, config.supersampleFactor).toInt();
    config.peakDisplayMode   = m_settings->fileValue(PeakDisplayModeKey, config.peakDisplayMode).toInt();
    config.normaliseToPeak   = m_settings->fileValue(NormaliseToPeakKey, config.normaliseToPeak).toBool();
    config.decibelScale      = m_settings->fileValue(DecibelScaleKey, config.decibelScale).toBool();
    config.maxScale          = m_settings->fileValue(MaxScaleKey, config.maxScale).toDouble();
    config.centreGap         = m_settings->fileValue(CentreGapKey, config.centreGap).toInt();
    config.channelScale      = m_settings->fileValue(ChannelScaleKey, config.channelScale).toDouble();
    config.colourOptions     = m_settings->fileValue(ColoursKey, config.colourOptions);

    return config;
}

WaveBarWidget::ConfigData WaveBarWidget::factoryConfig() const
{
    return {
        .showLabels        = false,
        .showRemainingTime = false,
        .showCursor        = true,
        .cursorWidth       = 3,
        .mode              = static_cast<int>(Default),
        .downmix           = 0,
        .barWidth          = 1,
        .barGap            = 0,
        .supersampleFactor = 1,
        .peakDisplayMode   = static_cast<int>(PeakDisplayMode::Maximum),
        .normaliseToPeak   = false,
        .decibelScale      = false,
        .maxScale          = 1.0,
        .centreGap         = 0,
        .channelScale      = 0.9,
        .colourOptions     = QVariant{},
    };
}

const WaveBarWidget::ConfigData& WaveBarWidget::currentConfig() const
{
    return m_config;
}

int WaveBarWidget::globalNumSamples() const
{
    return m_settings->value<Settings::WaveBar::NumSamples>();
}

bool WaveBarWidget::setGlobalNumSamples(int samples) const
{
    return m_settings->set<Settings::WaveBar::NumSamples>(samples);
}

QString WaveBarWidget::cacheSizeText() const
{
    const QFile cacheFile{cachePath()};
    return tr("Disk cache usage") + u": %1"_s.arg(Utils::formatFileSize(cacheFile.size()));
}

void WaveBarWidget::requestClearCache()
{
    Q_EMIT clearCacheRequested();
}

void WaveBarWidget::saveDefaults(const ConfigData& config) const
{
    auto validated{config};

    validated.cursorWidth       = std::clamp(validated.cursorWidth, 1, 20);
    validated.barWidth          = std::clamp(validated.barWidth, 1, 50);
    validated.barGap            = std::clamp(validated.barGap, 0, 50);
    validated.supersampleFactor = normaliseSupersampleFactor(validated.supersampleFactor);
    validated.peakDisplayMode   = std::clamp(validated.peakDisplayMode, static_cast<int>(PeakDisplayMode::Maximum),
                                             static_cast<int>(PeakDisplayMode::SmoothedAverage));
    validated.maxScale          = std::clamp(validated.maxScale, 0.0, 2.0);
    validated.centreGap         = std::clamp(validated.centreGap, 0, 10);
    validated.channelScale      = std::clamp(validated.channelScale, 0.0, 1.0);
    validated.downmix
        = std::clamp(validated.downmix, static_cast<int>(DownmixOption::Off), static_cast<int>(DownmixOption::Mono));
    validated.mode &= static_cast<int>(MinMax | Rms | Silence);

    if(!validated.colourOptions.canConvert<Colours>()
       || (validated.colourOptions.isValid() && validated.colourOptions.value<Colours>().isEmpty())) {
        validated.colourOptions = QVariant{};
    }

    m_settings->fileSet(ShowLabelsKey, validated.showLabels);
    m_settings->fileSet(ShowRemainingTimeKey, validated.showRemainingTime);
    m_settings->fileRemove(LegacyElapsedTotalKey);
    m_settings->fileSet(ShowCursorKey, validated.showCursor);
    m_settings->fileSet(CursorWidthKey, validated.cursorWidth);
    m_settings->fileSet(ModeKey, validated.mode);
    m_settings->fileSet(DownmixKey, validated.downmix);
    m_settings->fileSet(BarWidthKey, validated.barWidth);
    m_settings->fileSet(BarGapKey, validated.barGap);
    m_settings->fileSet(SupersampleFactorKey, validated.supersampleFactor);
    m_settings->fileSet(PeakDisplayModeKey, validated.peakDisplayMode);
    m_settings->fileSet(NormaliseToPeakKey, validated.normaliseToPeak);
    m_settings->fileSet(DecibelScaleKey, validated.decibelScale);
    m_settings->fileSet(MaxScaleKey, validated.maxScale);
    m_settings->fileSet(CentreGapKey, validated.centreGap);
    m_settings->fileSet(ChannelScaleKey, validated.channelScale);
    m_settings->fileSet(ColoursKey, validated.colourOptions);
}

void WaveBarWidget::clearSavedDefaults() const
{
    m_settings->fileRemove(ShowLabelsKey);
    m_settings->fileRemove(ShowRemainingTimeKey);
    m_settings->fileRemove(LegacyElapsedTotalKey);
    m_settings->fileRemove(ShowCursorKey);
    m_settings->fileRemove(CursorWidthKey);
    m_settings->fileRemove(ModeKey);
    m_settings->fileRemove(DownmixKey);
    m_settings->fileRemove(BarWidthKey);
    m_settings->fileRemove(BarGapKey);
    m_settings->fileRemove(SupersampleFactorKey);
    m_settings->fileRemove(PeakDisplayModeKey);
    m_settings->fileRemove(NormaliseToPeakKey);
    m_settings->fileRemove(DecibelScaleKey);
    m_settings->fileRemove(MaxScaleKey);
    m_settings->fileRemove(CentreGapKey);
    m_settings->fileRemove(ChannelScaleKey);
    m_settings->fileRemove(ColoursKey);
    m_settings->reset<Settings::WaveBar::NumSamples>();
}

void WaveBarWidget::applyConfig(const ConfigData& config)
{
    auto validated{config};

    validated.cursorWidth       = std::clamp(validated.cursorWidth, 1, 20);
    validated.barWidth          = std::clamp(validated.barWidth, 1, 50);
    validated.barGap            = std::clamp(validated.barGap, 0, 50);
    validated.supersampleFactor = normaliseSupersampleFactor(validated.supersampleFactor);
    validated.peakDisplayMode   = std::clamp(validated.peakDisplayMode, static_cast<int>(PeakDisplayMode::Maximum),
                                             static_cast<int>(PeakDisplayMode::SmoothedAverage));
    validated.maxScale          = std::clamp(validated.maxScale, 0.0, 2.0);
    validated.centreGap         = std::clamp(validated.centreGap, 0, 10);
    validated.channelScale      = std::clamp(validated.channelScale, 0.0, 1.0);
    validated.downmix
        = std::clamp(validated.downmix, static_cast<int>(DownmixOption::Off), static_cast<int>(DownmixOption::Mono));
    validated.mode &= static_cast<int>(MinMax | Rms | Silence);

    if(!validated.colourOptions.canConvert<Colours>()
       || (validated.colourOptions.isValid() && validated.colourOptions.value<Colours>().isEmpty())) {
        validated.colourOptions = QVariant{};
    }

    m_config = validated;

    m_container->setLabelsEnabled(m_config.showLabels);
    m_container->setShowRemainingTime(m_config.showRemainingTime);

    m_seekbar->setShowCursor(m_config.showCursor);
    m_seekbar->setCursorWidth(m_config.cursorWidth);
    m_seekbar->setChannelScale(m_config.channelScale);
    m_seekbar->setBarWidth(m_config.barWidth);
    m_seekbar->setBarGap(m_config.barGap);
    m_seekbar->setSupersampleFactor(m_config.supersampleFactor);
    m_seekbar->setMaxScale(m_config.maxScale);
    m_seekbar->setCentreGap(m_config.centreGap);
    m_seekbar->setMode(static_cast<WaveModes>(m_config.mode));
    m_seekbar->setColours(m_config.colourOptions.isValid() ? m_config.colourOptions.value<Colours>() : Colours{});

    m_builder->setSampleWidth(m_config.barWidth + m_config.barGap);
    m_builder->setDownmix(static_cast<DownmixOption>(m_config.downmix));
    m_builder->setSupersampleFactor(m_config.supersampleFactor);
    m_builder->setPeakDisplayMode(static_cast<PeakDisplayMode>(m_config.peakDisplayMode));
    m_builder->setNormaliseToPeak(m_config.normaliseToPeak);
    m_builder->setDecibelScale(m_config.decibelScale);

    QMetaObject::invokeMethod(m_container, [this]() { rescaleWaveform(); }, Qt::QueuedConnection);
}

WaveBarWidget::ConfigData WaveBarWidget::configFromLayout(const QJsonObject& layout) const
{
    ConfigData config{defaultConfig()};

    if(layout.contains("ShowLabels"_L1)) {
        config.showLabels = layout.value("ShowLabels"_L1).toBool();
    }
    if(layout.contains("ShowRemainingTime"_L1) || layout.contains("ElapsedTotal"_L1)) {
        const auto key           = layout.contains("ShowRemainingTime"_L1) ? "ShowRemainingTime"_L1 : "ElapsedTotal"_L1;
        config.showRemainingTime = layout.value(key).toBool();
    }
    if(layout.contains("ShowCursor"_L1)) {
        config.showCursor = layout.value("ShowCursor"_L1).toBool();
    }
    if(layout.contains("CursorWidth"_L1)) {
        config.cursorWidth = layout.value("CursorWidth"_L1).toInt();
    }
    if(layout.contains("Mode"_L1)) {
        config.mode = layout.value("Mode"_L1).toInt();
    }
    if(layout.contains("Downmix"_L1)) {
        config.downmix = layout.value("Downmix"_L1).toInt();
    }
    if(layout.contains("BarWidth"_L1)) {
        config.barWidth = layout.value("BarWidth"_L1).toInt();
    }
    if(layout.contains("BarGap"_L1)) {
        config.barGap = layout.value("BarGap"_L1).toInt();
    }
    if(layout.contains("SupersampleFactor"_L1)) {
        config.supersampleFactor = layout.value("SupersampleFactor"_L1).toInt();
    }
    if(layout.contains("PeakDisplayMode"_L1)) {
        config.peakDisplayMode = layout.value("PeakDisplayMode"_L1).toInt();
    }
    if(layout.contains("NormaliseToPeak"_L1)) {
        config.normaliseToPeak = layout.value("NormaliseToPeak"_L1).toBool();
    }
    if(layout.contains("DecibelScale"_L1)) {
        config.decibelScale = layout.value("DecibelScale"_L1).toBool();
    }
    if(layout.contains("MaxScale"_L1)) {
        config.maxScale = layout.value("MaxScale"_L1).toDouble();
    }
    if(layout.contains("CentreGap"_L1)) {
        config.centreGap = layout.value("CentreGap"_L1).toInt();
    }
    if(layout.contains("ChannelScale"_L1)) {
        config.channelScale = layout.value("ChannelScale"_L1).toDouble();
    }

    if(layout.contains("UseCustomColours"_L1)) {
        if(layout.value("UseCustomColours"_L1).toBool()) {
            auto colours = Colours{};

            const auto setColour = [&layout, &colours](const QString& key, Colours::Type type) {
                if(!layout.contains(key)) {
                    return;
                }

                const QColor loadedColour{layout.value(key).toString()};
                if(loadedColour.isValid()) {
                    colours.setColour(type, loadedColour);
                }
            };

            setColour(u"BgUnplayedColour"_s, Colours::Type::BgUnplayed);
            setColour(u"BgPlayedColour"_s, Colours::Type::BgPlayed);
            setColour(u"MaxUnplayedColour"_s, Colours::Type::MaxUnplayed);
            setColour(u"MaxPlayedColour"_s, Colours::Type::MaxPlayed);
            setColour(u"MaxBorderColour"_s, Colours::Type::MaxBorder);
            setColour(u"MinUnplayedColour"_s, Colours::Type::MinUnplayed);
            setColour(u"MinPlayedColour"_s, Colours::Type::MinPlayed);
            setColour(u"MinBorderColour"_s, Colours::Type::MinBorder);
            setColour(u"RmsMaxUnplayedColour"_s, Colours::Type::RmsMaxUnplayed);
            setColour(u"RmsMaxPlayedColour"_s, Colours::Type::RmsMaxPlayed);
            setColour(u"RmsMaxBorderColour"_s, Colours::Type::RmsMaxBorder);
            setColour(u"RmsMinUnplayedColour"_s, Colours::Type::RmsMinUnplayed);
            setColour(u"RmsMinPlayedColour"_s, Colours::Type::RmsMinPlayed);
            setColour(u"RmsMinBorderColour"_s, Colours::Type::RmsMinBorder);
            setColour(u"CursorColour"_s, Colours::Type::Cursor);
            setColour(u"SeekingCursorColour"_s, Colours::Type::SeekingCursor);

            if(!colours.isEmpty()) {
                config.colourOptions = QVariant::fromValue(colours);
            }
        }
        else {
            config.colourOptions = QVariant{};
        }
    }

    return config;
}

void WaveBarWidget::saveConfigToLayout(const ConfigData& config, QJsonObject& layout) const
{
    layout["ShowLabels"_L1]        = config.showLabels;
    layout["ShowRemainingTime"_L1] = config.showRemainingTime;
    layout.remove("ElapsedTotal"_L1);
    layout["ShowCursor"_L1]        = config.showCursor;
    layout["CursorWidth"_L1]       = config.cursorWidth;
    layout["Mode"_L1]              = config.mode;
    layout["Downmix"_L1]           = config.downmix;
    layout["BarWidth"_L1]          = config.barWidth;
    layout["BarGap"_L1]            = config.barGap;
    layout["SupersampleFactor"_L1] = config.supersampleFactor;
    layout["PeakDisplayMode"_L1]   = config.peakDisplayMode;
    layout["NormaliseToPeak"_L1]   = config.normaliseToPeak;
    layout["DecibelScale"_L1]      = config.decibelScale;
    layout["MaxScale"_L1]          = config.maxScale;
    layout["CentreGap"_L1]         = config.centreGap;
    layout["ChannelScale"_L1]      = config.channelScale;

    const bool customColours      = config.colourOptions.isValid() && config.colourOptions.canConvert<Colours>()
                                 && !config.colourOptions.value<Colours>().isEmpty();
    layout["UseCustomColours"_L1] = customColours;
    if(!customColours) {
        layout.remove("BgUnplayedColour"_L1);
        layout.remove("BgPlayedColour"_L1);
        layout.remove("MaxUnplayedColour"_L1);
        layout.remove("MaxPlayedColour"_L1);
        layout.remove("MaxBorderColour"_L1);
        layout.remove("MinUnplayedColour"_L1);
        layout.remove("MinPlayedColour"_L1);
        layout.remove("MinBorderColour"_L1);
        layout.remove("RmsMaxUnplayedColour"_L1);
        layout.remove("RmsMaxPlayedColour"_L1);
        layout.remove("RmsMaxBorderColour"_L1);
        layout.remove("RmsMinUnplayedColour"_L1);
        layout.remove("RmsMinPlayedColour"_L1);
        layout.remove("RmsMinBorderColour"_L1);
        layout.remove("CursorColour"_L1);
        layout.remove("SeekingCursorColour"_L1);
        return;
    }

    const auto colours    = config.colourOptions.value<Colours>();
    const auto saveColour = [&layout, &colours](const QString& key, Colours::Type type) {
        if(colours.hasOverride(type)) {
            layout[key] = colours.waveColours.value(type).name(QColor::HexArgb);
        }
        else {
            layout.remove(key);
        }
    };

    saveColour(u"BgUnplayedColour"_s, Colours::Type::BgUnplayed);
    saveColour(u"BgPlayedColour"_s, Colours::Type::BgPlayed);
    saveColour(u"MaxUnplayedColour"_s, Colours::Type::MaxUnplayed);
    saveColour(u"MaxPlayedColour"_s, Colours::Type::MaxPlayed);
    saveColour(u"MaxBorderColour"_s, Colours::Type::MaxBorder);
    saveColour(u"MinUnplayedColour"_s, Colours::Type::MinUnplayed);
    saveColour(u"MinPlayedColour"_s, Colours::Type::MinPlayed);
    saveColour(u"MinBorderColour"_s, Colours::Type::MinBorder);
    saveColour(u"RmsMaxUnplayedColour"_s, Colours::Type::RmsMaxUnplayed);
    saveColour(u"RmsMaxPlayedColour"_s, Colours::Type::RmsMaxPlayed);
    saveColour(u"RmsMaxBorderColour"_s, Colours::Type::RmsMaxBorder);
    saveColour(u"RmsMinUnplayedColour"_s, Colours::Type::RmsMinUnplayed);
    saveColour(u"RmsMinPlayedColour"_s, Colours::Type::RmsMinPlayed);
    saveColour(u"RmsMinBorderColour"_s, Colours::Type::RmsMinBorder);
    saveColour(u"CursorColour"_s, Colours::Type::Cursor);
    saveColour(u"SeekingCursorColour"_s, Colours::Type::SeekingCursor);
}

void WaveBarWidget::changeTrack(const Track& track, bool update)
{
    m_seekbar->setPosition(m_playerController->currentPosition());
    m_builder->generateAndScale(track, update);
}

void WaveBarWidget::showEvent(QShowEvent* event)
{
    FyWidget::showEvent(event);

    rescaleWaveform();
}

void WaveBarWidget::resizeEvent(QResizeEvent* event)
{
    FyWidget::resizeEvent(event);

    rescaleWaveform();
}

void WaveBarWidget::openConfigDialog()
{
    showConfigDialog(new WaveBarConfigDialog(this, this));
}

void WaveBarWidget::contextMenuEvent(QContextMenuEvent* event)
{
    if(m_seekbar->isSeeking()) {
        m_seekbar->stopSeeking();
        return;
    }

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* showCursor = new QAction(tr("Show cursor"), menu);
    showCursor->setCheckable(true);
    showCursor->setChecked(m_config.showCursor);
    QObject::connect(showCursor, &QAction::triggered, this, [this](bool checked) {
        auto config{m_config};
        config.showCursor = checked;
        applyConfig(config);
    });

    auto* showLabels = new QAction(tr("Show labels"), menu);
    showLabels->setCheckable(true);
    showLabels->setChecked(m_config.showLabels);
    QObject::connect(showLabels, &QAction::triggered, this, [this](bool checked) {
        auto config{m_config};
        config.showLabels = checked;
        applyConfig(config);
    });

    auto* showRemainingTime = new QAction(tr("Show remaining time"), menu);
    showRemainingTime->setCheckable(true);
    showRemainingTime->setChecked(m_config.showRemainingTime);
    QObject::connect(showRemainingTime, &QAction::triggered, this, [this](bool checked) {
        auto config{m_config};
        config.showRemainingTime = checked;
        applyConfig(config);
    });

    auto* normaliseToPeak = new QAction(tr("Normalise waveform"), menu);
    normaliseToPeak->setCheckable(true);
    normaliseToPeak->setChecked(m_config.normaliseToPeak);
    QObject::connect(normaliseToPeak, &QAction::triggered, this, [this](bool checked) {
        auto config{m_config};
        config.normaliseToPeak = checked;
        applyConfig(config);
    });

    auto* decibelScale = new QAction(tr("dB scale"), menu);
    decibelScale->setCheckable(true);
    decibelScale->setChecked(m_config.decibelScale);
    QObject::connect(decibelScale, &QAction::triggered, this, [this](bool checked) {
        auto config{m_config};
        config.decibelScale = checked;
        applyConfig(config);
    });

    auto* peakDisplayMenu  = new QMenu(tr("Peak display"), menu);
    auto* peakDisplayGroup = new QActionGroup(peakDisplayMenu);

    auto* maximumPeaks    = new QAction(tr("Maximum"), peakDisplayGroup);
    auto* averagePeaks    = new QAction(tr("Average"), peakDisplayGroup);
    auto* smoothedAverage = new QAction(tr("Smoothed average"), peakDisplayGroup);

    maximumPeaks->setCheckable(true);
    averagePeaks->setCheckable(true);
    smoothedAverage->setCheckable(true);

    const auto peakDisplayMode = static_cast<PeakDisplayMode>(m_config.peakDisplayMode);
    if(peakDisplayMode == PeakDisplayMode::SmoothedAverage) {
        smoothedAverage->setChecked(true);
    }
    else if(peakDisplayMode == PeakDisplayMode::Average) {
        averagePeaks->setChecked(true);
    }
    else {
        maximumPeaks->setChecked(true);
    }

    const auto updatePeakConfig = [this](PeakDisplayMode peakMode) {
        auto config{m_config};
        config.peakDisplayMode = static_cast<int>(peakMode);
        applyConfig(config);
    };

    QObject::connect(maximumPeaks, &QAction::triggered, this,
                     [updatePeakConfig]() { updatePeakConfig(PeakDisplayMode::Maximum); });
    QObject::connect(averagePeaks, &QAction::triggered, this,
                     [updatePeakConfig]() { updatePeakConfig(PeakDisplayMode::Average); });
    QObject::connect(smoothedAverage, &QAction::triggered, this,
                     [updatePeakConfig]() { updatePeakConfig(PeakDisplayMode::SmoothedAverage); });

    peakDisplayMenu->addAction(maximumPeaks);
    peakDisplayMenu->addAction(averagePeaks);
    peakDisplayMenu->addAction(smoothedAverage);

    auto* modeMenu = new QMenu(tr("Display"), menu);

    auto* minMaxMode  = new QAction(tr("Min/Max"), modeMenu);
    auto* rmsMode     = new QAction(tr("RMS"), modeMenu);
    auto* silenceMode = new QAction(tr("Silence"), modeMenu);

    minMaxMode->setCheckable(true);
    rmsMode->setCheckable(true);
    silenceMode->setCheckable(true);

    const auto currentMode = static_cast<WaveModes>(m_config.mode);
    minMaxMode->setChecked(currentMode & MinMax);
    rmsMode->setChecked(currentMode & Rms);
    silenceMode->setChecked(currentMode & Silence);

    auto updateMode = [this](WaveMode mode, bool enable) {
        auto updatedMode = static_cast<WaveModes>(m_config.mode);
        if(enable) {
            updatedMode |= mode;
        }
        else {
            updatedMode &= ~mode;
        }

        auto config = m_config;
        config.mode = static_cast<int>(updatedMode);
        applyConfig(config);
    };

    QObject::connect(minMaxMode, &QAction::triggered, this,
                     [updateMode](bool checked) { updateMode(MinMax, checked); });
    QObject::connect(rmsMode, &QAction::triggered, this, [updateMode](bool checked) { updateMode(Rms, checked); });
    QObject::connect(silenceMode, &QAction::triggered, this,
                     [updateMode](bool checked) { updateMode(Silence, checked); });

    modeMenu->addAction(minMaxMode);
    modeMenu->addAction(rmsMode);
    modeMenu->addAction(silenceMode);

    auto* downmixMenu  = new QMenu(tr("Downmix"), menu);
    auto* downmixGroup = new QActionGroup(downmixMenu);

    auto* downmixOff    = new QAction(tr("Off"), downmixGroup);
    auto* downmixStereo = new QAction(tr("Stereo"), downmixGroup);
    auto* downmixMono   = new QAction(tr("Mono"), downmixGroup);

    downmixOff->setCheckable(true);
    downmixStereo->setCheckable(true);
    downmixMono->setCheckable(true);

    const auto downmixOption = static_cast<DownmixOption>(m_config.downmix);
    if(downmixOption == DownmixOption::Off) {
        downmixOff->setChecked(true);
    }
    else if(downmixOption == DownmixOption::Stereo) {
        downmixStereo->setChecked(true);
    }
    else {
        downmixMono->setChecked(true);
    }

    QObject::connect(downmixOff, &QAction::triggered, this, [this]() {
        auto config    = m_config;
        config.downmix = static_cast<int>(DownmixOption::Off);
        applyConfig(config);
    });
    QObject::connect(downmixStereo, &QAction::triggered, this, [this]() {
        auto config    = m_config;
        config.downmix = static_cast<int>(DownmixOption::Stereo);
        applyConfig(config);
    });
    QObject::connect(downmixMono, &QAction::triggered, this, [this]() {
        auto config    = m_config;
        config.downmix = static_cast<int>(DownmixOption::Mono);
        applyConfig(config);
    });

    downmixMenu->addAction(downmixOff);
    downmixMenu->addAction(downmixStereo);
    downmixMenu->addAction(downmixMono);

    menu->addAction(showCursor);
    menu->addAction(showLabels);
    menu->addAction(showRemainingTime);
    menu->addAction(normaliseToPeak);
    menu->addAction(decibelScale);
    menu->addSeparator();
    menu->addMenu(modeMenu);
    menu->addMenu(peakDisplayMenu);
    menu->addMenu(downmixMenu);
    addConfigureAction(menu);

    menu->popup(event->globalPos());
}

void WaveBarWidget::rescaleWaveform()
{
    m_builder->rescale(m_seekbar->contentsRect().width());
}
} // namespace Fooyin::WaveBar

#include "moc_wavebarwidget.cpp"
