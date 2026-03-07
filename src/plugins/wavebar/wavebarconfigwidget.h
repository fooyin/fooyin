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

#pragma once

#include "wavebarwidget.h"

#include <gui/configdialog.h>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QSpinBox;

namespace Fooyin {
class ColourButton;

namespace WaveBar {
class WaveBarConfigDialog : public WidgetConfigDialog<WaveBarWidget, WaveBarWidget::ConfigData>
{
    Q_OBJECT

public:
    explicit WaveBarConfigDialog(WaveBarWidget* waveBar, QWidget* parent = nullptr);

    void apply() override;
    void saveDefaults() override;
    void restoreSavedDefaults() override;
    void restoreFactoryDefaults() override;
    void clearSavedDefaults() override;

protected:
    [[nodiscard]] WaveBarWidget::ConfigData config() const override;
    void setConfig(const WaveBarWidget::ConfigData& config) override;

private:
    void setGlobalCacheConfig(int numSamples);
    [[nodiscard]] int globalNumSamples() const;
    void updateCacheSize();
    void applyGlobalCacheConfig();

    QCheckBox* m_showLabels;
    QCheckBox* m_elapsedTotal;
    QCheckBox* m_minMax;
    QCheckBox* m_rms;
    QCheckBox* m_silence;
    QRadioButton* m_downmixOff;
    QRadioButton* m_downmixStereo;
    QRadioButton* m_downmixMono;
    QCheckBox* m_showCursor;
    QDoubleSpinBox* m_channelScale;
    QSpinBox* m_cursorWidth;
    QSpinBox* m_barWidth;
    QSpinBox* m_barGap;
    QDoubleSpinBox* m_maxScale;
    QSpinBox* m_centreGap;

    QGroupBox* m_colourGroup;

    ColourButton* m_bgUnplayed;
    ColourButton* m_bgPlayed;
    ColourButton* m_maxUnplayed;
    ColourButton* m_maxPlayed;
    ColourButton* m_maxBorder;
    ColourButton* m_minUnplayed;
    ColourButton* m_minPlayed;
    ColourButton* m_minBorder;
    ColourButton* m_rmsMaxUnplayed;
    ColourButton* m_rmsMaxPlayed;
    ColourButton* m_rmsMaxBorder;
    ColourButton* m_rmsMinUnplayed;
    ColourButton* m_rmsMinPlayed;
    ColourButton* m_rmsMinBorder;
    ColourButton* m_cursorColour;
    ColourButton* m_seekingCursorColour;

    QLabel* m_cacheSizeLabel;
    QComboBox* m_numSamples;
    QPushButton* m_clearCacheButton;
};
} // namespace WaveBar
} // namespace Fooyin
