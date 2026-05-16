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

#include "soundtouchsettingswidget.h"

#include <gui/widgets/doubleslidereditor.h>

#include <QAction>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimerEvent>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <memory>

using namespace Qt::StringLiterals;

constexpr auto PreviewDebounceMs = 32;

constexpr auto TempoMin = 0.25;
constexpr auto TempoMax = 4.0;
constexpr auto PitchMin = -24.0;
constexpr auto PitchMax = 24.0;
constexpr auto RateMin  = 0.25;
constexpr auto RateMax  = 4.0;

namespace {
std::unique_ptr<Fooyin::SoundTouchDsp> createDsp(const Fooyin::SoundTouchDsp::Parameter parameter)
{
    using Parameter = Fooyin::SoundTouchDsp::Parameter;

    switch(parameter) {
        case Parameter::Tempo:
            return std::make_unique<Fooyin::SoundTouchTempoDsp>();
        case Parameter::Pitch:
            return std::make_unique<Fooyin::SoundTouchPitchDsp>();
        case Parameter::Rate:
            return std::make_unique<Fooyin::SoundTouchRateDsp>();
    }

    return std::make_unique<Fooyin::SoundTouchTempoDsp>();
}

void configureEditor(Fooyin::DoubleSliderEditor* editor, const Fooyin::SoundTouchDsp::Parameter parameter)
{
    using Parameter = Fooyin::SoundTouchDsp::Parameter;

    switch(parameter) {
        case Parameter::Tempo:
            editor->setRange(TempoMin, TempoMax);
            editor->setSingleStep(0.01);
            editor->setSuffix(QStringLiteral("x"));
            break;
        case Parameter::Pitch:
            editor->setRange(PitchMin, PitchMax);
            editor->setSingleStep(0.1);
            editor->setSuffix(QStringLiteral(" st"));
            break;
        case Parameter::Rate:
            editor->setRange(RateMin, RateMax);
            editor->setSingleStep(0.01);
            editor->setSuffix(QStringLiteral("x"));
            break;
    }
}

QString editorLabel(const Fooyin::SoundTouchDsp::Parameter parameter)
{
    using Parameter = Fooyin::SoundTouchDsp::Parameter;

    switch(parameter) {
        case Parameter::Tempo:
            return QObject::tr("Tempo multiplier");
        case Parameter::Pitch:
            return QObject::tr("Pitch shift");
        case Parameter::Rate:
            return QObject::tr("Rate multiplier");
    }

    return QStringLiteral("SoundTouch");
}

QString layoutLabel(const Fooyin::SoundTouchDsp::Parameter parameter)
{
    using Parameter = Fooyin::SoundTouchDsp::Parameter;

    switch(parameter) {
        case Parameter::Tempo:
            return QObject::tr("Tempo") + u":"_s;
        case Parameter::Pitch:
            return QObject::tr("Pitch") + u":"_s;
        case Parameter::Rate:
            return QObject::tr("Playback rate") + u":"_s;
    }

    return u"SoundTouch:"_s;
}

struct SliderConfig
{
    double min;
    double max;
    double step;
    QString suffix;
};

SliderConfig sliderConfig(const Fooyin::SoundTouchDsp::Parameter parameter)
{
    using Parameter = Fooyin::SoundTouchDsp::Parameter;

    switch(parameter) {
        case Parameter::Tempo:
            return {.min = TempoMin, .max = TempoMax, .step = 0.01, .suffix = QStringLiteral("x")};
        case Parameter::Pitch:
            return {.min = PitchMin, .max = PitchMax, .step = 0.1, .suffix = QStringLiteral(" st")};
        case Parameter::Rate:
            return {.min = RateMin, .max = RateMax, .step = 0.01, .suffix = QStringLiteral("x")};
    }

    return {.min = TempoMin, .max = TempoMax, .step = 0.01, .suffix = QStringLiteral("x")};
}
} // namespace

