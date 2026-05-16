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

#include "soundtouchdsp.h"

#include <gui/dsp/dsplayouteditor.h>
#include <gui/dsp/dspsettingsprovider.h>

#include <QBasicTimer>
#include <QSize>

namespace Fooyin {
class DoubleSliderEditor;
}
class QLabel;
class QJsonObject;
class QMenu;
class QSlider;
class QTimerEvent;

namespace Fooyin::SoundTouch {
class SoundTouchLayoutEditor : public DspLayoutEditor
{
public:
    explicit SoundTouchLayoutEditor(SoundTouchDsp::Parameter parameter, QWidget* parent = nullptr);

    void loadSettings(const QByteArray& settings) override;
    [[nodiscard]] QByteArray saveSettings() const override;

    void setControlsEnabled(bool enabled) override;
    void restoreDefaults() override;
    void populateContextMenu(QMenu* menu) override;
    void saveLayoutData(QJsonObject& layout) override;
    void loadLayoutData(const QJsonObject& layout) override;

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    [[nodiscard]] double value() const;
    void setValue(double value);
    void setNameLabelVisible(bool visible);
    void setValueLabelVisible(bool visible);
    void updateValueLabel();
    [[nodiscard]] QString formattedValue() const;

    SoundTouchDsp::Parameter m_parameter;
    QLabel* m_nameLabel;
    QSlider* m_slider;
    QLabel* m_valueLabel;
    QBasicTimer m_previewTimer;
    double m_min;
    double m_max;
    double m_step;
    QString m_suffix;
    bool m_showNameLabel;
    bool m_showValueLabel;
};

class SoundTouchSettingsWidget : public DspSettingsDialog
{
public:
    explicit SoundTouchSettingsWidget(SoundTouchDsp::Parameter parameter, QWidget* parent = nullptr);

    void loadSettings(const QByteArray& settings) override;
    [[nodiscard]] QByteArray saveSettings() const override;

protected:
    void restoreDefaults() override;
    void timerEvent(QTimerEvent* event) override;

private:
    SoundTouchDsp::Parameter m_parameter;
    DoubleSliderEditor* m_valueEditor;
    QBasicTimer m_previewTimer;
};

class SoundTouchTempoSettingsProvider : public DspSettingsProvider
{
public:
    [[nodiscard]] QString id() const override;
    [[nodiscard]] QString displayName() const override;
    [[nodiscard]] bool showAsLayoutWidget() const override;
    [[nodiscard]] DspLayoutEditor* createLayoutEditor(QWidget* parent) override;
    DspSettingsDialog* createSettingsWidget(QWidget* parent) override;
};

class SoundTouchPitchSettingsProvider : public DspSettingsProvider
{
public:
    [[nodiscard]] QString id() const override;
    [[nodiscard]] QString displayName() const override;
    [[nodiscard]] bool showAsLayoutWidget() const override;
    [[nodiscard]] DspLayoutEditor* createLayoutEditor(QWidget* parent) override;
    DspSettingsDialog* createSettingsWidget(QWidget* parent) override;
};

class SoundTouchRateSettingsProvider : public DspSettingsProvider
{
public:
    [[nodiscard]] QString id() const override;
    [[nodiscard]] QString displayName() const override;
    [[nodiscard]] bool showAsLayoutWidget() const override;
    [[nodiscard]] DspLayoutEditor* createLayoutEditor(QWidget* parent) override;
    DspSettingsDialog* createSettingsWidget(QWidget* parent) override;
};
} // namespace Fooyin::SoundTouch
