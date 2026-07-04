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

#include "oscilloscopeconfigwidget.h"

#include "oscilloscopecolours.h"

#include <gui/framerate.h>
#include <gui/widgets/colourbutton.h>

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>

using namespace Qt::StringLiterals;

namespace Fooyin::Oscilloscope {
OscilloscopeConfigDialog::OscilloscopeConfigDialog(OscilloscopeWidget* oscilloscope, QWidget* parent)
    : WidgetConfigDialog{oscilloscope, tr("Oscilloscope Settings"), parent}
    , m_curveDurationMs{new QComboBox(this)}
    , m_zoomPercent{new QComboBox(this)}
    , m_updateFps{new QComboBox(this)}
    , m_downmixMode{new QComboBox(this)}
    , m_showZeroLine{new QCheckBox(tr("Show zero line"), this)}
    , m_colourGroup{new QGroupBox(tr("Custom colours"), this)}
    , m_backgroundColour{new ColourButton(tr("Background colour") + ":"_L1, true, this)}
    , m_waveformColour{new ColourButton(tr("Waveform colour") + ":"_L1, true, this)}
    , m_zeroLineColour{new ColourButton(tr("Zero line colour") + ":"_L1, true, this)}
{
    auto* generalGroup  = new QGroupBox(tr("General"), this);
    auto* generalLayout = new QGridLayout(generalGroup);

    for(const int durationMs : OscilloscopeWidget::CurveDurationPresets) {
        m_curveDurationMs->addItem(QString::number(durationMs), durationMs);
    }
    m_curveDurationMs->setEditable(true);
    m_curveDurationMs->setInsertPolicy(QComboBox::NoInsert);
    m_curveDurationMs->lineEdit()->setValidator(new QIntValidator(
        OscilloscopeWidget::MinCurveDurationMs, OscilloscopeWidget::MaxCurveDurationMs, m_curveDurationMs));
    m_curveDurationMs->setToolTip(tr("Time span of audio displayed across the oscilloscope"));

    for(const int zoomPercent : OscilloscopeWidget::ZoomPresets) {
        m_zoomPercent->addItem(QString::number(zoomPercent), zoomPercent);
    }
    m_zoomPercent->setEditable(true);
    m_zoomPercent->setInsertPolicy(QComboBox::NoInsert);
    m_zoomPercent->lineEdit()->setValidator(
        new QIntValidator(OscilloscopeWidget::MinZoomPercent, OscilloscopeWidget::MaxZoomPercent, m_zoomPercent));
    m_zoomPercent->setToolTip(tr("Vertical amplitude scale; high values may overlap stereo channels"));

    for(const auto preset : Gui::FrameRate::Presets) {
        const int fps = Gui::FrameRate::toFps(preset);
        m_updateFps->addItem(tr("%1 fps").arg(fps), fps);
    }
    m_updateFps->setToolTip(tr("Maximum waveform refresh rate; lower values can reduce motion blending"));

    m_downmixMode->addItem(tr("Stereo"), static_cast<int>(OscilloscopeWidget::DownmixMode::Stereo));
    m_downmixMode->addItem(tr("Mono"), static_cast<int>(OscilloscopeWidget::DownmixMode::Mono));
    m_downmixMode->setToolTip(tr("Display separate stereo channels or combine them into mono"));

    int row = 0;
    generalLayout->addWidget(new QLabel(tr("Curve duration (ms)") + ":"_L1, this), row, 0);
    generalLayout->addWidget(m_curveDurationMs, row++, 1);
    generalLayout->addWidget(new QLabel(tr("Zoom (%)") + ":"_L1, this), row, 0);
    generalLayout->addWidget(m_zoomPercent, row++, 1);
    generalLayout->addWidget(new QLabel(tr("Update FPS") + ":"_L1, this), row, 0);
    generalLayout->addWidget(m_updateFps, row++, 1);
    generalLayout->addWidget(new QLabel(tr("Downmix mode") + ":"_L1, this), row, 0);
    generalLayout->addWidget(m_downmixMode, row++, 1);
    generalLayout->addWidget(m_showZeroLine, row++, 0, 1, 2);
    generalLayout->setColumnStretch(2, 1);

    auto* coloursLayout = new QGridLayout(m_colourGroup);

    ColourButton::alignLabels({m_backgroundColour, m_waveformColour, m_zeroLineColour});

    row = 0;
    coloursLayout->addWidget(m_backgroundColour, row++, 0, 1, 2);
    coloursLayout->addWidget(m_waveformColour, row++, 0, 1, 2);
    coloursLayout->addWidget(m_zeroLineColour, row++, 0, 1, 2);
    coloursLayout->setColumnStretch(1, 1);

    auto* layout{contentLayout()};
    layout->addWidget(generalGroup, 0, 0);
    layout->addWidget(m_colourGroup, 0, 1);
    layout->setRowStretch(layout->rowCount(), 1);

    loadCurrentConfig();
}

OscilloscopeWidget::ConfigData OscilloscopeConfigDialog::config() const
{
    OscilloscopeWidget::ConfigData config{
        .curveDurationMs = m_curveDurationMs->currentText().toInt(),
        .zoomPercent     = m_zoomPercent->currentText().toInt(),
        .updateFps       = m_updateFps->currentData().toInt(),
        .downmixMode     = static_cast<OscilloscopeWidget::DownmixMode>(m_downmixMode->currentData().toInt()),
        .showZeroLine    = m_showZeroLine->isChecked(),
        .colours         = QVariant{},
    };

    Colours colours;
    if(m_backgroundColour->isChecked()) {
        colours.setColour(Colours::Type::Background, m_backgroundColour->colour());
    }
    if(m_waveformColour->isChecked()) {
        colours.setColour(Colours::Type::Waveform, m_waveformColour->colour());
    }
    if(m_zeroLineColour->isChecked()) {
        colours.setColour(Colours::Type::ZeroLine, m_zeroLineColour->colour());
    }
    if(!colours.isEmpty()) {
        config.colours = QVariant::fromValue(colours);
    }

    return config;
}

void OscilloscopeConfigDialog::setConfig(const OscilloscopeWidget::ConfigData& config)
{
    const int durationIndex = m_curveDurationMs->findData(config.curveDurationMs);
    if(durationIndex >= 0) {
        m_curveDurationMs->setCurrentIndex(durationIndex);
    }
    else {
        m_curveDurationMs->setEditText(QString::number(config.curveDurationMs));
    }

    const int zoomIndex = m_zoomPercent->findData(config.zoomPercent);
    if(zoomIndex >= 0) {
        m_zoomPercent->setCurrentIndex(zoomIndex);
    }
    else {
        m_zoomPercent->setEditText(QString::number(config.zoomPercent));
    }

    const int nearestFps = Gui::FrameRate::nearestPresetFps(config.updateFps);
    int fpsIndex         = m_updateFps->findData(nearestFps);
    if(fpsIndex < 0) {
        fpsIndex = m_updateFps->findData(OscilloscopeWidget::DefaultUpdateFps);
    }
    m_updateFps->setCurrentIndex(fpsIndex);

    int downmixIndex = m_downmixMode->findData(static_cast<int>(config.downmixMode));
    if(downmixIndex < 0) {
        downmixIndex = m_downmixMode->findData(static_cast<int>(OscilloscopeWidget::DownmixMode::Stereo));
    }
    m_downmixMode->setCurrentIndex(downmixIndex);
    m_showZeroLine->setChecked(config.showZeroLine);

    const bool customColours = config.colours.isValid() && config.colours.canConvert<Colours>()
                            && !config.colours.value<Colours>().isEmpty();
    const Colours colours    = customColours ? config.colours.value<Colours>() : Colours{};

    m_backgroundColour->setChecked(colours.hasOverride(Colours::Type::Background));
    m_backgroundColour->setColour(colours.colour(Colours::Type::Background, widget()->palette()));
    m_waveformColour->setChecked(colours.hasOverride(Colours::Type::Waveform));
    m_waveformColour->setColour(colours.colour(Colours::Type::Waveform, widget()->palette()));
    m_zeroLineColour->setChecked(colours.hasOverride(Colours::Type::ZeroLine));
    m_zeroLineColour->setColour(colours.colour(Colours::Type::ZeroLine, widget()->palette()));
}
} // namespace Fooyin::Oscilloscope

#include "moc_oscilloscopeconfigwidget.cpp"