namespace Fooyin::SoundTouch {
SoundTouchLayoutEditor::SoundTouchLayoutEditor(const SoundTouchDsp::Parameter parameter, QWidget* parent)
    : DspLayoutEditor{parent}
    , m_parameter{parameter}
    , m_nameLabel{new QLabel(layoutLabel(parameter), this)}
    , m_slider{new QSlider(Qt::Horizontal, this)}
    , m_valueLabel{new QLabel(this)}
    , m_min{sliderConfig(parameter).min}
    , m_max{sliderConfig(parameter).max}
    , m_step{sliderConfig(parameter).step}
    , m_suffix{sliderConfig(parameter).suffix}
    , m_showNameLabel{true}
    , m_showValueLabel{true}
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});
    layout->addWidget(m_nameLabel, 0, Qt::AlignVCenter);
    layout->addWidget(m_slider, 1, Qt::AlignVCenter);
    layout->addWidget(m_valueLabel, 0, Qt::AlignVCenter);

    setNameLabelVisible(m_showNameLabel);
    setValueLabelVisible(m_showValueLabel);
    m_nameLabel->setAlignment(Qt::AlignVCenter);
    m_valueLabel->setAlignment(Qt::AlignVCenter);
    m_slider->setRange(0, static_cast<int>(std::round((m_max - m_min) / m_step)));
    m_slider->setSingleStep(1);
    m_slider->setMinimumWidth(80);

    QObject::connect(m_slider, &QSlider::valueChanged, this, [this]() {
        updateValueLabel();
        m_previewTimer.start(PreviewDebounceMs, this);
    });
}

void SoundTouchLayoutEditor::loadSettings(const QByteArray& settings)
{
    auto dsp = createDsp(m_parameter);
    dsp->loadSettings(settings);
    setValue(dsp->parameterValue());
}

QByteArray SoundTouchLayoutEditor::saveSettings() const
{
    auto dsp = createDsp(m_parameter);
    dsp->setParameterValue(value());
    return dsp->saveSettings();
}

void SoundTouchLayoutEditor::setControlsEnabled(bool enabled)
{
    m_nameLabel->setEnabled(enabled);
    m_slider->setEnabled(enabled);
    m_valueLabel->setEnabled(enabled);

    // So the widget can still receive context menu events when disabled
    m_nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents, !enabled);
    m_slider->setAttribute(Qt::WA_TransparentForMouseEvents, !enabled);
    m_valueLabel->setAttribute(Qt::WA_TransparentForMouseEvents, !enabled);
}

void SoundTouchLayoutEditor::restoreDefaults()
{
    auto dsp = createDsp(m_parameter);
    setValue(dsp->parameterDefaultValue());
}

void SoundTouchLayoutEditor::populateContextMenu(QMenu* menu)
{
    if(!menu) {
        return;
    }

    auto* showName = new QAction(tr("Show name"), menu);
    menu->addAction(showName);
    showName->setCheckable(true);
    showName->setChecked(m_showNameLabel);
    QObject::connect(showName, &QAction::triggered, this, &SoundTouchLayoutEditor::setNameLabelVisible);

    auto* showValue = new QAction(tr("Show value"), menu);
    menu->addAction(showValue);
    showValue->setCheckable(true);
    showValue->setChecked(m_showValueLabel);
    QObject::connect(showValue, &QAction::triggered, this, &SoundTouchLayoutEditor::setValueLabelVisible);
}

void SoundTouchLayoutEditor::saveLayoutData(QJsonObject& layout)
{
    layout["ShowName"_L1]  = m_showNameLabel;
    layout["ShowValue"_L1] = m_showValueLabel;
}

void SoundTouchLayoutEditor::loadLayoutData(const QJsonObject& layout)
{
    if(layout.contains("ShowName"_L1)) {
        setNameLabelVisible(layout.value("ShowName"_L1).toBool());
    }
    if(layout.contains("ShowValue"_L1)) {
        setValueLabelVisible(layout.value("ShowValue"_L1).toBool());
    }
}

void SoundTouchLayoutEditor::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_previewTimer.timerId()) {
        m_previewTimer.stop();
        Q_EMIT previewSettingsChanged(saveSettings());
        return;
    }

    DspLayoutEditor::timerEvent(event);
}

double SoundTouchLayoutEditor::value() const
{
    return m_min + (static_cast<double>(m_slider->value()) * m_step);
}

void SoundTouchLayoutEditor::setValue(double value)
{
    const int sliderValue = static_cast<int>(std::round((std::clamp(value, m_min, m_max) - m_min) / m_step));
    const QSignalBlocker blocker{m_slider};
    m_slider->setValue(sliderValue);
    updateValueLabel();
}

void SoundTouchLayoutEditor::setNameLabelVisible(bool visible)
{
    m_showNameLabel = visible;
    m_nameLabel->setVisible(visible);
    updateGeometry();
}

void SoundTouchLayoutEditor::setValueLabelVisible(bool visible)
{
    m_showValueLabel = visible;
    m_valueLabel->setVisible(visible);
    updateGeometry();
}

void SoundTouchLayoutEditor::updateValueLabel()
{
    m_valueLabel->setText(formattedValue());
    m_slider->setToolTip(formattedValue());
}

