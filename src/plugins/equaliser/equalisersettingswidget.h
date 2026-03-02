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

#include <gui/dsp/dspsettingsprovider.h>

#include <QBasicTimer>
#include <QPointer>

#include <array>
#include <vector>

class QComboBox;
class QDoubleSpinBox;
class QPushButton;
class QSlider;
class QTimerEvent;

namespace Fooyin {
class ToolTip;

namespace Equaliser {
class EqualiserSettingsWidget : public DspSettingsDialog
{
public:
    explicit EqualiserSettingsWidget(QWidget* parent = nullptr);

    void loadSettings(const QByteArray& settings) override;
    [[nodiscard]] QByteArray saveSettings() const override;

protected:
    void restoreDefaults() override;
    void timerEvent(QTimerEvent* event) override;

private:
    static constexpr int PreviewDebounceMs = 16;

    struct PresetItem
    {
        QString name;
        QByteArray settings;
    };

    static int gainDbToSliderValue(double gainDb);
    static double sliderValueToGainDb(int sliderValue);
    static QString gainTooltip(double gainDb);

    void loadStoredPresets();
    void saveStoredPresets() const;
    [[nodiscard]] int presetIndexByName(const QString& name) const;
    void refreshPresets();
    void updatePresetButtons();
    void loadPreset();
    void savePreset();
    void deletePreset();
    void importPreset();
    void exportPreset();

    void refreshTooltips();
    void zeroAll();
    void autoLevel();
    void updateSliderToolTip(QSlider* slider);
    void hideSliderToolTip();

    QComboBox* m_presetBox;

    QPushButton* m_loadPresetButton;
    QPushButton* m_savePresetButton;
    QPushButton* m_deletePresetButton;
    QPushButton* m_importPresetButton;
    QPushButton* m_exportPresetButton;

    QComboBox* m_selectedBandCombo;
    QDoubleSpinBox* m_selectedBandSpin;

    QSlider* m_preampSlider;
    std::array<QSlider*, 18> m_bandSliders;
    QPointer<ToolTip> m_sliderToolTip;

    QBasicTimer m_previewTimer;
    std::vector<PresetItem> m_presets;

    void refreshSelectedBandEditor();
};

class EqualiserSettingsProvider : public DspSettingsProvider
{
public:
    [[nodiscard]] QString id() const override;
    DspSettingsDialog* createSettingsWidget(QWidget* parent) override;
};
} // namespace Equaliser
} // namespace Fooyin
