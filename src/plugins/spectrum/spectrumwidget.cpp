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

#include "spectrumwidget.h"

#include "spectrumcolours.h"
#include "spectrumconfigwidget.h"
#include "spectrumview.h"

#include <gui/guisettings.h>
#include <utils/settings/settingsmanager.h>

#include <QAction>
#include <QActionGroup>
#include <QColor>
#include <QContextMenuEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QMenu>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

constexpr auto BandCountKey           = u"Spectrum/BandCount";
constexpr auto BarSpacingKey          = u"Spectrum/BarSpacing";
constexpr auto LegacyBarWidthKey      = u"Spectrum/BarWidth";
constexpr auto LegacyBarAlignmentKey  = u"Spectrum/BarAlignment";
constexpr auto BarSectionsKey         = u"Spectrum/BarSections";
constexpr auto SectionSpacingKey      = u"Spectrum/SectionSpacing";
constexpr auto MinFrequencyKey        = u"Spectrum/MinFrequencyHz";
constexpr auto MaxFrequencyKey        = u"Spectrum/MaxFrequencyHz";
constexpr auto MinNoteKey             = u"Spectrum/MinNote";
constexpr auto MaxNoteKey             = u"Spectrum/MaxNote";
constexpr auto PitchHzKey             = u"Spectrum/PitchHz";
constexpr auto TransposeKey           = u"Spectrum/Transpose";
constexpr auto AmplitudeMinDbKey      = u"Spectrum/AmplitudeMinDb";
constexpr auto AmplitudeMaxDbKey      = u"Spectrum/AmplitudeMaxDb";
constexpr auto ChannelModeKey         = u"Spectrum/ChannelMode";
constexpr auto AmplitudesEnabledKey   = u"Spectrum/AmplitudesEnabled";
constexpr auto AmplitudeHoldTimeKey   = u"Spectrum/AmplitudeHoldTimeMs";
constexpr auto AmplitudeGravityKey    = u"Spectrum/AmplitudeGravity";
constexpr auto PeaksEnabledKey        = u"Spectrum/PeaksEnabled";
constexpr auto PeakHoldTimeKey        = u"Spectrum/PeakHoldTimeMs";
constexpr auto PeakGravityKey         = u"Spectrum/PeakGravity";
constexpr auto UpdateFpsKey           = u"Spectrum/UpdateFps";
constexpr auto FftSizeKey             = u"Spectrum/FftSize";
constexpr auto WindowFunctionKey      = u"Spectrum/WindowFunction";
constexpr auto GradientOrientationKey = u"Spectrum/GradientOrientation";
constexpr auto LabelModeKey           = u"Spectrum/LabelMode";
constexpr auto DrawStyleKey           = u"Spectrum/DrawStyle";
constexpr auto ShowTopLabelsKey       = u"Spectrum/ShowTopLabels";
constexpr auto ShowBottomLabelsKey    = u"Spectrum/ShowBottomLabels";
constexpr auto ShowLeftLabelsKey      = u"Spectrum/ShowLeftLabels";
constexpr auto ShowRightLabelsKey     = u"Spectrum/ShowRightLabels";
constexpr auto ShowHorizontalGridKey  = u"Spectrum/ShowHorizontalGrid";
constexpr auto ShowVerticalGridKey    = u"Spectrum/ShowVerticalGrid";
constexpr auto ShowWhiteKeysKey       = u"Spectrum/ShowWhiteKeys";
constexpr auto ShowBlackKeysKey       = u"Spectrum/ShowBlackKeys";
constexpr auto ShowTooltipKey         = u"Spectrum/ShowTooltip";
constexpr auto FillSpectrumKey        = u"Spectrum/FillSpectrum";
constexpr auto InterpolateKey         = u"Spectrum/Interpolate";
constexpr auto AxisFontKey            = u"Spectrum/AxisFont";
constexpr auto ColoursKey             = u"Spectrum/Colours";

namespace Fooyin::Spectrum {
namespace {
int normaliseFftSize(int fftSize)
{
    const int normalized = std::clamp(fftSize, MinFftSize, MaxFftSize);
    if((normalized & (normalized - 1)) == 0) {
        return normalized;
    }

    int power = MinFftSize;
    while(power < normalized && power < MaxFftSize) {
        power <<= 1;
    }
    return std::clamp(power, MinFftSize, MaxFftSize);
}
} // namespace

SpectrumWidget::SpectrumWidget(EngineController* engine, PlayerController* playerController, SettingsManager* settings,
                               QWidget* parent)
    : FyWidget{parent}
    , m_settings{settings}
    , m_view{new SpectrumView(engine, playerController, this)}
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_view);

    QObject::connect(m_view, &SpectrumView::contextMenuRequested, this, &SpectrumWidget::showContextMenu);

    m_config = defaultConfig();
    applyConfig(m_config);

    const auto updateThemeColours = [this]() {
        m_view->refreshStyleColours();
    };
    m_settings->subscribe<Settings::Gui::Theme>(this, updateThemeColours);
    m_settings->subscribe<Settings::Gui::Style>(this, updateThemeColours);
}

SpectrumWidget::~SpectrumWidget() = default;

QString SpectrumWidget::name() const
{
    return tr("Spectrum");
}

