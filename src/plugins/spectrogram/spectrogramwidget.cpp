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

#include "livespectrogramview.h"
#include "spectrogramconfigwidget.h"
#include "spectrogramseekbar.h"

#include <core/engine/visualisationservice.h>
#include <gui/widgets/abrepeatcontrols.h>
#include <utils/settings/settingsmanager.h>

#include <QActionGroup>
#include <QApplication>
#include <QContextMenuEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QMenu>
#include <QVBoxLayout>

#include <array>
#include <vector>

using namespace Qt::StringLiterals;

constexpr auto ChannelModeKey           = u"Spectrogram/ChannelMode";
constexpr auto TrackPreferenceKey       = u"Spectrogram/TrackPreference";
constexpr auto PresentationModeKey      = u"Spectrogram/PresentationMode";
constexpr auto LogFrequencyKey          = u"Spectrogram/LogFrequency";
constexpr auto ClearOnTrackChangeKey    = u"Spectrogram/ClearOnTrackChange";
constexpr auto ShowTopLabelsKey         = u"Spectrogram/ShowTopLabels";
constexpr auto ShowBottomLabelsKey      = u"Spectrogram/ShowBottomLabels";
constexpr auto ShowLeftLabelsKey        = u"Spectrogram/ShowLeftLabels";
constexpr auto ShowRightLabelsKey       = u"Spectrogram/ShowRightLabels";
constexpr auto ShowCursorKey            = u"Spectrogram/ShowCursor";
constexpr auto CursorWidthKey           = u"Spectrogram/CursorWidth";
constexpr auto CursorColourKey          = u"Spectrogram/CursorColour";
constexpr auto SeekingCursorColourKey   = u"Spectrogram/SeekingCursorColour";
constexpr auto PlayedOverlayColourKey   = u"Spectrogram/PlayedOverlayColour";
constexpr auto UnplayedOverlayColourKey = u"Spectrogram/UnplayedOverlayColour";
constexpr auto ChannelSpacingKey        = u"Spectrogram/ChannelSpacing";
constexpr auto TimeColumnsKey           = u"Spectrogram/TimeColumns";
constexpr auto FftSizeKey               = u"Spectrogram/FftSize";
constexpr auto MinDbKey                 = u"Spectrogram/MinDb";
constexpr auto MaxDbKey                 = u"Spectrogram/MaxDb";
constexpr auto WindowFunctionKey        = u"Spectrogram/WindowFunction";
constexpr auto ColoursKey               = u"Spectrogram/Colours";

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
    int best      = FftSizes.front();
    int bestDelta = std::abs(fftSize - best);

    for(const int candidate : FftSizes) {
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

ChannelMode normaliseChannelMode(ChannelMode mode)
{
    using ChannelMode = ChannelMode;
    return std::clamp(mode, ChannelMode::Unchanged, ChannelMode::BackOnly);
}

PresentationMode normalisePresentationMode(PresentationMode mode)
{
    using PresentationMode = PresentationMode;
    return std::clamp(mode, PresentationMode::Scrolling, PresentationMode::FullTrack);
}

} // namespace

SpectrogramWidget::SpectrogramWidget(std::shared_ptr<AudioLoader> audioLoader, ActionManager* actionManager,
                                     PlayerController* playerController, EngineController* engine,
                                     TrackSelectionController* trackSelection, SettingsManager* settings,
                                     QWidget* parent)
    : FyWidget{parent}
    , m_audioLoader{std::move(audioLoader)}
    , m_actionManager{actionManager}
    , m_playerController{playerController}
    , m_engine{engine}
    , m_trackSelection{trackSelection}
    , m_settings{settings}
    , m_layout{new QVBoxLayout(this)}
{
    m_layout->setContentsMargins({});
    m_layout->setSpacing(0);

    applyConfig(defaultConfig());
}

