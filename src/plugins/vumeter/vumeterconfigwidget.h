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

#pragma once

#include "vumeterwidget.h"

#include <gui/configdialog.h>

class QComboBox;
class QCheckBox;
class QGroupBox;
class QSpinBox;

namespace Fooyin {
class ColourButton;
class GradientEditor;

namespace VuMeter {
class VuMeterConfigDialog : public WidgetConfigDialog<VuMeterWidget, VuMeterWidget::ConfigData>
{
    Q_OBJECT

public:
    explicit VuMeterConfigDialog(VuMeterWidget* vuMeter, QWidget* parent = nullptr);

private:
    [[nodiscard]] VuMeterWidget::ConfigData config() const override;
    void setConfig(const VuMeterWidget::ConfigData& config) override;

    QSpinBox* m_peakHold;
    QSpinBox* m_falloff;
    QSpinBox* m_peakFalloff;
    QComboBox* m_updateFps;
    QCheckBox* m_showLegend;
    QGroupBox* m_peaksGroup;

    QSpinBox* m_channelSpacing;
    QSpinBox* m_barSize;
    QSpinBox* m_barSpacing;
    QSpinBox* m_barSections;
    QSpinBox* m_sectionSpacing;

    QGroupBox* m_colourGroup;
    ColourButton* m_bgColour;
    ColourButton* m_peakColour;
    ColourButton* m_legendColour;
    GradientEditor* m_barGradient;
};
} // namespace VuMeter
} // namespace Fooyin