QString SpectrumWidget::layoutName() const
{
    return u"Spectrum"_s;
}

void SpectrumWidget::saveLayoutData(QJsonObject& layout)
{
    saveConfigToLayout(m_config, layout);
}

void SpectrumWidget::loadLayoutData(const QJsonObject& layout)
{
    applyConfig(configFromLayout(layout));
}

SpectrumWidget::ConfigData SpectrumWidget::factoryConfig() const
{
    return {};
}

SpectrumWidget::ConfigData SpectrumWidget::defaultConfig() const
{
    auto config{factoryConfig()};

    config.bandCount           = m_settings->fileValue(BandCountKey, config.bandCount).toInt();
    config.barSpacing          = m_settings->fileValue(BarSpacingKey, config.barSpacing).toInt();
    config.barSections         = m_settings->fileValue(BarSectionsKey, config.barSections).toInt();
    config.sectionSpacing      = m_settings->fileValue(SectionSpacingKey, config.sectionSpacing).toInt();
    config.minFrequencyHz      = m_settings->fileValue(MinFrequencyKey, config.minFrequencyHz).toInt();
    config.maxFrequencyHz      = m_settings->fileValue(MaxFrequencyKey, config.maxFrequencyHz).toInt();
    config.minNote             = m_settings->fileValue(MinNoteKey, config.minNote).toInt();
    config.maxNote             = m_settings->fileValue(MaxNoteKey, config.maxNote).toInt();
    config.pitchHz             = m_settings->fileValue(PitchHzKey, config.pitchHz).toInt();
    config.transpose           = m_settings->fileValue(TransposeKey, config.transpose).toInt();
    config.amplitudeMinDb      = m_settings->fileValue(AmplitudeMinDbKey, config.amplitudeMinDb).toInt();
    config.amplitudeMaxDb      = m_settings->fileValue(AmplitudeMaxDbKey, config.amplitudeMaxDb).toInt();
    config.amplitudesEnabled   = m_settings->fileValue(AmplitudesEnabledKey, config.amplitudesEnabled).toBool();
    config.amplitudeHoldTimeMs = m_settings->fileValue(AmplitudeHoldTimeKey, config.amplitudeHoldTimeMs).toInt();
    config.amplitudeGravity    = m_settings->fileValue(AmplitudeGravityKey, config.amplitudeGravity).toInt();
    config.peaksEnabled        = m_settings->fileValue(PeaksEnabledKey, config.peaksEnabled).toBool();
    config.peakHoldTimeMs      = m_settings->fileValue(PeakHoldTimeKey, config.peakHoldTimeMs).toInt();
    config.peakGravity         = m_settings->fileValue(PeakGravityKey, config.peakGravity).toInt();
    config.updateFps           = m_settings->fileValue(UpdateFpsKey, config.updateFps).toInt();
    config.fftSize             = m_settings->fileValue(FftSizeKey, config.fftSize).toInt();
    config.windowFunction      = static_cast<WindowFunction>(
        m_settings->fileValue(WindowFunctionKey, static_cast<int>(config.windowFunction)).toInt());
    config.gradientOrientation = static_cast<GradientOrientation>(
        m_settings->fileValue(GradientOrientationKey, static_cast<int>(config.gradientOrientation)).toInt());
    config.labelMode
        = static_cast<LabelMode>(m_settings->fileValue(LabelModeKey, static_cast<int>(config.labelMode)).toInt());
    config.drawStyle
        = static_cast<DrawStyle>(m_settings->fileValue(DrawStyleKey, static_cast<int>(config.drawStyle)).toInt());
    config.showTopLabels      = m_settings->fileValue(ShowTopLabelsKey, config.showTopLabels).toBool();
    config.showBottomLabels   = m_settings->fileValue(ShowBottomLabelsKey, config.showBottomLabels).toBool();
    config.showLeftLabels     = m_settings->fileValue(ShowLeftLabelsKey, config.showLeftLabels).toBool();
    config.showRightLabels    = m_settings->fileValue(ShowRightLabelsKey, config.showRightLabels).toBool();
    config.showHorizontalGrid = m_settings->fileValue(ShowHorizontalGridKey, config.showHorizontalGrid).toBool();
    config.showVerticalGrid   = m_settings->fileValue(ShowVerticalGridKey, config.showVerticalGrid).toBool();
    config.showWhiteKeys      = m_settings->fileValue(ShowWhiteKeysKey, config.showWhiteKeys).toBool();
    config.showBlackKeys      = m_settings->fileValue(ShowBlackKeysKey, config.showBlackKeys).toBool();
    config.showTooltip        = m_settings->fileValue(ShowTooltipKey, config.showTooltip).toBool();
    config.fillSpectrum       = m_settings->fileValue(FillSpectrumKey, config.fillSpectrum).toBool();
    config.interpolate        = m_settings->fileValue(InterpolateKey, config.interpolate).toBool();
    config.axisFont           = m_settings->fileValue(AxisFontKey, config.axisFont).toString();
    config.colours            = m_settings->fileValue(ColoursKey, config.colours);

    return config;
}

const SpectrumWidget::ConfigData& SpectrumWidget::currentConfig() const
{
    return m_config;
}

