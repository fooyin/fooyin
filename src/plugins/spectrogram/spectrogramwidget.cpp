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

#include "spectrogramwidget.h"

#include "spectrogramconfigwidget.h"

#include <core/engine/enginecontroller.h>
#include <core/engine/visualisationservice.h>
#include <core/player/playercontroller.h>
#include <utils/settings/settingsmanager.h>

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMenu>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimerEvent>

#include <array>
#include <vector>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(SPECTROGRAM, "fy.spectrogram")

constexpr auto ChannelModeKey                = u"Spectrogram/ChannelMode";
constexpr auto PresentationModeKey           = u"Spectrogram/PresentationMode";
constexpr auto LogFrequencyKey               = u"Spectrogram/LogFrequency";
constexpr auto ClearOnTrackChangeKey         = u"Spectrogram/ClearOnTrackChange";
constexpr auto ChannelSpacingKey             = u"Spectrogram/ChannelSpacing";
constexpr auto FftSizeKey                    = u"Spectrogram/FftSize";
constexpr auto MinDbKey                      = u"Spectrogram/MinDb";
constexpr auto MaxDbKey                      = u"Spectrogram/MaxDb";
constexpr auto WindowFunctionKey             = u"Spectrogram/WindowFunction";
constexpr auto ColoursKey                    = u"Spectrogram/Colours";
constexpr auto BacklogMs                     = 5000;
constexpr uint64_t AnalysisHopMs             = 25;
constexpr auto PresentationIntervalMs        = 8;
constexpr auto MaximumCatchUpColumns         = 16;
constexpr uint64_t BackwardsClockToleranceMs = 250;
constexpr auto PresentationResetThresholdMs  = 300.0;
constexpr auto SeekSettleMs                  = 150;
constexpr auto SeekPositionTimeoutMs         = 1000;
constexpr auto MinimumLogFrequencyHz         = 25.0;