QString SpectrogramWidget::name() const
{
    return tr("Spectrogram");
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

SpectrogramConfig SpectrogramWidget::factoryConfig() const
{
    SpectrogramConfig config{};
    config.colours          = defaultColours();
    config.presentationMode = normalisePresentationMode(config.presentationMode);
    return config;
}

SpectrogramConfig SpectrogramWidget::defaultConfig() const
{
    auto config{factoryConfig()};

    config.channelMode
        = static_cast<ChannelMode>(m_settings->fileValue(ChannelModeKey, static_cast<int>(config.channelMode)).toInt());
    config.presentationMode      = normalisePresentationMode(static_cast<PresentationMode>(
        m_settings->fileValue(PresentationModeKey, static_cast<int>(config.presentationMode)).toInt()));
    config.trackPreference       = m_settings->fileValue(TrackPreferenceKey, config.trackPreference).toInt();
    config.logFrequency          = m_settings->fileValue(LogFrequencyKey, config.logFrequency).toBool();
    config.clearOnTrackChange    = m_settings->fileValue(ClearOnTrackChangeKey, config.clearOnTrackChange).toBool();
    config.showTopLabels         = m_settings->fileValue(ShowTopLabelsKey, config.showTopLabels).toBool();
    config.showBottomLabels      = m_settings->fileValue(ShowBottomLabelsKey, config.showBottomLabels).toBool();
    config.showLeftLabels        = m_settings->fileValue(ShowLeftLabelsKey, config.showLeftLabels).toBool();
    config.showRightLabels       = m_settings->fileValue(ShowRightLabelsKey, config.showRightLabels).toBool();
    config.showCursor            = m_settings->fileValue(ShowCursorKey, config.showCursor).toBool();
    config.cursorWidth           = m_settings->fileValue(CursorWidthKey, config.cursorWidth).toInt();
    config.cursorColour          = QColor{m_settings->fileValue(CursorColourKey).toString()};
    config.seekingCursorColour   = QColor{m_settings->fileValue(SeekingCursorColourKey).toString()};
    config.playedOverlayColour   = QColor{m_settings->fileValue(PlayedOverlayColourKey).toString()};
    config.unplayedOverlayColour = QColor{m_settings->fileValue(UnplayedOverlayColourKey).toString()};
    config.channelSpacing        = m_settings->fileValue(ChannelSpacingKey, config.channelSpacing).toInt();
    config.fftSize               = m_settings->fileValue(FftSizeKey, config.fftSize).toInt();
    config.timeColumns           = m_settings->fileValue(TimeColumnsKey, config.timeColumns).toInt();
    config.minDb                 = m_settings->fileValue(MinDbKey, config.minDb).toInt();
    config.maxDb                 = m_settings->fileValue(MaxDbKey, config.maxDb).toInt();
    config.windowFunction        = m_settings->fileValue(WindowFunctionKey, config.windowFunction).toInt();
    config.colours               = deserialiseColours(
        m_settings->fileValue(ColoursKey, serialiseColours(config.colours)).toStringList(), config.colours);

    return config;
}

const SpectrogramConfig& SpectrogramWidget::currentConfig() const
{
    return m_config;
}

void SpectrogramWidget::saveDefaults(const SpectrogramConfig& config) const
{
    SpectrogramConfig validated{config};

    validated.trackPreference
        = std::clamp(validated.trackPreference, static_cast<int>(TrackDisplayPreference::PlayingTrack),
                     static_cast<int>(TrackDisplayPreference::PlayingTrackBlankWhenStopped));
    validated.channelMode      = normaliseChannelMode(validated.channelMode);
    validated.presentationMode = normalisePresentationMode(validated.presentationMode);
    validated.channelSpacing   = normaliseChannelSpacing(validated.channelSpacing);
    validated.fftSize          = normaliseFftSize(validated.fftSize);
    validated.timeColumns      = std::clamp(validated.timeColumns, MinTimeColumns, MaxTimeColumns);
    validated.cursorWidth      = std::clamp(validated.cursorWidth, 1, 20);

    validated.minDb = std::clamp(validated.minDb, -120, 0);
    validated.maxDb = std::clamp(validated.maxDb, -60, 12);
    if(validated.maxDb <= validated.minDb) {
        validated.maxDb = std::min(12, validated.minDb + 12);
    }

    validated.windowFunction = normaliseWindowFunction(validated.windowFunction);
    validated.colours        = normaliseColours(validated.colours);

    m_settings->fileSet(TrackPreferenceKey, validated.trackPreference);
    m_settings->fileSet(ChannelModeKey, static_cast<int>(validated.channelMode));
    m_settings->fileSet(PresentationModeKey, static_cast<int>(validated.presentationMode));
    m_settings->fileSet(LogFrequencyKey, validated.logFrequency);
    m_settings->fileSet(ClearOnTrackChangeKey, validated.clearOnTrackChange);
    m_settings->fileSet(ShowTopLabelsKey, validated.showTopLabels);
    m_settings->fileSet(ShowBottomLabelsKey, validated.showBottomLabels);
    m_settings->fileSet(ShowLeftLabelsKey, validated.showLeftLabels);
    m_settings->fileSet(ShowRightLabelsKey, validated.showRightLabels);
    m_settings->fileSet(ShowCursorKey, validated.showCursor);
    m_settings->fileSet(CursorWidthKey, validated.cursorWidth);
    m_settings->fileSet(CursorColourKey,
                        validated.cursorColour.isValid() ? validated.cursorColour.name(QColor::HexArgb) : QString{});
    m_settings->fileSet(SeekingCursorColourKey, validated.seekingCursorColour.isValid()
                                                    ? validated.seekingCursorColour.name(QColor::HexArgb)
                                                    : QString{});
    m_settings->fileSet(PlayedOverlayColourKey, validated.playedOverlayColour.isValid()
                                                    ? validated.playedOverlayColour.name(QColor::HexArgb)
                                                    : QString{});
    m_settings->fileSet(UnplayedOverlayColourKey, validated.unplayedOverlayColour.isValid()
                                                      ? validated.unplayedOverlayColour.name(QColor::HexArgb)
                                                      : QString{});
    m_settings->fileSet(ChannelSpacingKey, validated.channelSpacing);
    m_settings->fileSet(FftSizeKey, validated.fftSize);
    m_settings->fileSet(TimeColumnsKey, validated.timeColumns);
    m_settings->fileSet(MinDbKey, validated.minDb);
    m_settings->fileSet(MaxDbKey, validated.maxDb);
    m_settings->fileSet(WindowFunctionKey, validated.windowFunction);
    m_settings->fileSet(ColoursKey, serialiseColours(validated.colours));
}

void SpectrogramWidget::clearSavedDefaults() const
{
    m_settings->fileRemove(TrackPreferenceKey);
    m_settings->fileRemove(ChannelModeKey);
    m_settings->fileRemove(PresentationModeKey);
    m_settings->fileRemove(LogFrequencyKey);
    m_settings->fileRemove(ClearOnTrackChangeKey);
    m_settings->fileRemove(ShowTopLabelsKey);
    m_settings->fileRemove(ShowBottomLabelsKey);
    m_settings->fileRemove(ShowLeftLabelsKey);
    m_settings->fileRemove(ShowRightLabelsKey);
    m_settings->fileRemove(ShowCursorKey);
    m_settings->fileRemove(CursorWidthKey);
    m_settings->fileRemove(CursorColourKey);
    m_settings->fileRemove(SeekingCursorColourKey);
    m_settings->fileRemove(PlayedOverlayColourKey);
    m_settings->fileRemove(UnplayedOverlayColourKey);
    m_settings->fileRemove(ChannelSpacingKey);
    m_settings->fileRemove(FftSizeKey);
    m_settings->fileRemove(TimeColumnsKey);
    m_settings->fileRemove(MinDbKey);
    m_settings->fileRemove(MaxDbKey);
    m_settings->fileRemove(WindowFunctionKey);
    m_settings->fileRemove(ColoursKey);
}

void SpectrogramWidget::applyConfig(const SpectrogramConfig& config)
{
    SpectrogramConfig validated{config};

    validated.trackPreference
        = std::clamp(validated.trackPreference, static_cast<int>(TrackDisplayPreference::PlayingTrack),
                     static_cast<int>(TrackDisplayPreference::PlayingTrackBlankWhenStopped));
    validated.channelMode      = normaliseChannelMode(validated.channelMode);
    validated.presentationMode = normalisePresentationMode(validated.presentationMode);
    validated.channelSpacing   = normaliseChannelSpacing(validated.channelSpacing);
    validated.fftSize          = normaliseFftSize(validated.fftSize);
    validated.timeColumns      = std::clamp(validated.timeColumns, MinTimeColumns, MaxTimeColumns);
    validated.cursorWidth      = std::clamp(validated.cursorWidth, 1, 20);

    validated.minDb = std::clamp(validated.minDb, -120, 0);
    validated.maxDb = std::clamp(validated.maxDb, -60, 12);
    if(validated.maxDb <= validated.minDb) {
        validated.maxDb = std::min(12, validated.minDb + 12);
    }

    validated.windowFunction = normaliseWindowFunction(validated.windowFunction);
    validated.colours        = normaliseColours(validated.colours);

    const bool fullTrack    = validated.presentationMode == PresentationMode::FullTrack;
    const bool wasFullTrack = m_config.presentationMode == PresentationMode::FullTrack;
    if(m_view && fullTrack != wasFullTrack) {
        delete m_view;
    }

    m_config = std::move(validated);

    if(!m_view) {
        if(fullTrack) {
            m_view = new SpectrogramSeekBar(m_audioLoader, m_playerController, m_trackSelection, m_settings, this);
        }
        else {
            m_view = new LiveSpectrogramView(m_playerController, m_engine, this);
        }
        m_layout->addWidget(m_view);
    }
    m_view->applyConfig(m_config);

    Q_EMIT configChanged();
}

QSize SpectrogramWidget::minimumSizeHint() const
{
    return {120, 40};
}

QSize SpectrogramWidget::sizeHint() const
{
    return {320, 120};
}

void SpectrogramWidget::contextMenuEvent(QContextMenuEvent* event)
{
    const bool wasSeeking = m_view && m_view->cancelSeeking();
    if(wasSeeking) {
        event->accept();
        return;
    }

    if(event->reason() == QContextMenuEvent::Mouse && QApplication::mouseButtons().testFlag(Qt::LeftButton)) {
        event->accept();
        return;
    }

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    if(auto* seekbar = qobject_cast<SpectrogramSeekBar*>(m_view)) {
        seekbar->repeatControls()->populateMenu(menu, m_actionManager, seekbar->mapFromGlobal(event->globalPos()));
        menu->addSeparator();
    }

    const auto addToggle = [this](QMenu* target, const QString& text, bool SpectrogramConfig::* option) {
        auto* action = target->addAction(text);
        action->setCheckable(true);
        action->setChecked(m_config.*option);
        QObject::connect(action, &QAction::toggled, this, [this, option](bool checked) {
            SpectrogramConfig config{m_config};
            config.*option = checked;
            applyConfig(config);
        });
    };

    addToggle(menu, tr("Use logarithmic frequency scale"), &SpectrogramConfig::logFrequency);

    if(m_config.presentationMode == PresentationMode::FullTrack) {
        addToggle(menu, tr("Show cursor"), &SpectrogramConfig::showCursor);
        auto* labelsMenu = menu->addMenu(tr("Labels"));
        addToggle(labelsMenu, tr("Top"), &SpectrogramConfig::showTopLabels);
        addToggle(labelsMenu, tr("Bottom"), &SpectrogramConfig::showBottomLabels);
        addToggle(labelsMenu, tr("Left"), &SpectrogramConfig::showLeftLabels);
        addToggle(labelsMenu, tr("Right"), &SpectrogramConfig::showRightLabels);
    }
    else {
        addToggle(menu, tr("Clear on track change"), &SpectrogramConfig::clearOnTrackChange);
    }

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
            SpectrogramConfig config{m_config};
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
            SpectrogramConfig config{m_config};
            config.presentationMode = mode;
            applyConfig(config);
        });
    };
    addPresentationMode(tr("Scrolling"), PresentationMode::Scrolling);
    addPresentationMode(tr("Stationary"), PresentationMode::Stationary);
    addPresentationMode(tr("Full Track (Seekbar)"), PresentationMode::FullTrack);

    auto* fftSizeMenu  = menu->addMenu(tr("FFT size"));
    auto* fftSizeGroup = new QActionGroup(fftSizeMenu);
    fftSizeGroup->setExclusive(true);
    for(const int fftSize : FftSizes) {
        auto* action = fftSizeMenu->addAction(QString::number(fftSize));
        action->setActionGroup(fftSizeGroup);
        action->setCheckable(true);
        action->setChecked(m_config.fftSize == fftSize);
        QObject::connect(action, &QAction::triggered, this, [this, fftSize]() {
            SpectrogramConfig config{m_config};
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
    showConfigDialog(new SpectrogramConfigDialog(this, this), Qt::NonModal);
}

SpectrogramConfig SpectrogramWidget::configFromLayout(const QJsonObject& layout) const
{
    SpectrogramConfig config{defaultConfig()};

    config.trackPreference = layout.value("TrackPreference"_L1).toInt(config.trackPreference);

    config.channelMode
        = static_cast<ChannelMode>(layout.value("ChannelMode"_L1).toInt(static_cast<int>(config.channelMode)));
    config.presentationMode = static_cast<PresentationMode>(
        layout.value("PresentationMode"_L1).toInt(static_cast<int>(config.presentationMode)));

    config.logFrequency       = layout.value("LogFrequency"_L1).toBool(config.logFrequency);
    config.clearOnTrackChange = layout.value("ClearOnTrackChange"_L1).toBool(config.clearOnTrackChange);
    config.showTopLabels      = layout.value("ShowTopLabels"_L1).toBool(config.showTopLabels);
    config.showBottomLabels   = layout.value("ShowBottomLabels"_L1).toBool(config.showBottomLabels);
    config.showLeftLabels     = layout.value("ShowLeftLabels"_L1).toBool(config.showLeftLabels);
    config.showRightLabels    = layout.value("ShowRightLabels"_L1).toBool(config.showRightLabels);
    config.showCursor         = layout.value("ShowCursor"_L1).toBool(config.showCursor);
    config.cursorWidth        = layout.value("CursorWidth"_L1).toInt(config.cursorWidth);

    if(layout.contains("CursorColour"_L1)) {
        config.cursorColour = QColor{layout.value("CursorColour"_L1).toString()};
    }
    if(layout.contains("SeekingCursorColour"_L1)) {
        config.seekingCursorColour = QColor{layout.value("SeekingCursorColour"_L1).toString()};
    }
    if(layout.contains("PlayedOverlayColour"_L1)) {
        config.playedOverlayColour = QColor{layout.value("PlayedOverlayColour"_L1).toString()};
    }
    if(layout.contains("UnplayedOverlayColour"_L1)) {
        config.unplayedOverlayColour = QColor{layout.value("UnplayedOverlayColour"_L1).toString()};
    }

    config.channelSpacing = layout.value("ChannelSpacing"_L1).toInt(config.channelSpacing);
    config.fftSize        = layout.value("FftSize"_L1).toInt(config.fftSize);
    config.timeColumns    = layout.value("TimeColumns"_L1).toInt(config.timeColumns);
    config.minDb          = layout.value("MinDb"_L1).toInt(config.minDb);
    config.maxDb          = layout.value("MaxDb"_L1).toInt(config.maxDb);
    config.windowFunction = layout.value("WindowFunction"_L1).toInt(config.windowFunction);

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

void SpectrogramWidget::saveConfigToLayout(const SpectrogramConfig& config, QJsonObject& layout) const
{
    layout["TrackPreference"_L1]    = config.trackPreference;
    layout["ChannelMode"_L1]        = static_cast<int>(config.channelMode);
    layout["PresentationMode"_L1]   = static_cast<int>(config.presentationMode);
    layout["LogFrequency"_L1]       = config.logFrequency;
    layout["ClearOnTrackChange"_L1] = config.clearOnTrackChange;
    layout["ShowTopLabels"_L1]      = config.showTopLabels;
    layout["ShowBottomLabels"_L1]   = config.showBottomLabels;
    layout["ShowLeftLabels"_L1]     = config.showLeftLabels;
    layout["ShowRightLabels"_L1]    = config.showRightLabels;
    layout["ShowCursor"_L1]         = config.showCursor;
    layout["CursorWidth"_L1]        = config.cursorWidth;
    layout["CursorColour"_L1] = config.cursorColour.isValid() ? config.cursorColour.name(QColor::HexArgb) : QString{};
    layout["SeekingCursorColour"_L1]
        = config.seekingCursorColour.isValid() ? config.seekingCursorColour.name(QColor::HexArgb) : QString{};
    layout["PlayedOverlayColour"_L1]
        = config.playedOverlayColour.isValid() ? config.playedOverlayColour.name(QColor::HexArgb) : QString{};
    layout["UnplayedOverlayColour"_L1]
        = config.unplayedOverlayColour.isValid() ? config.unplayedOverlayColour.name(QColor::HexArgb) : QString{};
    layout["ChannelSpacing"_L1] = config.channelSpacing;
    layout["FftSize"_L1]        = config.fftSize;
    layout["TimeColumns"_L1]    = config.timeColumns;
    layout["MinDb"_L1]          = config.minDb;
    layout["MaxDb"_L1]          = config.maxDb;
    layout["WindowFunction"_L1] = config.windowFunction;

    QJsonArray colours;
    const auto configColours = serialiseColours(config.colours);
    for(const QString& colour : configColours) {
        colours.push_back(colour);
    }
    layout["Colours"_L1] = colours;
}

} // namespace Fooyin::Spectrogram
