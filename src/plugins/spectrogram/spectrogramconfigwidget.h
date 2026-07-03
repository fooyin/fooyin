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

#include "spectrogramwidget.h"

#include <gui/configdialog.h>

class QCheckBox;
class QComboBox;
class QSpinBox;

namespace Fooyin {
class GradientEditor;

namespace Spectrogram {
class SpectrogramConfigDialog : public WidgetConfigDialog<SpectrogramWidget, SpectrogramWidget::ConfigData>
{
    Q_OBJECT

public:
    explicit SpectrogramConfigDialog(SpectrogramWidget* widget, QWidget* parent = nullptr);

protected:
    [[nodiscard]] SpectrogramWidget::ConfigData config() const override;
    void setConfig(const SpectrogramWidget::ConfigData& config) override;

private:
    QCheckBox* m_logFrequency;
    QCheckBox* m_clearOnTrackChange;
    QComboBox* m_channelMode;
    QComboBox* m_presentationMode;
    QSpinBox* m_channelSpacing;
    QComboBox* m_fftSize;
    QComboBox* m_windowFunction;
    QSpinBox* m_minDb;
    QSpinBox* m_maxDb;
    GradientEditor* m_colours;
};
} // namespace Spectrogram
} // namespace Fooyin