namespace Fooyin::Spectrogram {
namespace {
std::vector<QColor> defaultColours()
{
    return {{0, 0, 0}, {0, 32, 100}, {0, 148, 160}, {128, 255, 120}, {255, 255, 0}, {255, 128, 0}, {255, 0, 0}};
}

std::vector<QColor> normaliseColours(const std::vector<QColor>& colours)
{
    std::vector<QColor> result;
    std::ranges::copy_if(colours, std::back_inserter(result), [](const QColor& colour) { return colour.isValid(); });
    return result.empty() ? defaultColours() : result;
}

QStringList serialiseColours(const std::vector<QColor>& colours)
{
    QStringList result;
    for(const QColor& colour : normaliseColours(colours)) {
        result.push_back(colour.name(QColor::HexArgb));
    }
    return result;
}

std::vector<QColor> deserialiseColours(const QStringList& values, const std::vector<QColor>& fallback)
{
    std::vector<QColor> colours;
    colours.reserve(static_cast<size_t>(values.size()));

    for(const QString& value : values) {
        const QColor colour{value};
        if(colour.isValid()) {
            colours.push_back(colour);
        }
    }

    return colours.empty() ? fallback : colours;
}

int normaliseFftSize(int fftSize)
{
    int best      = SpectrogramWidget::FftSizes.front();
    int bestDelta = std::abs(fftSize - best);

    for(const int candidate : SpectrogramWidget::FftSizes) {
        const int delta = std::abs(fftSize - candidate);
        if(delta < bestDelta) {
            best      = candidate;
            bestDelta = delta;
        }
    }

    return best;
}

int normaliseWindowFunction(int value)
{
    return std::clamp(value, static_cast<int>(VisualisationSession::SpectrumWindowFunction::Hann),
                      static_cast<int>(VisualisationSession::SpectrumWindowFunction::None));
}

int normaliseChannelSpacing(int value)
{
    return std::clamp(value, 0, 100);
}

SpectrogramWidget::ChannelMode normaliseChannelMode(SpectrogramWidget::ChannelMode mode)
{
    using ChannelMode = SpectrogramWidget::ChannelMode;
    return std::clamp(mode, ChannelMode::Unchanged, ChannelMode::BackOnly);
}

SpectrogramWidget::PresentationMode normalisePresentationMode(SpectrogramWidget::PresentationMode mode)
{
    using PresentationMode = SpectrogramWidget::PresentationMode;
    return std::clamp(mode, PresentationMode::Scrolling, PresentationMode::Stationary);
}

VisualisationSession::SpectrumWindowFunction windowFunctionFor(const SpectrogramWidget::ConfigData& config)
{
    return static_cast<VisualisationSession::SpectrumWindowFunction>(normaliseWindowFunction(config.windowFunction));
}

double binPositionForRatio(const VisualisationSession::SpectrumWindow& spectrum, bool logFrequency, double ratio)
{
    const int maxBinIndex = spectrum.binCount() - 1;
    if(maxBinIndex <= 0) {
        return 0.0;
    }

    const double clampedRatio = std::clamp(ratio, 0.0, 1.0);

    if(logFrequency) {
        const double binFrequency = static_cast<double>(spectrum.sampleRate) / static_cast<double>(spectrum.fftSize);
        const double minimumBin
            = std::clamp(MinimumLogFrequencyHz / binFrequency, 1.0, static_cast<double>(maxBinIndex));
        return std::exp(std::lerp(std::log(minimumBin), std::log(static_cast<double>(maxBinIndex)), clampedRatio));
    }

    return clampedRatio * static_cast<double>(maxBinIndex);
}

double magnitudeDb(float magnitude)
{
    static constexpr double MinMagnitude = 1.0e-12;
    return 20.0 * std::log10(std::max<double>(magnitude, MinMagnitude));
}

QColor colourForLevel(double level, const std::vector<QColor>& colours)
{
    if(colours.size() == 1) {
        return colours.front();
    }

    const double position = std::clamp(level, 0.0, 1.0) * static_cast<double>(colours.size() - 1);
    const auto first      = static_cast<size_t>(std::floor(position));
    const size_t second   = std::min(first + 1, colours.size() - 1);
    const auto fraction   = static_cast<float>(position - static_cast<double>(first));

    const QColor& from = colours[first];
    const QColor& to   = colours[second];

    return QColor::fromRgbF(
        std::lerp(from.redF(), to.redF(), fraction), std::lerp(from.greenF(), to.greenF(), fraction),
        std::lerp(from.blueF(), to.blueF(), fraction), std::lerp(from.alphaF(), to.alphaF(), fraction));
}

float sampleMagnitude(const VisualisationSession::SpectrumWindow& spectrum, bool logFrequency, int row, int rows)
{
    if(!spectrum.isValid() || rows <= 0) {
        return 0.0F;
    }

    const int maxBinIndex = spectrum.binCount() - 1;
    if(maxBinIndex <= 0) {
        return 0.0F;
    }

    const double upperRatio = 1.0 - (static_cast<double>(row) / static_cast<double>(rows));
    const double lowerRatio = 1.0 - (static_cast<double>(row + 1) / static_cast<double>(rows));
    const double binStart   = binPositionForRatio(spectrum, logFrequency, lowerRatio);
    const double binEnd     = binPositionForRatio(spectrum, logFrequency, upperRatio);

    const auto bins = spectrum.bins();

    if(binEnd - binStart < 1.0) {
        const double center = std::clamp((binStart + binEnd) * 0.5, 0.0, static_cast<double>(maxBinIndex));
        const int lowerBin  = static_cast<int>(std::floor(center));
        const int upperBin  = std::min(lowerBin + 1, maxBinIndex);
        return static_cast<float>(std::lerp(static_cast<double>(bins[static_cast<size_t>(lowerBin)]),
                                            static_cast<double>(bins[static_cast<size_t>(upperBin)]),
                                            center - static_cast<double>(lowerBin)));
    }

    double peak{0.0};

    // Use only bins whose centres fall inside this pixel's frequency interval
    const int firstBin = std::clamp(static_cast<int>(std::ceil(binStart)), 0, maxBinIndex);
    const int endBin   = std::clamp(static_cast<int>(std::ceil(binEnd)), firstBin + 1, maxBinIndex + 1);

    for(int bin{firstBin}; bin < endBin; ++bin) {
        const double magnitude = bins[static_cast<size_t>(bin)];
        peak                   = std::max(peak, magnitude);
    }

    return static_cast<float>(peak);
}

void fillSpectrumSection(std::vector<float>& levels, int top, int height,
                         const VisualisationSession::SpectrumWindow& spectrum,
                         const SpectrogramWidget::ConfigData& config)
{
    if(height <= 0 || top < 0 || std::cmp_greater(top + height, levels.size())) {
        return;
    }

    const auto levelSpan = static_cast<double>(std::max(1, config.maxDb - config.minDb));

    for(int row{0}; row < height; ++row) {
        const double db = magnitudeDb(sampleMagnitude(spectrum, config.logFrequency, row, height));
        const float level
            = db < static_cast<double>(config.minDb)
                ? -1.0F
                : static_cast<float>(std::clamp((db - static_cast<double>(config.minDb)) / levelSpan, 0.0, 1.0));
        levels[top + row] = level;
    }
}
} // namespace

SpectrogramWidget::SpectrogramWidget(PlayerController* playerController, EngineController* engine,
                                     SettingsManager* settings, QWidget* parent)
    : FyWidget{parent}
    , m_playerController{playerController}
    , m_visualisationService{engine->visualisationService()}
    , m_settings{settings}
    , m_track{playerController->currentTrack()}
    , m_resizeSourceWritePixel{0}
    , m_resizeSourceHistoryPixelCount{0}
    , m_resizeSettleTimer{}
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