void SpectrumWidget::saveDefaults(const ConfigData& config) const
{
    auto validated{config};
    validated.bandCount      = std::clamp(validated.bandCount, MinBandCount, MaxBandCount);
    validated.barSpacing     = std::clamp(validated.barSpacing, MinBarSpacing, MaxBarSpacing);
    validated.barSections    = std::clamp(validated.barSections, MinBarSections, MaxBarSections);
    validated.sectionSpacing = std::clamp(validated.sectionSpacing, MinSectionSpacing, MaxSectionSpacing);
    validated.minFrequencyHz = std::clamp(validated.minFrequencyHz, MinFrequencyHz, MaxFrequencyHz);
    validated.maxFrequencyHz = std::clamp(validated.maxFrequencyHz, validated.minFrequencyHz + 1, MaxFrequencyHz);
    validated.minNote        = std::clamp(validated.minNote, MinNote, MaxNote - 12);
    validated.maxNote        = std::clamp(validated.maxNote, validated.minNote + 12, MaxNote);
    validated.pitchHz        = std::clamp(validated.pitchHz, MinPitchHz, MaxPitchHz);
    validated.transpose      = std::clamp(validated.transpose, MinTranspose, MaxTranspose);
    validated.amplitudeMinDb = std::clamp(validated.amplitudeMinDb, MinAmplitudeDb, MaxAmplitudeDb);
    validated.amplitudeMaxDb = std::clamp(validated.amplitudeMaxDb, validated.amplitudeMinDb + 1, MaxAmplitudeDb);
    validated.amplitudeHoldTimeMs
        = std::clamp(validated.amplitudeHoldTimeMs, MinAmplitudeHoldTimeMs, MaxAmplitudeHoldTimeMs);
    validated.amplitudeGravity = std::clamp(validated.amplitudeGravity, MinAmplitudeGravity, MaxAmplitudeGravity);
    validated.peakHoldTimeMs   = std::clamp(validated.peakHoldTimeMs, MinPeakHoldTimeMs, MaxPeakHoldTimeMs);
    validated.peakGravity      = std::clamp(validated.peakGravity, MinPeakGravity, MaxPeakGravity);
    validated.updateFps        = std::clamp(validated.updateFps, MinUpdateFps, MaxUpdateFps);
    validated.fftSize          = normaliseFftSize(validated.fftSize);
    validated.windowFunction
        = std::clamp(validated.windowFunction, WindowFunction::BlackmanHarris, WindowFunction::None);
    validated.gradientOrientation
        = std::clamp(validated.gradientOrientation, GradientOrientation::Vertical, GradientOrientation::Horizontal);
    validated.labelMode = std::clamp(validated.labelMode, LabelMode::Frequency, LabelMode::Notes);
    validated.drawStyle = std::clamp(validated.drawStyle, DrawStyle::Bars, DrawStyle::Curve);

    if(!validated.colours.canConvert<Colours>()
       || (validated.colours.isValid() && validated.colours.value<Colours>().isEmpty())) {
        validated.colours = QVariant{};
    }

    m_settings->fileSet(BandCountKey, validated.bandCount);
    m_settings->fileSet(BarSpacingKey, validated.barSpacing);
    m_settings->fileRemove(LegacyBarWidthKey);
    m_settings->fileRemove(LegacyBarAlignmentKey);
    m_settings->fileSet(BarSectionsKey, validated.barSections);
    m_settings->fileSet(SectionSpacingKey, validated.sectionSpacing);
    m_settings->fileSet(MinFrequencyKey, validated.minFrequencyHz);
    m_settings->fileSet(MaxFrequencyKey, validated.maxFrequencyHz);
    m_settings->fileSet(MinNoteKey, validated.minNote);
    m_settings->fileSet(MaxNoteKey, validated.maxNote);
    m_settings->fileSet(PitchHzKey, validated.pitchHz);
    m_settings->fileSet(TransposeKey, validated.transpose);
    m_settings->fileSet(AmplitudeMinDbKey, validated.amplitudeMinDb);
    m_settings->fileSet(AmplitudeMaxDbKey, validated.amplitudeMaxDb);
    m_settings->fileSet(AmplitudesEnabledKey, validated.amplitudesEnabled);
    m_settings->fileSet(AmplitudeHoldTimeKey, validated.amplitudeHoldTimeMs);
    m_settings->fileSet(AmplitudeGravityKey, validated.amplitudeGravity);
    m_settings->fileSet(PeaksEnabledKey, validated.peaksEnabled);
    m_settings->fileSet(PeakHoldTimeKey, validated.peakHoldTimeMs);
    m_settings->fileSet(PeakGravityKey, validated.peakGravity);
    m_settings->fileSet(UpdateFpsKey, validated.updateFps);
    m_settings->fileSet(FftSizeKey, validated.fftSize);
    m_settings->fileSet(WindowFunctionKey, static_cast<int>(validated.windowFunction));
    m_settings->fileSet(GradientOrientationKey, static_cast<int>(validated.gradientOrientation));
    m_settings->fileSet(LabelModeKey, static_cast<int>(validated.labelMode));
    m_settings->fileSet(DrawStyleKey, static_cast<int>(validated.drawStyle));
    m_settings->fileSet(ShowTopLabelsKey, validated.showTopLabels);
    m_settings->fileSet(ShowBottomLabelsKey, validated.showBottomLabels);
    m_settings->fileSet(ShowLeftLabelsKey, validated.showLeftLabels);
    m_settings->fileSet(ShowRightLabelsKey, validated.showRightLabels);
    m_settings->fileSet(ShowHorizontalGridKey, validated.showHorizontalGrid);
    m_settings->fileSet(ShowVerticalGridKey, validated.showVerticalGrid);
    m_settings->fileSet(ShowWhiteKeysKey, validated.showWhiteKeys);
    m_settings->fileSet(ShowBlackKeysKey, validated.showBlackKeys);
    m_settings->fileSet(ShowTooltipKey, validated.showTooltip);
    m_settings->fileSet(FillSpectrumKey, validated.fillSpectrum);
    m_settings->fileSet(InterpolateKey, validated.interpolate);
    m_settings->fileSet(AxisFontKey, validated.axisFont);
    m_settings->fileSet(ColoursKey, validated.colours);
}

