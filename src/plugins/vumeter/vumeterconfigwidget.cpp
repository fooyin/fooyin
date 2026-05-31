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

#include "vumeterconfigwidget.h"

#include "vumetercolours.h"

#include <gui/framerate.h>
#include <gui/widgets/colourbutton.h>

#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLocale>
#include <QSpinBox>

using namespace Qt::StringLiterals;

constexpr auto DefaultFps = Fooyin::Gui::FrameRate::Preset::Fps40;

namespace Fooyin::VuMeter {
VuMeterConfigDialog::VuMeterConfigDialog(VuMeter::VuMeterWidget* vuMeter, QWidget* parent)
    : WidgetConfigDialog{vuMeter, tr("VU Meter Settings"), parent}
    , m_peakHold{new QSpinBox(this)}
    , m_falloff{new QSpinBox(this)}
    , m_peakFalloff{new QSpinBox(this)}
    , m_updateFps{new QComboBox(this)}
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
    , m_leftColour{new ColourButton(this)}
    , m_rightColour{new ColourButton(this)}
{
    auto* meterGroup  = new QGroupBox(tr("Meter"), this);
    auto* meterLayout = new QGridLayout(meterGroup);
    auto* peaksLayout = new QGridLayout(m_peaksGroup);

    m_peakHold->setRange(0, 5000);
    m_peakHold->setSingleStep(100);
    m_peakHold->setSuffix(u" ms"_s);
    m_falloff->setRange(0, 500);
    m_falloff->setSingleStep(10);
    m_falloff->setSuffix(u" dB/s"_s);
    m_peakFalloff->setRange(0, 500);
    m_peakFalloff->setSingleStep(10);
    m_peakFalloff->setSuffix(u" dB/s"_s);

    for(const auto preset : Gui::FrameRate::Presets) {
        const int fps = Gui::FrameRate::toFps(preset);
        m_updateFps->addItem(tr("%1 fps").arg(fps), fps);
    }

    int row = 0;
    meterLayout->addWidget(new QLabel(tr("Falloff") + ":"_L1, this), row, 0);
    meterLayout->addWidget(m_falloff, row++, 1);
    meterLayout->addWidget(new QLabel(tr("Refresh rate") + ":"_L1, this), row, 0);
    meterLayout->addWidget(m_updateFps, row++, 1);
    meterLayout->setColumnStretch(2, 1);

    m_peaksGroup->setCheckable(true);

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
    coloursLayout->addWidget(m_bgColour, row++, 1);
    coloursLayout->addWidget(new QLabel(tr("Peak colour") + ":"_L1, this), row, 0);
    coloursLayout->addWidget(m_peakColour, row++, 1);
    coloursLayout->addWidget(new QLabel(tr("Legend colour") + ":"_L1, this), row, 0);
    coloursLayout->addWidget(m_legendColour, row++, 1);
    coloursLayout->addWidget(new QLabel(tr("Bar colours") + ":"_L1, this), row, 0);
    coloursLayout->addWidget(m_leftColour, row, 1);
    coloursLayout->addWidget(m_rightColour, row++, 2);
    coloursLayout->setColumnStretch(1, 1);
    coloursLayout->setColumnStretch(2, 1);

    auto* layout = contentLayout();
    layout->addWidget(meterGroup, 0, 0);
    layout->addWidget(m_peaksGroup, 0, 1);
    layout->addWidget(dimensionGroup, 1, 0, 1, 2);
    layout->addWidget(m_colourGroup, 2, 0, 1, 2);
    layout->setRowStretch(layout->rowCount(), 1);

    loadCurrentConfig();
}

VuMeterWidget::ConfigData VuMeterConfigDialog::config() const
{
    VuMeterWidget::ConfigData config{
        .peakHoldTimeMs  = m_peakHold->value(),
        .falloffTime     = m_falloff->value(),
        .peakFalloffTime = m_peakFalloff->value(),
        .showPeaks       = m_peaksGroup->isChecked(),
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
        colours.setColour(Colours::Type::Gradient1, m_leftColour->colour());
        colours.setColour(Colours::Type::Gradient2, m_rightColour->colour());
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
    m_leftColour->setColour(colours.colour(Colours::Type::Gradient1, palette()));
    m_rightColour->setColour(colours.colour(Colours::Type::Gradient2, palette()));
}
} // namespace Fooyin::VuMeter