    m_config = defaultConfig();
    applyConfig(m_config);

    QObject::connect(m_playerController, &PlayerController::currentTrackChanged, this, &SpectrogramWidget::setTrack);
    QObject::connect(m_playerController, &PlayerController::playStateChanged, this,
                     [this](Player::PlayState state) { m_playState = state; });
    QObject::connect(m_playerController, &PlayerController::positionMoved, this,
                     &SpectrogramWidget::handlePositionMoved);
    QObject::connect(m_playerController, &PlayerController::positionChanged, this,
                     &SpectrogramWidget::handlePositionChanged);

    QObject::connect(engine, &EngineController::engineStateChanged, this, &SpectrogramWidget::setEngineState);
    QObject::connect(engine, &EngineController::audiblePauseDrainStarted, this,
                     [this]() { setPauseDrainActive(true); });
    QObject::connect(engine, &EngineController::audiblePauseDrainCompleted, this,
                     [this]() { setPauseDrainActive(false); });
}

QString SpectrogramWidget::name() const
{
    return u"Spectrogram"_s;
}

QString SpectrogramWidget::layoutName() const
{
    return u"Spectrogram"_s;
}

void SpectrogramWidget::saveLayoutData(QJsonObject& layout)
{
    saveConfigToLayout(m_config, layout);
}

void SpectrogramWidget::loadLayoutData(const QJsonObject& layout)
{
    applyConfig(configFromLayout(layout));
}

SpectrogramWidget::ConfigData SpectrogramWidget::factoryConfig() const
{
    ConfigData config{};
    config.colours = defaultColours();
    return config;
}

SpectrogramWidget::ConfigData SpectrogramWidget::defaultConfig() const
{
    auto config{factoryConfig()};

    config.channelMode
        = static_cast<ChannelMode>(m_settings->fileValue(ChannelModeKey, static_cast<int>(config.channelMode)).toInt());
    config.presentationMode = static_cast<PresentationMode>(
        m_settings->fileValue(PresentationModeKey, static_cast<int>(config.presentationMode)).toInt());
    config.logFrequency       = m_settings->fileValue(LogFrequencyKey, config.logFrequency).toBool();
    config.clearOnTrackChange = m_settings->fileValue(ClearOnTrackChangeKey, config.clearOnTrackChange).toBool();
    config.channelSpacing     = m_settings->fileValue(ChannelSpacingKey, config.channelSpacing).toInt();
    config.fftSize            = m_settings->fileValue(FftSizeKey, config.fftSize).toInt();
    config.minDb              = m_settings->fileValue(MinDbKey, config.minDb).toInt();
    config.maxDb              = m_settings->fileValue(MaxDbKey, config.maxDb).toInt();
    config.windowFunction     = m_settings->fileValue(WindowFunctionKey, config.windowFunction).toInt();
    config.colours            = deserialiseColours(
        m_settings->fileValue(ColoursKey, serialiseColours(config.colours)).toStringList(), config.colours);

    return config;
}

