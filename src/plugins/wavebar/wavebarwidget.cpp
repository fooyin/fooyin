/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "settings/wavebarsettings.h"
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
constexpr auto ShowLabelsKey   = u"WaveBar/ShowLabels";
constexpr auto ElapsedTotalKey = u"WaveBar/ElapsedTotal";
constexpr auto ShowCursorKey   = u"WaveBar/ShowCursor";
constexpr auto CursorWidthKey  = u"WaveBar/CursorWidth";
constexpr auto ModeKey         = u"WaveBar/Mode";
constexpr auto DownmixKey      = u"WaveBar/Downmix";
constexpr auto BarWidthKey     = u"WaveBar/BarWidth";
constexpr auto BarGapKey       = u"WaveBar/BarGap";
constexpr auto MaxScaleKey     = u"WaveBar/MaxScale";
constexpr auto CentreGapKey    = u"WaveBar/CentreGap";
constexpr auto ChannelScaleKey = u"WaveBar/ChannelScale";
constexpr auto ColoursKey      = u"WaveBar/Colours";

namespace Fooyin::WaveBar {
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
    m_seekbar->setPosition(m_playerController->currentPosition());

    m_config = defaultConfig();
    applyConfig(m_config);

    QObject::connect(m_builder.get(), &WaveformBuilder::generatingWaveform, this,
                     [this]() { m_seekbar->processData({}); });
    QObject::connect(m_builder.get(), &WaveformBuilder::waveformRescaled, m_seekbar, &WaveSeekBar::processData);

    QObject::connect(playerController, &PlayerController::positionChanged, m_seekbar, &WaveSeekBar::setPosition);
    QObject::connect(playerController, &PlayerController::playStateChanged, m_seekbar, &WaveSeekBar::setPlayState);
    QObject::connect(m_seekbar, &WaveSeekBar::sliderMoved, playerController, [this](uint64_t pos) {
        m_playerController->seek(pos);
        if(m_playerController->playState() == Player::PlayState::Stopped) {
            m_playerController->play();
        }
    });
    QObject::connect(m_seekbar, &WaveSeekBar::seekForward, playerController,
                     [this]() { m_playerController->seekForward(m_settings->value<Settings::Gui::SeekStepSmall>()); });
    QObject::connect(m_seekbar, &WaveSeekBar::seekBackward, playerController,
                     [this]() { m_playerController->seekBackward(m_settings->value<Settings::Gui::SeekStepSmall>()); });

    QObject::connect(m_container, &SeekContainer::totalClicked, this, [this]() {
        auto config         = m_config;
        config.elapsedTotal = m_container->elapsedTotal();
        applyConfig(config);
    });

    auto* throttler = new SignalThrottler(this);
    throttler->setTimeout(250ms);
    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, throttler, &SignalThrottler::throttle);
    QObject::connect(throttler, &SignalThrottler::triggered, this,
                     [this]() { changeTrack(m_playerController->currentTrack()); });

    auto updateColours = [this]() {
        if(m_config.colourOptions.isValid()) {
            return;
        }

        m_seekbar->setColours(Colours{});
    };
    m_settings->subscribe<Settings::Gui::Theme>(this, updateColours);
    m_settings->subscribe<Settings::Gui::Style>(this, updateColours);
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
    return {
        .showLabels    = m_settings->fileValue(ShowLabelsKey, false).toBool(),
        .elapsedTotal  = m_settings->fileValue(ElapsedTotalKey, false).toBool(),
        .showCursor    = m_settings->fileValue(ShowCursorKey, true).toBool(),
        .cursorWidth   = m_settings->fileValue(CursorWidthKey, 3).toInt(),
        .mode          = m_settings->fileValue(ModeKey, static_cast<int>(Default)).toInt(),
        .downmix       = m_settings->fileValue(DownmixKey, 0).toInt(),
        .barWidth      = m_settings->fileValue(BarWidthKey, 1).toInt(),
        .barGap        = m_settings->fileValue(BarGapKey, 0).toInt(),
        .maxScale      = m_settings->fileValue(MaxScaleKey, 1.0).toDouble(),
        .centreGap     = m_settings->fileValue(CentreGapKey, 0).toInt(),
        .channelScale  = m_settings->fileValue(ChannelScaleKey, 0.9).toDouble(),
        .colourOptions = m_settings->fileValue(ColoursKey, QVariant{}),
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
    emit clearCacheRequested();
}

