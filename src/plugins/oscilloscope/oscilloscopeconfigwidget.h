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

#include "oscilloscopewidget.h"

#include <gui/configdialog.h>

class QCheckBox;
class QComboBox;
class QGroupBox;

namespace Fooyin {
class ColourButton;

namespace Oscilloscope {
class OscilloscopeConfigDialog : public WidgetConfigDialog<OscilloscopeWidget, OscilloscopeWidget::ConfigData>
{
    Q_OBJECT

public:
    explicit OscilloscopeConfigDialog(OscilloscopeWidget* oscilloscope, QWidget* parent = nullptr);

protected:
    [[nodiscard]] OscilloscopeWidget::ConfigData config() const override;
    void setConfig(const OscilloscopeWidget::ConfigData& config) override;

private:
    QComboBox* m_curveDurationMs;
    QComboBox* m_zoomPercent;
    QComboBox* m_updateFps;
    QComboBox* m_downmixMode;
    QCheckBox* m_showZeroLine;
    QGroupBox* m_colourGroup;
    ColourButton* m_backgroundColour;
    ColourButton* m_waveformColour;
    ColourButton* m_zeroLineColour;
};
} // namespace Oscilloscope
} // namespace Fooyin