const SpectrogramWidget::ConfigData& SpectrogramWidget::currentConfig() const
{
    return m_config;
}

void SpectrogramWidget::saveDefaults(const ConfigData& config) const
{
    ConfigData validated{config};

    validated.channelMode      = normaliseChannelMode(validated.channelMode);
    validated.presentationMode = normalisePresentationMode(validated.presentationMode);
    validated.channelSpacing   = normaliseChannelSpacing(validated.channelSpacing);
    validated.fftSize          = normaliseFftSize(validated.fftSize);

    validated.minDb = std::clamp(validated.minDb, -120, 0);
    validated.maxDb = std::clamp(validated.maxDb, -60, 12);
    if(validated.maxDb <= validated.minDb) {
        validated.maxDb = std::min(12, validated.minDb + 12);
    }

    validated.windowFunction = normaliseWindowFunction(validated.windowFunction);
    validated.colours        = normaliseColours(validated.colours);

    m_settings->fileSet(ChannelModeKey, static_cast<int>(validated.channelMode));
    m_settings->fileSet(PresentationModeKey, static_cast<int>(validated.presentationMode));
    m_settings->fileSet(LogFrequencyKey, validated.logFrequency);
    m_settings->fileSet(ClearOnTrackChangeKey, validated.clearOnTrackChange);
    m_settings->fileSet(ChannelSpacingKey, validated.channelSpacing);
    m_settings->fileSet(FftSizeKey, validated.fftSize);
    m_settings->fileSet(MinDbKey, validated.minDb);
    m_settings->fileSet(MaxDbKey, validated.maxDb);
    m_settings->fileSet(WindowFunctionKey, validated.windowFunction);
    m_settings->fileSet(ColoursKey, serialiseColours(validated.colours));
}

void SpectrogramWidget::clearSavedDefaults() const
{
    m_settings->fileRemove(ChannelModeKey);
    m_settings->fileRemove(PresentationModeKey);
    m_settings->fileRemove(LogFrequencyKey);
    m_settings->fileRemove(ClearOnTrackChangeKey);
    m_settings->fileRemove(ChannelSpacingKey);
    m_settings->fileRemove(FftSizeKey);
    m_settings->fileRemove(MinDbKey);
    m_settings->fileRemove(MaxDbKey);
    m_settings->fileRemove(WindowFunctionKey);
    m_settings->fileRemove(ColoursKey);
}

void SpectrogramWidget::applyConfig(const ConfigData& config)
{
    const PresentationMode prevMode = m_config.presentationMode;

    ConfigData validated{config};
    validated.channelMode      = normaliseChannelMode(validated.channelMode);
    validated.presentationMode = normalisePresentationMode(validated.presentationMode);
    validated.channelSpacing   = normaliseChannelSpacing(validated.channelSpacing);
    validated.fftSize          = normaliseFftSize(validated.fftSize);

    validated.minDb = std::clamp(validated.minDb, -120, 0);
    validated.maxDb = std::clamp(validated.maxDb, -60, 12);
    if(validated.maxDb <= validated.minDb) {
        validated.maxDb = std::min(12, validated.minDb + 12);
    }

    validated.windowFunction = normaliseWindowFunction(validated.windowFunction);
    validated.colours        = normaliseColours(validated.colours);

    m_config = validated;

    if(prevMode != validated.presentationMode) {
        normaliseHistoryForPresentationMode();
    }

    syncStreams();
    updateTimerState();
    renderFrame();
    update();
}

QSize SpectrogramWidget::minimumSizeHint() const
{
    return {120, 40};
}

QSize SpectrogramWidget::sizeHint() const
{
    return {320, 120};
}

void SpectrogramWidget::showEvent(QShowEvent* event)
{
    FyWidget::showEvent(event);

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

void SpectrogramWidget::resizeEvent(QResizeEvent* event)
{
    FyWidget::resizeEvent(event);

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

void SpectrogramWidget::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_timer.timerId()) {
        renderFrame();
        return;
    }

    FyWidget::timerEvent(event);
}

void SpectrogramWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{this};
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.fillRect(rect(), normaliseColours(m_config.colours).front());

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