void WaveBarWidget::saveDefaults(const ConfigData& config) const
{
    auto validated{config};

    validated.cursorWidth  = std::clamp(validated.cursorWidth, 1, 20);
    validated.barWidth     = std::clamp(validated.barWidth, 1, 50);
    validated.barGap       = std::clamp(validated.barGap, 0, 50);
    validated.maxScale     = std::clamp(validated.maxScale, 0.0, 2.0);
    validated.centreGap    = std::clamp(validated.centreGap, 0, 10);
    validated.channelScale = std::clamp(validated.channelScale, 0.0, 1.0);
    validated.downmix
        = std::clamp(validated.downmix, static_cast<int>(DownmixOption::Off), static_cast<int>(DownmixOption::Mono));
    validated.mode &= static_cast<int>(MinMax | Rms | Silence);

    if(!validated.colourOptions.canConvert<Colours>()) {
        validated.colourOptions = QVariant{};
    }

    m_settings->fileSet(ShowLabelsKey, validated.showLabels);
    m_settings->fileSet(ElapsedTotalKey, validated.elapsedTotal);
    m_settings->fileSet(ShowCursorKey, validated.showCursor);
    m_settings->fileSet(CursorWidthKey, validated.cursorWidth);
    m_settings->fileSet(ModeKey, validated.mode);
    m_settings->fileSet(DownmixKey, validated.downmix);
    m_settings->fileSet(BarWidthKey, validated.barWidth);
    m_settings->fileSet(BarGapKey, validated.barGap);
    m_settings->fileSet(MaxScaleKey, validated.maxScale);
    m_settings->fileSet(CentreGapKey, validated.centreGap);
    m_settings->fileSet(ChannelScaleKey, validated.channelScale);
    m_settings->fileSet(ColoursKey, validated.colourOptions);
}

void WaveBarWidget::applyConfig(const ConfigData& config)
{
    auto validated{config};

    validated.cursorWidth  = std::clamp(validated.cursorWidth, 1, 20);
    validated.barWidth     = std::clamp(validated.barWidth, 1, 50);
    validated.barGap       = std::clamp(validated.barGap, 0, 50);
    validated.maxScale     = std::clamp(validated.maxScale, 0.0, 2.0);
    validated.centreGap    = std::clamp(validated.centreGap, 0, 10);
    validated.channelScale = std::clamp(validated.channelScale, 0.0, 1.0);
    validated.downmix
        = std::clamp(validated.downmix, static_cast<int>(DownmixOption::Off), static_cast<int>(DownmixOption::Mono));
    validated.mode &= static_cast<int>(MinMax | Rms | Silence);

    if(!validated.colourOptions.canConvert<Colours>()) {
        validated.colourOptions = QVariant{};
    }

    m_config = validated;

    m_container->setLabelsEnabled(m_config.showLabels);
    m_container->setElapsedTotal(m_config.elapsedTotal);

    m_seekbar->setShowCursor(m_config.showCursor);
    m_seekbar->setCursorWidth(m_config.cursorWidth);
    m_seekbar->setChannelScale(m_config.channelScale);
    m_seekbar->setBarWidth(m_config.barWidth);
    m_seekbar->setBarGap(m_config.barGap);
    m_seekbar->setMaxScale(m_config.maxScale);
    m_seekbar->setCentreGap(m_config.centreGap);
    m_seekbar->setMode(static_cast<WaveModes>(m_config.mode));
    m_seekbar->setColours(m_config.colourOptions.isValid() ? m_config.colourOptions.value<Colours>() : Colours{});

    m_builder->setSampleWidth(m_config.barWidth + m_config.barGap);
    m_builder->setDownmix(static_cast<DownmixOption>(m_config.downmix));

    QMetaObject::invokeMethod(m_container, [this]() { rescaleWaveform(); }, Qt::QueuedConnection);
}

WaveBarWidget::ConfigData WaveBarWidget::configFromLayout(const QJsonObject& layout) const
{
    ConfigData config{defaultConfig()};

    if(layout.contains("ShowLabels"_L1)) {
        config.showLabels = layout.value("ShowLabels"_L1).toBool();
    }
    if(layout.contains("ElapsedTotal"_L1)) {
        config.elapsedTotal = layout.value("ElapsedTotal"_L1).toBool();
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

            auto setColour = [&layout](const QString& key, QColor* colour) {
                if(!layout.contains(key)) {
                    return;
                }

                const QColor loadedColour{layout.value(key).toString()};
                if(loadedColour.isValid()) {
                    *colour = loadedColour;
                }
            };

            setColour(u"BgUnplayedColour"_s, &colours.bgUnplayed);
            setColour(u"BgPlayedColour"_s, &colours.bgPlayed);
            setColour(u"MaxUnplayedColour"_s, &colours.maxUnplayed);
            setColour(u"MaxPlayedColour"_s, &colours.maxPlayed);
            setColour(u"MaxBorderColour"_s, &colours.maxBorder);
            setColour(u"MinUnplayedColour"_s, &colours.minUnplayed);
            setColour(u"MinPlayedColour"_s, &colours.minPlayed);
            setColour(u"MinBorderColour"_s, &colours.minBorder);
            setColour(u"RmsMaxUnplayedColour"_s, &colours.rmsMaxUnplayed);
            setColour(u"RmsMaxPlayedColour"_s, &colours.rmsMaxPlayed);
            setColour(u"RmsMaxBorderColour"_s, &colours.rmsMaxBorder);
            setColour(u"RmsMinUnplayedColour"_s, &colours.rmsMinUnplayed);
            setColour(u"RmsMinPlayedColour"_s, &colours.rmsMinPlayed);
            setColour(u"RmsMinBorderColour"_s, &colours.rmsMinBorder);
            setColour(u"CursorColour"_s, &colours.cursor);
            setColour(u"SeekingCursorColour"_s, &colours.seekingCursor);
            config.colourOptions = QVariant::fromValue(colours);
        }
        else {
            config.colourOptions = QVariant{};
        }
    }

    return config;
}

