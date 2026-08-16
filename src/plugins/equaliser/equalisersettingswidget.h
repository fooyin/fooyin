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

#include "equalisersliderstate.h"

#include <gui/dsp/dsplayouteditor.h>
#include <gui/dsp/dspsettingsprovider.h>

#include <QBasicTimer>
#include <QPointer>

#include <array>
#include <memory>

class QComboBox;
class QDoubleSpinBox;
class QJsonObject;
class QLabel;
class QMenu;
class QPushButton;
class QSlider;
class QTimerEvent;

namespace Fooyin {
class ToolTip;

namespace Equaliser {
class EqualiserPresetStore;
class ScaleLabelsWidget;

constexpr auto PreviewDebounceMs = 16;

class EqualiserLayoutEditor : public DspLayoutEditor
{
    Q_OBJECT

public:
    explicit EqualiserLayoutEditor(EqualiserPresetStore& presetStore, QWidget* parent = nullptr);

    void loadSettings(const QByteArray& settings) override;
    [[nodiscard]] QByteArray saveSettings() const override;

    void setControlsEnabled(bool enabled) override;
    void restoreDefaults() override;
    [[nodiscard]] QString restoreDefaultsActionText() const override;
    void populateContextMenu(QMenu* menu) override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    static constexpr size_t SliderCount = EqualiserDsp::BandCount + 1;

    void setControlsVisible(bool visible);
    [[nodiscard]] EqualiserSliderState sliderState() const;
    void applySliderState(const EqualiserSliderState& state);
    void updateValueLabels();
    void updatePresetSelection();
    void refreshPresets();
    void loadPreset(int index);
    void savePreset();
    void zeroLevel();
    void autoLevel();

    QWidget* m_controls;
    EqualiserPresetStore& m_presetStore;
    QWidget* m_controlRow;
    QPushButton* m_enabledToggle;
    QPushButton* m_zeroLevelButton;
    QPushButton* m_autoLevelButton;
    QComboBox* m_presetBox;
    QPushButton* m_savePresetButton;
    std::array<QSlider*, SliderCount> m_sliders;
    std::array<QLabel*, SliderCount> m_valueLabels;
    QPointer<ToolTip> m_sliderToolTip;
    QBasicTimer m_previewTimer;
    bool m_showControls;
};

class EqualiserSettingsWidget : public DspSettingsDialog
{
    Q_OBJECT

public:
    explicit EqualiserSettingsWidget(EqualiserPresetStore& presetStore, QWidget* parent = nullptr);

    void loadSettings(const QByteArray& settings) override;
    [[nodiscard]] QByteArray saveSettings() const override;

protected:
    void restoreDefaults() override;
    void timerEvent(QTimerEvent* event) override;

private:
    void connectSliderSignals(QSlider* slider, bool refreshBandEditor);
    [[nodiscard]] EqualiserSliderState sliderState() const;
    void applySliderState(const EqualiserSliderState& state);
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
    void refreshValueLabels();

    QComboBox* m_presetBox;
    EqualiserPresetStore& m_presetStore;

    QPushButton* m_loadPresetButton;
    QPushButton* m_savePresetButton;
    QPushButton* m_deletePresetButton;
    QPushButton* m_importPresetButton;
    QPushButton* m_exportPresetButton;

    QComboBox* m_selectedBandCombo;
    QDoubleSpinBox* m_selectedBandSpin;

    QSlider* m_preampSlider;
    QLabel* m_preampValueLabel;
    std::array<QSlider*, EqualiserDsp::BandCount> m_bandSliders;
    std::array<QLabel*, EqualiserDsp::BandCount> m_bandValueLabels;
    ScaleLabelsWidget* m_scaleTrackWidget;
    QPointer<ToolTip> m_sliderToolTip;

    QBasicTimer m_previewTimer;

    void refreshSelectedBandEditor();
};

class EqualiserSettingsProvider : public DspSettingsProvider
{
public:
    EqualiserSettingsProvider();
    ~EqualiserSettingsProvider() override;

    [[nodiscard]] QString id() const override;
    [[nodiscard]] QString displayName() const override;
    [[nodiscard]] QString viewMenuText() const override;
    [[nodiscard]] QString viewMenuStatusTip() const override;
    [[nodiscard]] bool showInViewMenu() const override;
    [[nodiscard]] bool showAsLayoutWidget() const override;
    [[nodiscard]] DspLayoutEditor* createLayoutEditor(QWidget* parent) override;
    DspSettingsDialog* createSettingsWidget(QWidget* parent) override;

private:
    std::unique_ptr<EqualiserPresetStore> m_presetStore;
};
} // namespace Equaliser
} // namespace Fooyin