void SpectrogramWidget::contextMenuEvent(QContextMenuEvent* event)
{
    auto* menu = new QMenu(this);

    const auto addToggle = [this, &menu](const QString& text, bool ConfigData::* option) {
        auto* action = menu->addAction(text);
        action->setCheckable(true);
        action->setChecked(m_config.*option);
        QObject::connect(action, &QAction::toggled, this, [this, option](bool checked) {
            ConfigData config{m_config};
            config.*option = checked;
            applyConfig(config);
        });
    };

    addToggle(tr("Use logarithmic frequency scale"), &ConfigData::logFrequency);
    addToggle(tr("Clear on track change"), &ConfigData::clearOnTrackChange);

    menu->addSeparator();

    auto* channelsMenu  = menu->addMenu(tr("Channels"));
    auto* channelsGroup = new QActionGroup(channelsMenu);
    channelsGroup->setExclusive(true);
    const auto addChannelMode = [this, channelsMenu, channelsGroup](const QString& text, ChannelMode mode) {
        auto* action = channelsMenu->addAction(text);
        action->setActionGroup(channelsGroup);
        action->setCheckable(true);
        action->setChecked(m_config.channelMode == mode);
        QObject::connect(action, &QAction::triggered, this, [this, mode]() {
            ConfigData config{m_config};
            config.channelMode = mode;
            applyConfig(config);
        });
    };
    addChannelMode(tr("Unchanged"), ChannelMode::Unchanged);
    addChannelMode(tr("Mono"), ChannelMode::Mono);
    addChannelMode(tr("Front Only"), ChannelMode::FrontOnly);
    addChannelMode(tr("Back Only"), ChannelMode::BackOnly);

    auto* styleMenu  = menu->addMenu(tr("Style"));
    auto* styleGroup = new QActionGroup(styleMenu);
    styleGroup->setExclusive(true);
    const auto addPresentationMode = [this, styleMenu, styleGroup](const QString& text, PresentationMode mode) {
        auto* action = styleMenu->addAction(text);
        action->setActionGroup(styleGroup);
        action->setCheckable(true);
        action->setChecked(m_config.presentationMode == mode);
        QObject::connect(action, &QAction::triggered, this, [this, mode]() {
            ConfigData config{m_config};
            config.presentationMode = mode;
            applyConfig(config);
        });
    };
    addPresentationMode(tr("Scrolling"), PresentationMode::Scrolling);
    addPresentationMode(tr("Stationary"), PresentationMode::Stationary);

    auto* fftSizeMenu  = menu->addMenu(tr("FFT size"));
    auto* fftSizeGroup = new QActionGroup(fftSizeMenu);
    fftSizeGroup->setExclusive(true);
    for(const int fftSize : FftSizes) {
        auto* action = fftSizeMenu->addAction(QString::number(fftSize));
        action->setActionGroup(fftSizeGroup);
        action->setCheckable(true);
        action->setChecked(m_config.fftSize == fftSize);
        QObject::connect(action, &QAction::triggered, this, [this, fftSize]() {
            ConfigData config{m_config};
            config.fftSize = fftSize;
            applyConfig(config);
        });
    }

    menu->addSeparator();
    addConfigureAction(menu, false);

    menu->popup(event->globalPos());
}

void SpectrogramWidget::openConfigDialog()
{
    showConfigDialog(new SpectrogramConfigDialog(this, this));
}

