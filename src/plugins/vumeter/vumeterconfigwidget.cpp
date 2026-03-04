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
#include <QSpinBox>

using namespace Qt::StringLiterals;

constexpr auto DefaultFps = Fooyin::Gui::FrameRate::Preset::Fps40;

namespace Fooyin::VuMeter {
VuMeterConfigDialog::VuMeterConfigDialog(VuMeter::VuMeterWidget* vuMeter, QWidget* parent)
    : WidgetConfigDialog{vuMeter, tr("Configure %1").arg(vuMeter->name()), parent}
    , m_peakHold{new QDoubleSpinBox(this)}
    , m_falloff{new QDoubleSpinBox(this)}
    , m_updateFps{new QComboBox(this)}
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
    auto* generalGroup  = new QGroupBox(tr("General"), this);
    auto* generalLayout = new QGridLayout(generalGroup);

    m_peakHold->setRange(0.1, 30.0);
    m_peakHold->setSuffix(" "_L1 + tr("seconds"));
    m_falloff->setRange(0.1, 96.0);
    m_falloff->setSuffix(" "_L1 + tr("dB per second"));

    for(const auto preset : Gui::FrameRate::Presets) {
        const int fps = Gui::FrameRate::toFps(preset);
        m_updateFps->addItem(tr("%1 FPS").arg(fps), fps);
    }

    int row = 0;
    generalLayout->addWidget(new QLabel(tr("Peak hold time") + ":"_L1, this), row, 0);
    generalLayout->addWidget(m_peakHold, row++, 1);
    generalLayout->addWidget(new QLabel(tr("Falloff time") + ":"_L1, this), row, 0);
    generalLayout->addWidget(m_falloff, row++, 1);
    generalLayout->addWidget(new QLabel(tr("Refresh rate") + ":"_L1, this), row, 0);
    generalLayout->addWidget(m_updateFps, row++, 1);
    generalLayout->setColumnStretch(2, 1);

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
    layout->addWidget(generalGroup, 0, 0);
    layout->addWidget(dimensionGroup, 1, 0);
    layout->addWidget(m_colourGroup, 2, 0);
    layout->setRowStretch(layout->rowCount(), 1);

    loadCurrentConfig();
}

VuMeterWidget::ConfigData VuMeterConfigDialog::config() const
{
    VuMeterWidget::ConfigData config{
        .peakHoldTime   = m_peakHold->value(),
        .falloffTime    = m_falloff->value(),
        .updateFps      = m_updateFps->currentData().toInt(),
        .channelSpacing = m_channelSpacing->value(),
        .barSize        = m_barSize->value(),
        .barSpacing     = m_barSpacing->value(),
        .barSections    = m_barSections->value(),
        .sectionSpacing = m_sectionSpacing->value(),
        .meterColours   = QVariant{},
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
    m_peakHold->setValue(config.peakHoldTime);
    m_falloff->setValue(config.falloffTime);

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

    const bool customColours = config.meterColours.isValid() && config.meterColours.canConvert<Colours>();
    const Colours colours    = customColours ? config.meterColours.value<Colours>() : Colours{};

    m_colourGroup->setChecked(customColours);
    m_bgColour->setColour(colours.colour(Colours::Type::Background));
    m_peakColour->setColour(colours.colour(Colours::Type::Peak));
    m_legendColour->setColour(colours.colour(Colours::Type::Legend));
    m_leftColour->setColour(colours.colour(Colours::Type::Gradient1));
    m_rightColour->setColour(colours.colour(Colours::Type::Gradient2));
}
} // namespace Fooyin::VuMeter