void SpectrumWidget::clearSavedDefaults() const
{
    m_settings->fileRemove(BandCountKey);
    m_settings->fileRemove(BarSpacingKey);
    m_settings->fileRemove(LegacyBarWidthKey);
    m_settings->fileRemove(LegacyBarAlignmentKey);
    m_settings->fileRemove(BarSectionsKey);
    m_settings->fileRemove(SectionSpacingKey);
    m_settings->fileRemove(MinFrequencyKey);
    m_settings->fileRemove(MaxFrequencyKey);
    m_settings->fileRemove(MinNoteKey);
    m_settings->fileRemove(MaxNoteKey);
    m_settings->fileRemove(PitchHzKey);
    m_settings->fileRemove(TransposeKey);
    m_settings->fileRemove(AmplitudeMinDbKey);
    m_settings->fileRemove(AmplitudeMaxDbKey);
    m_settings->fileRemove(ChannelModeKey);
    m_settings->fileRemove(AmplitudesEnabledKey);
    m_settings->fileRemove(AmplitudeHoldTimeKey);
    m_settings->fileRemove(AmplitudeGravityKey);
    m_settings->fileRemove(PeaksEnabledKey);
    m_settings->fileRemove(PeakHoldTimeKey);
    m_settings->fileRemove(PeakGravityKey);
    m_settings->fileRemove(UpdateFpsKey);
    m_settings->fileRemove(FftSizeKey);
    m_settings->fileRemove(WindowFunctionKey);
    m_settings->fileRemove(GradientOrientationKey);
    m_settings->fileRemove(LabelModeKey);
    m_settings->fileRemove(DrawStyleKey);
    m_settings->fileRemove(ShowTopLabelsKey);
    m_settings->fileRemove(ShowBottomLabelsKey);
    m_settings->fileRemove(ShowLeftLabelsKey);
    m_settings->fileRemove(ShowRightLabelsKey);
    m_settings->fileRemove(ShowHorizontalGridKey);
    m_settings->fileRemove(ShowVerticalGridKey);
    m_settings->fileRemove(ShowWhiteKeysKey);
    m_settings->fileRemove(ShowBlackKeysKey);
    m_settings->fileRemove(ShowTooltipKey);
    m_settings->fileRemove(FillSpectrumKey);
    m_settings->fileRemove(InterpolateKey);
    m_settings->fileRemove(AxisFontKey);
    m_settings->fileRemove(ColoursKey);
}

void SpectrumWidget::applyConfig(const ConfigData& config)
{
    auto validated{config};
    validated.bandCount      = std::clamp(validated.bandCount, MinBandCount, MaxBandCount);
    validated.barSpacing     = std::clamp(validated.barSpacing, MinBarSpacing, MaxBarSpacing);
    validated.barSections    = std::clamp(validated.barSections, MinBarSections, MaxBarSections);
    validated.sectionSpacing = std::clamp(validated.sectionSpacing, MinSectionSpacing, MaxSectionSpacing);
    validated.minFrequencyHz = std::clamp(validated.minFrequencyHz, MinFrequencyHz, MaxFrequencyHz);
    validated.maxFrequencyHz = std::clamp(validated.maxFrequencyHz, validated.minFrequencyHz + 1, MaxFrequencyHz);
    validated.minNote        = std::clamp(validated.minNote, MinNote, MaxNote - 12);
    validated.maxNote        = std::clamp(validated.maxNote, validated.minNote + 12, MaxNote);
    validated.pitchHz        = std::clamp(validated.pitchHz, MinPitchHz, MaxPitchHz);
    validated.transpose      = std::clamp(validated.transpose, MinTranspose, MaxTranspose);
    validated.amplitudeMinDb = std::clamp(validated.amplitudeMinDb, MinAmplitudeDb, MaxAmplitudeDb);
    validated.amplitudeMaxDb = std::clamp(validated.amplitudeMaxDb, validated.amplitudeMinDb + 1, MaxAmplitudeDb);
    validated.amplitudeHoldTimeMs
        = std::clamp(validated.amplitudeHoldTimeMs, MinAmplitudeHoldTimeMs, MaxAmplitudeHoldTimeMs);
    validated.amplitudeGravity = std::clamp(validated.amplitudeGravity, MinAmplitudeGravity, MaxAmplitudeGravity);
    validated.peakHoldTimeMs   = std::clamp(validated.peakHoldTimeMs, MinPeakHoldTimeMs, MaxPeakHoldTimeMs);
    validated.peakGravity      = std::clamp(validated.peakGravity, MinPeakGravity, MaxPeakGravity);
    validated.updateFps        = std::clamp(validated.updateFps, MinUpdateFps, MaxUpdateFps);
    validated.fftSize          = normaliseFftSize(validated.fftSize);
    validated.windowFunction
        = std::clamp(validated.windowFunction, WindowFunction::BlackmanHarris, WindowFunction::None);
    validated.gradientOrientation
        = std::clamp(validated.gradientOrientation, GradientOrientation::Vertical, GradientOrientation::Horizontal);
    validated.labelMode = std::clamp(validated.labelMode, LabelMode::Frequency, LabelMode::Notes);
    validated.drawStyle = std::clamp(validated.drawStyle, DrawStyle::Bars, DrawStyle::Curve);

    if(!validated.colours.canConvert<Colours>()
       || (validated.colours.isValid() && validated.colours.value<Colours>().isEmpty())) {
        validated.colours = QVariant{};
    }

    m_config = validated;
    m_view->setConfig(m_config);
}