SpectrogramWidget::ConfigData SpectrogramWidget::configFromLayout(const QJsonObject& layout) const
{
    ConfigData config{defaultConfig()};

    if(layout.contains("ChannelMode"_L1)) {
        config.channelMode = static_cast<ChannelMode>(layout.value("ChannelMode"_L1).toInt());
    }
    if(layout.contains("PresentationMode"_L1)) {
        config.presentationMode = static_cast<PresentationMode>(layout.value("PresentationMode"_L1).toInt());
    }
    if(layout.contains("LogFrequency"_L1)) {
        config.logFrequency = layout.value("LogFrequency"_L1).toBool(config.logFrequency);
    }
    if(layout.contains("ClearOnTrackChange"_L1)) {
        config.clearOnTrackChange = layout.value("ClearOnTrackChange"_L1).toBool(config.clearOnTrackChange);
    }
    if(layout.contains("ChannelSpacing"_L1)) {
        config.channelSpacing = layout.value("ChannelSpacing"_L1).toInt(config.channelSpacing);
    }
    if(layout.contains("FftSize"_L1)) {
        config.fftSize = layout.value("FftSize"_L1).toInt(config.fftSize);
    }
    if(layout.contains("MinDb"_L1)) {
        config.minDb = layout.value("MinDb"_L1).toInt(config.minDb);
    }
    if(layout.contains("MaxDb"_L1)) {
        config.maxDb = layout.value("MaxDb"_L1).toInt(config.maxDb);
    }
    if(layout.contains("WindowFunction"_L1)) {
        config.windowFunction = layout.value("WindowFunction"_L1).toInt(config.windowFunction);
    }
    if(layout.contains("Colours"_L1) && layout.value("Colours"_L1).isArray()) {
        QStringList values;
        const auto colours = layout.value("Colours"_L1).toArray();
        for(const auto& value : colours) {
            if(value.isString()) {
                values.push_back(value.toString());
            }
        }
        config.colours = deserialiseColours(values, config.colours);
    }

    return config;
}

void SpectrogramWidget::saveConfigToLayout(const ConfigData& config, QJsonObject& layout) const
{
    layout["ChannelMode"_L1]        = static_cast<int>(config.channelMode);
    layout["PresentationMode"_L1]   = static_cast<int>(config.presentationMode);
    layout["LogFrequency"_L1]       = config.logFrequency;
    layout["ClearOnTrackChange"_L1] = config.clearOnTrackChange;
    layout["ChannelSpacing"_L1]     = config.channelSpacing;
    layout["FftSize"_L1]            = config.fftSize;
    layout["MinDb"_L1]              = config.minDb;
    layout["MaxDb"_L1]              = config.maxDb;
    layout["WindowFunction"_L1]     = config.windowFunction;

    QJsonArray colours;
    const auto configColours = serialiseColours(config.colours);
    for(const QString& colour : configColours) {
        colours.push_back(colour);
    }
    layout["Colours"_L1] = colours;
}

void SpectrogramWidget::updateTimerState()
{
    if(m_engineState != Engine::PlaybackState::Playing && !m_pauseDrainActive) {
        if(m_timer.isActive()) {
            m_timer.stop();
        }
        return;
    }

    m_timer.start(PresentationIntervalMs, Qt::PreciseTimer, this);
}

void SpectrogramWidget::resetAnalysis()
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

void SpectrogramWidget::resizeBuffers()
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

void SpectrogramWidget::normaliseHistoryForPresentationMode()
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

void SpectrogramWidget::syncStreams()
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

void SpectrogramWidget::setTrack(const Track& track)
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

void SpectrogramWidget::setEngineState(Engine::PlaybackState state)
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

void SpectrogramWidget::setPauseDrainActive(bool active)
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

void SpectrogramWidget::handlePositionMoved(uint64_t positionMs)
{
    m_seekPending             = true;
    m_seekAwaitingPosition    = true;
    m_requestedSeekPositionMs = positionMs;
    m_seekSettleClock.restart();
}

void SpectrogramWidget::handlePositionChanged(uint64_t positionMs)
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

void SpectrogramWidget::renderFrame()
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

bool SpectrogramWidget::analyseColumn(uint64_t endTimeMs)
{
    if(m_image.isNull()) {
        return false;
    }

    VisualisationSession::SpectrumWindow monoSpectrum;
    VisualisationSession::SpectrumWindow leftSpectrum;
    VisualisationSession::SpectrumWindow rightSpectrum;

    const auto windowFunction = windowFunctionFor(m_config);

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
    const auto colours = normaliseColours(m_config.colours);

    if(splitStereo && m_image.height() >= 2) {
        const int requestedSpacing = static_cast<int>(
            std::lround(static_cast<double>(normaliseChannelSpacing(m_config.channelSpacing)) * devicePixelRatioF()));
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

QImage SpectrogramWidget::linearHistoryImage() const
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

double SpectrogramWidget::oldestColumnTimeMs() const
{
    return m_historyColumnCount > 0
             ? m_newestColumnTimeMs
                   - (static_cast<double>(m_historyColumnCount - 1) * static_cast<double>(AnalysisHopMs))
             : 0.0;
}
} // namespace Fooyin::Spectrogram