void WaveBarWidget::saveConfigToLayout(const ConfigData& config, QJsonObject& layout) const
{
    layout["ShowLabels"_L1]   = config.showLabels;
    layout["ElapsedTotal"_L1] = config.elapsedTotal;
    layout["ShowCursor"_L1]   = config.showCursor;
    layout["CursorWidth"_L1]  = config.cursorWidth;
    layout["Mode"_L1]         = config.mode;
    layout["Downmix"_L1]      = config.downmix;
    layout["BarWidth"_L1]     = config.barWidth;
    layout["BarGap"_L1]       = config.barGap;
    layout["MaxScale"_L1]     = config.maxScale;
    layout["CentreGap"_L1]    = config.centreGap;
    layout["ChannelScale"_L1] = config.channelScale;

    const bool customColours      = config.colourOptions.isValid() && config.colourOptions.canConvert<Colours>();
    layout["UseCustomColours"_L1] = customColours;
    if(!customColours) {
        return;
    }

    const auto colours                = config.colourOptions.value<Colours>();
    layout["BgUnplayedColour"_L1]     = colours.bgUnplayed.name(QColor::HexArgb);
    layout["BgPlayedColour"_L1]       = colours.bgPlayed.name(QColor::HexArgb);
    layout["MaxUnplayedColour"_L1]    = colours.maxUnplayed.name(QColor::HexArgb);
    layout["MaxPlayedColour"_L1]      = colours.maxPlayed.name(QColor::HexArgb);
    layout["MaxBorderColour"_L1]      = colours.maxBorder.name(QColor::HexArgb);
    layout["MinUnplayedColour"_L1]    = colours.minUnplayed.name(QColor::HexArgb);
    layout["MinPlayedColour"_L1]      = colours.minPlayed.name(QColor::HexArgb);
    layout["MinBorderColour"_L1]      = colours.minBorder.name(QColor::HexArgb);
    layout["RmsMaxUnplayedColour"_L1] = colours.rmsMaxUnplayed.name(QColor::HexArgb);
    layout["RmsMaxPlayedColour"_L1]   = colours.rmsMaxPlayed.name(QColor::HexArgb);
    layout["RmsMaxBorderColour"_L1]   = colours.rmsMaxBorder.name(QColor::HexArgb);
    layout["RmsMinUnplayedColour"_L1] = colours.rmsMinUnplayed.name(QColor::HexArgb);
    layout["RmsMinPlayedColour"_L1]   = colours.rmsMinPlayed.name(QColor::HexArgb);
    layout["RmsMinBorderColour"_L1]   = colours.rmsMinBorder.name(QColor::HexArgb);
    layout["CursorColour"_L1]         = colours.cursor.name(QColor::HexArgb);
    layout["SeekingCursorColour"_L1]  = colours.seekingCursor.name(QColor::HexArgb);
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
        auto config       = m_config;
        config.showCursor = checked;
        applyConfig(config);
    });

    auto* showLabels = new QAction(tr("Show labels"), menu);
    showLabels->setCheckable(true);
    showLabels->setChecked(m_config.showLabels);
    QObject::connect(showLabels, &QAction::triggered, this, [this](bool checked) {
        auto config       = m_config;
        config.showLabels = checked;
        applyConfig(config);
    });

    auto* showElapsed = new QAction(tr("Show elapsed total"), menu);
    showElapsed->setCheckable(true);
    showElapsed->setChecked(m_config.elapsedTotal);
    QObject::connect(showElapsed, &QAction::triggered, this, [this](bool checked) {
        auto config         = m_config;
        config.elapsedTotal = checked;
        applyConfig(config);
    });

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
    menu->addAction(showElapsed);
    menu->addSeparator();
    menu->addMenu(modeMenu);
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