QSize SpectrumWidget::minimumSizeHint() const
{
    return {32, 24};
}

void SpectrumWidget::contextMenuEvent(QContextMenuEvent* event)
{
    showContextMenu(event->globalPos());
}

void SpectrumWidget::openConfigDialog()
{
    showConfigDialog(new SpectrumConfigDialog(this, this));
}

SpectrumWidget::ConfigData SpectrumWidget::configFromLayout(const QJsonObject& layout) const
{
    ConfigData config{defaultConfig()};

    if(layout.contains("BandCount"_L1)) {
        config.bandCount = layout.value("BandCount"_L1).toInt();
    }
    if(layout.contains("BarSpacing"_L1)) {
        config.barSpacing = layout.value("BarSpacing"_L1).toInt();
    }
    if(layout.contains("BarSections"_L1)) {
        config.barSections = layout.value("BarSections"_L1).toInt();
    }
    if(layout.contains("SectionSpacing"_L1)) {
        config.sectionSpacing = layout.value("SectionSpacing"_L1).toInt();
    }
    if(layout.contains("MinFrequencyHz"_L1)) {
        config.minFrequencyHz = layout.value("MinFrequencyHz"_L1).toInt();
    }
    if(layout.contains("MaxFrequencyHz"_L1)) {
        config.maxFrequencyHz = layout.value("MaxFrequencyHz"_L1).toInt();
    }
    if(layout.contains("MinNote"_L1)) {
        config.minNote = layout.value("MinNote"_L1).toInt();
    }
    if(layout.contains("MaxNote"_L1)) {
        config.maxNote = layout.value("MaxNote"_L1).toInt();
    }
    if(layout.contains("PitchHz"_L1)) {
        config.pitchHz = layout.value("PitchHz"_L1).toInt();
    }
    if(layout.contains("Transpose"_L1)) {
        config.transpose = layout.value("Transpose"_L1).toInt();
    }
    if(layout.contains("AmplitudeMinDb"_L1)) {
        config.amplitudeMinDb = layout.value("AmplitudeMinDb"_L1).toInt();
    }
    if(layout.contains("AmplitudeMaxDb"_L1)) {
        config.amplitudeMaxDb = layout.value("AmplitudeMaxDb"_L1).toInt();
    }
    if(layout.contains("AmplitudesEnabled"_L1)) {
        config.amplitudesEnabled = layout.value("AmplitudesEnabled"_L1).toBool();
    }
    if(layout.contains("AmplitudeHoldTimeMs"_L1)) {
        config.amplitudeHoldTimeMs = layout.value("AmplitudeHoldTimeMs"_L1).toInt();
    }
    if(layout.contains("AmplitudeGravity"_L1)) {
        config.amplitudeGravity = layout.value("AmplitudeGravity"_L1).toInt();
    }
    if(layout.contains("PeaksEnabled"_L1)) {
        config.peaksEnabled = layout.value("PeaksEnabled"_L1).toBool();
    }
    if(layout.contains("PeakHoldTimeMs"_L1)) {
        config.peakHoldTimeMs = layout.value("PeakHoldTimeMs"_L1).toInt();
    }
    if(layout.contains("PeakGravity"_L1)) {
        config.peakGravity = layout.value("PeakGravity"_L1).toInt();
    }
    if(layout.contains("UpdateFps"_L1)) {
        config.updateFps = layout.value("UpdateFps"_L1).toInt();
    }
    if(layout.contains("FftSize"_L1)) {
        config.fftSize = layout.value("FftSize"_L1).toInt();
    }
    if(layout.contains("WindowFunction"_L1)) {
        config.windowFunction = static_cast<WindowFunction>(layout.value("WindowFunction"_L1).toInt());
    }
    if(layout.contains("GradientOrientation"_L1)) {
        config.gradientOrientation = static_cast<GradientOrientation>(layout.value("GradientOrientation"_L1).toInt());
    }
    if(layout.contains("LabelMode"_L1)) {
        config.labelMode = static_cast<LabelMode>(layout.value("LabelMode"_L1).toInt());
    }
    if(layout.contains("DrawStyle"_L1)) {
        config.drawStyle = static_cast<DrawStyle>(layout.value("DrawStyle"_L1).toInt());
    }
    if(layout.contains("ShowTopLabels"_L1)) {
        config.showTopLabels = layout.value("ShowTopLabels"_L1).toBool();
    }
    if(layout.contains("ShowBottomLabels"_L1)) {
        config.showBottomLabels = layout.value("ShowBottomLabels"_L1).toBool();
    }
    if(layout.contains("ShowLeftLabels"_L1)) {
        config.showLeftLabels = layout.value("ShowLeftLabels"_L1).toBool();
    }
    if(layout.contains("ShowRightLabels"_L1)) {
        config.showRightLabels = layout.value("ShowRightLabels"_L1).toBool();
    }
    if(layout.contains("ShowHorizontalGrid"_L1)) {
        config.showHorizontalGrid = layout.value("ShowHorizontalGrid"_L1).toBool();
    }
    if(layout.contains("ShowVerticalGrid"_L1)) {
        config.showVerticalGrid = layout.value("ShowVerticalGrid"_L1).toBool();
    }
    if(layout.contains("ShowWhiteKeys"_L1)) {
        config.showWhiteKeys = layout.value("ShowWhiteKeys"_L1).toBool();
    }
    if(layout.contains("ShowBlackKeys"_L1)) {
        config.showBlackKeys = layout.value("ShowBlackKeys"_L1).toBool();
    }
    if(layout.contains("ShowTooltip"_L1)) {
        config.showTooltip = layout.value("ShowTooltip"_L1).toBool();
    }
    if(layout.contains("FillSpectrum"_L1)) {
        config.fillSpectrum = layout.value("FillSpectrum"_L1).toBool();
    }
    if(layout.contains("Interpolate"_L1)) {
        config.interpolate = layout.value("Interpolate"_L1).toBool();
    }
    if(layout.contains("AxisFont"_L1)) {
        config.axisFont = layout.value("AxisFont"_L1).toString();
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
            setColour(u"PeaksColour"_s, Colours::Type::Peaks);
            setColour(u"TextColour"_s, Colours::Type::Text);
            setColour(u"HorizontalGridColour"_s, Colours::Type::HorizontalGrid);
            setColour(u"VerticalGridColour"_s, Colours::Type::VerticalGrid);
            setColour(u"OctaveGridColour"_s, Colours::Type::OctaveGrid);
            setColour(u"WhiteKeyColour"_s, Colours::Type::WhiteKey);
            setColour(u"BlackKeyColour"_s, Colours::Type::BlackKey);

            if(const QJsonValue value = layout.value("BarGradientColours"_L1); value.isArray()) {
                std::vector<QColor> gradient;
                const QJsonArray array = value.toArray();
                for(const auto& colourValue : array) {
                    const QColor colour{colourValue.toString()};
                    if(colour.isValid()) {
                        gradient.push_back(colour);
                    }
                }
                colours.setGradient(gradient);
            }
            else {
                std::vector<QColor> gradient;
                const QColor topColour{layout.value("BarTopColour"_L1).toString()};
                const QColor bottomColour{layout.value("BarBottomColour"_L1).toString()};
                if(topColour.isValid()) {
                    gradient.push_back(topColour);
                }
                if(bottomColour.isValid()) {
                    gradient.push_back(bottomColour);
                }
                colours.setGradient(gradient);
            }

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

void SpectrumWidget::saveConfigToLayout(const ConfigData& config, QJsonObject& layout) const
{
    layout["BandCount"_L1]           = config.bandCount;
    layout["BarSpacing"_L1]          = config.barSpacing;
    layout["BarSections"_L1]         = config.barSections;
    layout["SectionSpacing"_L1]      = config.sectionSpacing;
    layout["MinFrequencyHz"_L1]      = config.minFrequencyHz;
    layout["MaxFrequencyHz"_L1]      = config.maxFrequencyHz;
    layout["MinNote"_L1]             = config.minNote;
    layout["MaxNote"_L1]             = config.maxNote;
    layout["PitchHz"_L1]             = config.pitchHz;
    layout["Transpose"_L1]           = config.transpose;
    layout["AmplitudeMinDb"_L1]      = config.amplitudeMinDb;
    layout["AmplitudeMaxDb"_L1]      = config.amplitudeMaxDb;
    layout["AmplitudesEnabled"_L1]   = config.amplitudesEnabled;
    layout["AmplitudeHoldTimeMs"_L1] = config.amplitudeHoldTimeMs;
    layout["AmplitudeGravity"_L1]    = config.amplitudeGravity;
    layout["PeaksEnabled"_L1]        = config.peaksEnabled;
    layout["PeakHoldTimeMs"_L1]      = config.peakHoldTimeMs;
    layout["PeakGravity"_L1]         = config.peakGravity;
    layout["UpdateFps"_L1]           = config.updateFps;
    layout["FftSize"_L1]             = config.fftSize;
    layout["WindowFunction"_L1]      = static_cast<int>(config.windowFunction);
    layout["GradientOrientation"_L1] = static_cast<int>(config.gradientOrientation);
    layout["LabelMode"_L1]           = static_cast<int>(config.labelMode);
    layout["DrawStyle"_L1]           = static_cast<int>(config.drawStyle);
    layout["ShowTopLabels"_L1]       = config.showTopLabels;
    layout["ShowBottomLabels"_L1]    = config.showBottomLabels;
    layout["ShowLeftLabels"_L1]      = config.showLeftLabels;
    layout["ShowRightLabels"_L1]     = config.showRightLabels;
    layout["ShowHorizontalGrid"_L1]  = config.showHorizontalGrid;
    layout["ShowVerticalGrid"_L1]    = config.showVerticalGrid;
    layout["ShowWhiteKeys"_L1]       = config.showWhiteKeys;
    layout["ShowBlackKeys"_L1]       = config.showBlackKeys;
    layout["ShowTooltip"_L1]         = config.showTooltip;
    layout["FillSpectrum"_L1]        = config.fillSpectrum;
    layout["Interpolate"_L1]         = config.interpolate;
    layout["AxisFont"_L1]            = config.axisFont;

    const bool customColours      = config.colours.isValid() && config.colours.canConvert<Colours>()
                                 && !config.colours.value<Colours>().isEmpty();
    layout["UseCustomColours"_L1] = customColours;

    if(!customColours) {
        return;
    }

    const auto colours    = config.colours.value<Colours>();
    const auto saveColour = [&layout, &colours](const QString& key, Colours::Type type) {
        const QColor colour = colours.customColour(type);
        if(colour.isValid()) {
            layout[key] = colour.name(QColor::HexArgb);
        }
        else {
            layout.remove(key);
        }
    };

    saveColour(u"BackgroundColour"_s, Colours::Type::Background);
    saveColour(u"PeaksColour"_s, Colours::Type::Peaks);
    saveColour(u"TextColour"_s, Colours::Type::Text);
    saveColour(u"HorizontalGridColour"_s, Colours::Type::HorizontalGrid);
    saveColour(u"VerticalGridColour"_s, Colours::Type::VerticalGrid);
    saveColour(u"OctaveGridColour"_s, Colours::Type::OctaveGrid);
    saveColour(u"WhiteKeyColour"_s, Colours::Type::WhiteKey);
    saveColour(u"BlackKeyColour"_s, Colours::Type::BlackKey);

    const std::vector<QColor>& customGradient = colours.customGradient();
    if(customGradient.empty()) {
        layout.remove("BarGradientColours"_L1);
    }
    else {
        QJsonArray gradientColours;
        for(const QColor& colour : customGradient) {
            gradientColours.append(colour.name(QColor::HexArgb));
        }
        layout["BarGradientColours"_L1] = gradientColours;
    }

    layout.remove("BarTopColour"_L1);
    layout.remove("BarBottomColour"_L1);
}

void SpectrumWidget::showContextMenu(const QPoint& globalPos)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto* styleMenu  = new QMenu(tr("Style"), menu);
    auto* styleGroup = new QActionGroup(styleMenu);

    auto* bars = new QAction(tr("Bars"), styleGroup);
    bars->setCheckable(true);
    bars->setChecked(m_config.drawStyle == DrawStyle::Bars);
    auto* curve = new QAction(tr("Curve"), styleGroup);
    curve->setCheckable(true);
    curve->setChecked(m_config.drawStyle == DrawStyle::Curve);

    QObject::connect(bars, &QAction::triggered, this, [this]() {
        auto config{m_config};
        config.drawStyle = DrawStyle::Bars;
        applyConfig(config);
    });
    QObject::connect(curve, &QAction::triggered, this, [this]() {
        auto config{m_config};
        config.drawStyle = DrawStyle::Curve;
        applyConfig(config);
    });

    styleMenu->addAction(bars);
    styleMenu->addAction(curve);

    auto* axisMenu  = new QMenu(tr("Axis"), menu);
    auto* axisGroup = new QActionGroup(axisMenu);

    auto* frequencies = new QAction(tr("Frequencies"), axisGroup);
    frequencies->setCheckable(true);
    frequencies->setChecked(m_config.labelMode == LabelMode::Frequency);
    auto* notes = new QAction(tr("Notes"), axisGroup);
    notes->setCheckable(true);
    notes->setChecked(m_config.labelMode == LabelMode::Notes);

    QObject::connect(frequencies, &QAction::triggered, this, [this]() {
        auto config{m_config};
        config.labelMode = LabelMode::Frequency;
        applyConfig(config);
    });
    QObject::connect(notes, &QAction::triggered, this, [this]() {
        auto config{m_config};
        config.labelMode = LabelMode::Notes;
        applyConfig(config);
    });

    axisMenu->addAction(frequencies);
    axisMenu->addAction(notes);

    auto* fftSizeMenu  = new QMenu(tr("FFT size"), menu);
    auto* fftSizeGroup = new QActionGroup(fftSizeMenu);

    for(int fftSize{MinFftSize}; fftSize <= MaxFftSize; fftSize <<= 1) {
        auto* action = new QAction(QString::number(fftSize), fftSizeGroup);
        action->setCheckable(true);
        action->setChecked(m_config.fftSize == fftSize);

        QObject::connect(action, &QAction::triggered, this, [this, fftSize]() {
            auto config{m_config};
            config.fftSize = fftSize;
            applyConfig(config);
        });

        fftSizeMenu->addAction(action);
    }

    auto* showPeaks = new QAction(tr("Show peaks"), menu);
    showPeaks->setCheckable(true);
    showPeaks->setChecked(m_config.peaksEnabled);
    QObject::connect(showPeaks, &QAction::triggered, this, [this](bool checked) {
        auto config{m_config};
        config.peaksEnabled = checked;
        applyConfig(config);
    });

    auto* fillSpectrum = new QAction(tr("Fill spectrum"), menu);
    fillSpectrum->setCheckable(true);
    fillSpectrum->setChecked(m_config.fillSpectrum);
    QObject::connect(fillSpectrum, &QAction::triggered, this, [this](bool checked) {
        auto config{m_config};
        config.fillSpectrum = checked;
        applyConfig(config);
    });

    auto* showTooltip = new QAction(tr("Show tooltip"), menu);
    showTooltip->setCheckable(true);
    showTooltip->setChecked(m_config.showTooltip);
    QObject::connect(showTooltip, &QAction::triggered, this, [this](bool checked) {
        auto config{m_config};
        config.showTooltip = checked;
        applyConfig(config);
    });

    auto* labelsMenu = new QMenu(tr("Labels"), menu);

    auto* topLabels = new QAction(tr("Top"), labelsMenu);
    topLabels->setCheckable(true);
    topLabels->setChecked(m_config.showTopLabels);
    QObject::connect(topLabels, &QAction::triggered, this, [this](bool checked) {
        auto config{m_config};
        config.showTopLabels = checked;
        applyConfig(config);
    });

    auto* bottomLabels = new QAction(tr("Bottom"), labelsMenu);
    bottomLabels->setCheckable(true);
    bottomLabels->setChecked(m_config.showBottomLabels);
    QObject::connect(bottomLabels, &QAction::triggered, this, [this](bool checked) {
        auto config{m_config};
        config.showBottomLabels = checked;
        applyConfig(config);
    });

    auto* leftLabels = new QAction(tr("Left"), labelsMenu);
    leftLabels->setCheckable(true);
    leftLabels->setChecked(m_config.showLeftLabels);
    QObject::connect(leftLabels, &QAction::triggered, this, [this](bool checked) {
        auto config{m_config};
        config.showLeftLabels = checked;
        applyConfig(config);
    });

    auto* rightLabels = new QAction(tr("Right"), labelsMenu);
    rightLabels->setCheckable(true);
    rightLabels->setChecked(m_config.showRightLabels);
    QObject::connect(rightLabels, &QAction::triggered, this, [this](bool checked) {
        auto config{m_config};
        config.showRightLabels = checked;
        applyConfig(config);
    });

    labelsMenu->addAction(topLabels);
    labelsMenu->addAction(bottomLabels);
    labelsMenu->addAction(leftLabels);
    labelsMenu->addAction(rightLabels);

    auto* gridMenu = new QMenu(tr("Gridlines"), menu);

    auto* horizontalGrid = new QAction(tr("Horizontal"), gridMenu);
    horizontalGrid->setCheckable(true);
    horizontalGrid->setChecked(m_config.showHorizontalGrid);
    QObject::connect(horizontalGrid, &QAction::triggered, this, [this](bool checked) {
        auto config{m_config};
        config.showHorizontalGrid = checked;
        applyConfig(config);
    });

    auto* verticalGrid = new QAction(tr("Vertical"), gridMenu);
    verticalGrid->setCheckable(true);
    verticalGrid->setChecked(m_config.showVerticalGrid);
    QObject::connect(verticalGrid, &QAction::triggered, this, [this](bool checked) {
        auto config{m_config};
        config.showVerticalGrid = checked;
        applyConfig(config);
    });

    auto* whiteKeys = new QAction(tr("White keys"), gridMenu);
    whiteKeys->setCheckable(true);
    whiteKeys->setChecked(m_config.showWhiteKeys);
    whiteKeys->setEnabled(m_config.labelMode == LabelMode::Notes);
    QObject::connect(whiteKeys, &QAction::triggered, this, [this](bool checked) {
        auto config{m_config};
        config.showWhiteKeys = checked;
        applyConfig(config);
    });

    auto* blackKeys = new QAction(tr("Black keys"), gridMenu);
    blackKeys->setCheckable(true);
    blackKeys->setChecked(m_config.showBlackKeys);
    blackKeys->setEnabled(m_config.labelMode == LabelMode::Notes);
    QObject::connect(blackKeys, &QAction::triggered, this, [this](bool checked) {
        auto config{m_config};
        config.showBlackKeys = checked;
        applyConfig(config);
    });

    gridMenu->addAction(horizontalGrid);
    gridMenu->addAction(verticalGrid);
    gridMenu->addSeparator();
    gridMenu->addAction(whiteKeys);
    gridMenu->addAction(blackKeys);

    menu->addAction(showPeaks);
    menu->addAction(fillSpectrum);
    menu->addAction(showTooltip);
    menu->addSeparator();
    menu->addMenu(labelsMenu);
    menu->addMenu(gridMenu);
    menu->addSeparator();
    menu->addMenu(styleMenu);
    menu->addMenu(axisMenu);
    menu->addMenu(fftSizeMenu);

    addConfigureAction(menu);
    menu->popup(globalPos);
}
} // namespace Fooyin::Spectrum

#include "moc_spectrumwidget.cpp"
