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

#include "vumeterconfigwidget.h"

#include "vumetercolours.h"

#include <gui/framerate.h>
#include <gui/widgets/colourbutton.h>
#include <gui/widgets/gradienteditor.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLocale>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTabWidget>

using namespace Qt::StringLiterals;

constexpr auto DefaultFps = Fooyin::Gui::FrameRate::Preset::Fps40;

namespace Fooyin::VuMeter {
VuMeterConfigDialog::VuMeterConfigDialog(VuMeter::VuMeterWidget* vuMeter, QWidget* parent)
    : WidgetConfigDialog{vuMeter, tr("VU Meter Settings"), parent}
    , m_peakHold{new QSpinBox(this)}
    , m_falloff{new QSpinBox(this)}
    , m_peakFalloff{new QSpinBox(this)}
    , m_updateFps{new QComboBox(this)}
    , m_showLegend{new QCheckBox(tr("Legend"), this)}
    , m_peaksGroup{new QGroupBox(tr("Peaks"), this)}
    , m_channelSpacing{new QSpinBox(this)}
    , m_barSize{new QSpinBox(this)}
    , m_barSpacing{new QSpinBox(this)}
    , m_barSections{new QSpinBox(this)}
    , m_sectionSpacing{new QSpinBox(this)}
    , m_colourGroup{new QGroupBox(tr("Custom colours"), this)}
    , m_bgColour{new ColourButton(this)}
    , m_peakColour{new ColourButton(this)}
    , m_legendColour{new ColourButton(this)}
    , m_barGradient{new GradientEditor(this)}
{
    m_peakHold->setRange(0, 5000);
    m_peakHold->setSingleStep(100);
    m_peakHold->setSuffix(u" ms"_s);
    m_falloff->setRange(0, 500);
    m_falloff->setSingleStep(10);
    m_falloff->setSuffix(u" dB/s"_s);
    m_peakFalloff->setRange(0, 500);
    m_peakFalloff->setSingleStep(10);
    m_peakFalloff->setSuffix(u" dB/s"_s);

    m_barGradient->setOrientationControlVisible(false);

    for(const auto preset : Gui::FrameRate::Presets) {
        const int fps = Gui::FrameRate::toFps(preset);
        m_updateFps->addItem(tr("%1 fps").arg(fps), fps);
    }
    auto* displayGroup  = new QGroupBox(tr("Display"), this);
    auto* displayLayout = new QGridLayout(displayGroup);

    int row = 0;
    displayLayout->addWidget(m_showLegend, row++, 0, 1, 2);
    displayLayout->addWidget(new QLabel(tr("Falloff") + ":"_L1, this), row, 0);
    displayLayout->addWidget(m_falloff, row++, 1);
    displayLayout->addWidget(new QLabel(tr("Refresh rate") + ":"_L1, this), row, 0);
    displayLayout->addWidget(m_updateFps, row++, 1);
    displayLayout->setColumnStretch(2, 1);

    m_peaksGroup->setCheckable(true);

    auto* peaksLayout = new QGridLayout(m_peaksGroup);

    row = 0;
    peaksLayout->addWidget(new QLabel(tr("Hold time") + ":"_L1, this), row, 0);
    peaksLayout->addWidget(m_peakHold, row++, 1);
    peaksLayout->addWidget(new QLabel(tr("Falloff") + ":"_L1, this), row, 0);
    peaksLayout->addWidget(m_peakFalloff, row++, 1);
    peaksLayout->setColumnStretch(2, 1);

    auto* dimensionGroup  = new QGroupBox(tr("Dimension"), this);
    auto* dimensionLayout = new QGridLayout(dimensionGroup);

    m_channelSpacing->setRange(0, 20);
    m_channelSpacing->setSuffix(u" px"_s);
    m_barSize->setRange(0, 50);
    m_barSize->setSuffix(u" px"_s);
    m_barSpacing->setRange(1, 20);
    m_barSpacing->setSuffix(u" px"_s);
    m_barSections->setRange(1, 20);
    m_sectionSpacing->setRange(1, 20);
    m_sectionSpacing->setSuffix(u" px"_s);

    row = 0;
    dimensionLayout->addWidget(new QLabel(tr("Channel spacing") + ":"_L1, this), row, 0);
    dimensionLayout->addWidget(m_channelSpacing, row++, 1);
    dimensionLayout->addWidget(new QLabel(tr("Bar size") + ":"_L1, this), row, 0);
    dimensionLayout->addWidget(m_barSize, row++, 1);
    dimensionLayout->addWidget(new QLabel(tr("Bar spacing") + ":"_L1, this), row, 0);
    dimensionLayout->addWidget(m_barSpacing, row++, 1);
    row = 0;
    dimensionLayout->addWidget(new QLabel(tr("Sections") + ":"_L1, this), row, 2);
    dimensionLayout->addWidget(m_barSections, row++, 3);
    dimensionLayout->addWidget(new QLabel(tr("Section spacing") + ":"_L1, this), row, 2);
    dimensionLayout->addWidget(m_sectionSpacing, row++, 3);
    dimensionLayout->setColumnStretch(4, 1);

    m_colourGroup->setCheckable(true);
    auto* coloursLayout = new QGridLayout(m_colourGroup);

    row = 0;
    coloursLayout->addWidget(new QLabel(tr("Background colour") + ":"_L1, this), row, 0);
    coloursLayout->addWidget(m_bgColour, row++, 1, 1, 2);
    coloursLayout->addWidget(new QLabel(tr("Peak colour") + ":"_L1, this), row, 0);
    coloursLayout->addWidget(m_peakColour, row++, 1, 1, 2);
    coloursLayout->addWidget(new QLabel(tr("Legend colour") + ":"_L1, this), row, 0);
    coloursLayout->addWidget(m_legendColour, row++, 1, 1, 2);
    coloursLayout->addWidget(new QLabel(tr("Bar gradient") + ":"_L1, this), row, 0, Qt::AlignTop);
    coloursLayout->addWidget(m_barGradient, row++, 1);
    coloursLayout->setColumnStretch(2, 1);

    auto* generalPage       = new QWidget(this);
    auto* generalPageLayout = new QGridLayout(generalPage);

    generalPageLayout->addWidget(displayGroup, 0, 0);
    generalPageLayout->addWidget(m_peaksGroup, 0, 1);
    generalPageLayout->addWidget(dimensionGroup, 1, 0, 1, 2);
    generalPageLayout->setRowStretch(2, 1);
    generalPageLayout->setColumnStretch(0, 1);
    generalPageLayout->setColumnStretch(1, 1);

    auto* coloursPage       = new QWidget(this);
    auto* coloursPageLayout = new QGridLayout(coloursPage);

    coloursPageLayout->addWidget(m_colourGroup, 0, 0);
    coloursPageLayout->setRowStretch(1, 1);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(generalPage, tr("General"));
    tabs->addTab(coloursPage, tr("Colours"));

    auto* layout = contentLayout();
    layout->addWidget(tabs, 0, 0);
    layout->setRowStretch(0, 1);

    loadCurrentConfig();
}

VuMeterWidget::ConfigData VuMeterConfigDialog::config() const
{
    VuMeterWidget::ConfigData config{
        .peakHoldTimeMs  = m_peakHold->value(),
        .falloffTime     = m_falloff->value(),
        .peakFalloffTime = m_peakFalloff->value(),
        .showPeaks       = m_peaksGroup->isChecked(),
        .showLegend      = m_showLegend->isChecked(),
        .updateFps       = m_updateFps->currentData().toInt(),
        .channelSpacing  = m_channelSpacing->value(),
        .barSize         = m_barSize->value(),
        .barSpacing      = m_barSpacing->value(),
        .barSections     = m_barSections->value(),
        .sectionSpacing  = m_sectionSpacing->value(),
        .meterColours    = QVariant{},
    };

    if(m_colourGroup->isChecked()) {
        Colours colours;
        colours.setColour(Colours::Type::Background, m_bgColour->colour());
        colours.setColour(Colours::Type::Peak, m_peakColour->colour());
        colours.setColour(Colours::Type::Legend, m_legendColour->colour());
        colours.setGradient(m_barGradient->colours());
        config.meterColours = QVariant::fromValue(colours);
    }

    return config;
}

void VuMeterConfigDialog::setConfig(const VuMeterWidget::ConfigData& config)
{
    m_peakHold->setValue(config.peakHoldTimeMs);
    m_falloff->setValue(config.falloffTime);
    m_peakFalloff->setValue(config.peakFalloffTime);
    m_peaksGroup->setChecked(config.showPeaks);
    m_showLegend->setChecked(config.showLegend);

    const int nearest = Gui::FrameRate::nearestPresetFps(config.updateFps);
    int fpsIndex      = m_updateFps->findData(nearest);
    if(fpsIndex < 0) {
        fpsIndex = m_updateFps->findData(Gui::FrameRate::toFps(DefaultFps));
    }
    m_updateFps->setCurrentIndex(fpsIndex);

    m_channelSpacing->setValue(config.channelSpacing);
    m_barSize->setValue(config.barSize);
    m_barSpacing->setValue(config.barSpacing);
    m_barSections->setValue(config.barSections);
    m_sectionSpacing->setValue(config.sectionSpacing);

    const bool customColours = config.meterColours.isValid() && config.meterColours.canConvert<Colours>()
                            && !config.meterColours.value<Colours>().isEmpty();
    const Colours colours    = customColours ? config.meterColours.value<Colours>() : Colours{};

    m_colourGroup->setChecked(customColours);
    m_bgColour->setColour(colours.colour(Colours::Type::Background, palette()));
    m_peakColour->setColour(colours.colour(Colours::Type::Peak, palette()));
    m_legendColour->setColour(colours.colour(Colours::Type::Legend, palette()));
    m_barGradient->setColours(colours.gradient(palette()));
    m_barGradient->setOrientation(widget()->orientation());
}
} // namespace Fooyin::VuMeter
