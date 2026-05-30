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

#pragma once

#include "spectrumwidget.h"

#include <gui/configdialog.h>

class QComboBox;
class QCheckBox;
class QGroupBox;
class QSpinBox;

namespace Fooyin {
class ColourButton;
class FontButton;
class GradientEditor;

namespace Spectrum {
class SpectrumConfigDialog : public WidgetConfigDialog<SpectrumWidget, SpectrumWidget::ConfigData>
{
    Q_OBJECT

public:
    explicit SpectrumConfigDialog(SpectrumWidget* spectrum, QWidget* parent = nullptr);

private:
    [[nodiscard]] SpectrumWidget::ConfigData config() const override;
    void setConfig(const SpectrumWidget::ConfigData& config) override;

    QSpinBox* m_bandCount;
    QSpinBox* m_barSpacing;
    QSpinBox* m_barSections;
    QSpinBox* m_sectionSpacing;
    QComboBox* m_labelMode;
    QSpinBox* m_minFrequency;
    QSpinBox* m_maxFrequency;
    QSpinBox* m_minNote;
    QSpinBox* m_maxNote;
    QSpinBox* m_pitchHz;
    QSpinBox* m_transpose;
    QGroupBox* m_amplitudesGroup;
    QSpinBox* m_amplitudeMinDb;
    QSpinBox* m_amplitudeMaxDb;
    QSpinBox* m_amplitudeHoldTime;
    QSpinBox* m_decayTime;
    QGroupBox* m_peaksGroup;
    QSpinBox* m_peakHoldTime;
    QSpinBox* m_peakGravity;
    QComboBox* m_updateFps;
    QComboBox* m_fftSize;
    QComboBox* m_windowFunction;
    QComboBox* m_drawStyle;
    QCheckBox* m_showTopLabels;
    QCheckBox* m_showBottomLabels;
    QCheckBox* m_showLeftLabels;
    QCheckBox* m_showRightLabels;
    QCheckBox* m_showHorizontalGrid;
    QCheckBox* m_showVerticalGrid;
    QCheckBox* m_showWhiteKeys;
    QCheckBox* m_showBlackKeys;
    QCheckBox* m_showTooltip;
    QCheckBox* m_fillSpectrum;
    QCheckBox* m_interpolate;
    FontButton* m_axisFont;
    QGroupBox* m_colourGroup;
    ColourButton* m_textColour;
    ColourButton* m_backgroundColour;
    GradientEditor* m_barGradient;
    ColourButton* m_peaksColour;
    ColourButton* m_horizontalGridColour;
    ColourButton* m_verticalGridColour;
    ColourButton* m_octaveGridColour;
    ColourButton* m_whiteKeyColour;
    ColourButton* m_blackKeyColour;
};
} // namespace Spectrum
} // namespace Fooyin
