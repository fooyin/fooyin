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

#include <QTimerEvent>
#include <QVBoxLayout>

#include <memory>

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

    return QObject::tr("SoundTouch");
}
} // namespace

namespace Fooyin::SoundTouch {
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

DspSettingsDialog* SoundTouchTempoSettingsProvider::createSettingsWidget(QWidget* parent)
{
    return new SoundTouchSettingsWidget(SoundTouchDsp::Parameter::Tempo, parent);
}

QString SoundTouchPitchSettingsProvider::id() const
{
    return QStringLiteral("fooyin.dsp.soundtouch.pitch");
}

DspSettingsDialog* SoundTouchPitchSettingsProvider::createSettingsWidget(QWidget* parent)
{
    return new SoundTouchSettingsWidget(SoundTouchDsp::Parameter::Pitch, parent);
}

QString SoundTouchRateSettingsProvider::id() const
{
    return QStringLiteral("fooyin.dsp.soundtouch.rate");
}

DspSettingsDialog* SoundTouchRateSettingsProvider::createSettingsWidget(QWidget* parent)
{
    return new SoundTouchSettingsWidget(SoundTouchDsp::Parameter::Rate, parent);
}
} // namespace Fooyin::SoundTouch