QString SoundTouchLayoutEditor::formattedValue() const
{
    const int decimals = m_step < 0.1 ? 2 : 1;
    return u"%1%2"_s.arg(QString::number(value(), 'f', decimals), m_suffix);
}

SoundTouchSettingsWidget::SoundTouchSettingsWidget(const SoundTouchDsp::Parameter parameter, QWidget* parent)
    : DspSettingsDialog{parent}
    , m_parameter{parameter}
    , m_valueEditor{new DoubleSliderEditor(editorLabel(parameter), this)}
{
    auto* layout = contentLayout();
    layout->addWidget(m_valueEditor);

    QObject::connect(m_valueEditor, &DoubleSliderEditor::valueChanged, this,
                     [this](double) { m_previewTimer.start(PreviewDebounceMs, this); });

    configureEditor(m_valueEditor, m_parameter);
}

void SoundTouchSettingsWidget::loadSettings(const QByteArray& settings)
{
    auto dsp = createDsp(m_parameter);
    if(!dsp) {
        return;
    }

    dsp->loadSettings(settings);
    m_valueEditor->setValue(dsp->parameterValue());
}

QByteArray SoundTouchSettingsWidget::saveSettings() const
{
    auto dsp = createDsp(m_parameter);
    if(!dsp) {
        return {};
    }

    dsp->setParameterValue(m_valueEditor->value());
    return dsp->saveSettings();
}

void SoundTouchSettingsWidget::restoreDefaults()
{
    if(auto dsp = createDsp(m_parameter)) {
        m_valueEditor->setValue(dsp->parameterDefaultValue());
    }
}

void SoundTouchSettingsWidget::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == m_previewTimer.timerId()) {
        m_previewTimer.stop();
        publishPreviewSettings();
        return;
    }

    DspSettingsDialog::timerEvent(event);
}

QString SoundTouchTempoSettingsProvider::id() const
{
    return QStringLiteral("fooyin.dsp.soundtouch.tempo");
}

QString SoundTouchTempoSettingsProvider::displayName() const
{
    return QCoreApplication::translate("Fooyin::SoundTouch::SoundTouchTempoSettingsProvider", "Tempo");
}

bool SoundTouchTempoSettingsProvider::showAsLayoutWidget() const
{
    return true;
}

DspLayoutEditor* SoundTouchTempoSettingsProvider::createLayoutEditor(QWidget* parent)
{
    return new SoundTouchLayoutEditor(SoundTouchDsp::Parameter::Tempo, parent);
}

DspSettingsDialog* SoundTouchTempoSettingsProvider::createSettingsWidget(QWidget* parent)
{
    return new SoundTouchSettingsWidget(SoundTouchDsp::Parameter::Tempo, parent);
}

QString SoundTouchPitchSettingsProvider::id() const
{
    return QStringLiteral("fooyin.dsp.soundtouch.pitch");
}

QString SoundTouchPitchSettingsProvider::displayName() const
{
    return QCoreApplication::translate("Fooyin::SoundTouch::SoundTouchPitchSettingsProvider", "Pitch");
}

bool SoundTouchPitchSettingsProvider::showAsLayoutWidget() const
{
    return true;
}

DspLayoutEditor* SoundTouchPitchSettingsProvider::createLayoutEditor(QWidget* parent)
{
    return new SoundTouchLayoutEditor(SoundTouchDsp::Parameter::Pitch, parent);
}

DspSettingsDialog* SoundTouchPitchSettingsProvider::createSettingsWidget(QWidget* parent)
{
    return new SoundTouchSettingsWidget(SoundTouchDsp::Parameter::Pitch, parent);
}

QString SoundTouchRateSettingsProvider::id() const
{
    return u"fooyin.dsp.soundtouch.rate"_s;
}

QString SoundTouchRateSettingsProvider::displayName() const
{
    return QCoreApplication::translate("Fooyin::SoundTouch::SoundTouchRateSettingsProvider", "Playback Rate");
}

bool SoundTouchRateSettingsProvider::showAsLayoutWidget() const
{
    return true;
}

DspLayoutEditor* SoundTouchRateSettingsProvider::createLayoutEditor(QWidget* parent)
{
    return new SoundTouchLayoutEditor(SoundTouchDsp::Parameter::Rate, parent);
}

DspSettingsDialog* SoundTouchRateSettingsProvider::createSettingsWidget(QWidget* parent)
{
    return new SoundTouchSettingsWidget(SoundTouchDsp::Parameter::Rate, parent);
}
} // namespace Fooyin::SoundTouch
