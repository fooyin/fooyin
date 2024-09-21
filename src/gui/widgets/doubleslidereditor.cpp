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

#include <gui/widgets/doubleslidereditor.h>

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>

namespace Fooyin {
DoubleSliderEditor::DoubleSliderEditor(const QString& name, QWidget* parent)
    : QWidget{parent}
    , m_slider{new QSlider(Qt::Horizontal, this)}
    , m_spinBox{new QDoubleSpinBox(this)}
    , m_updatingSlider{false}
    , m_updatingSpinBox{false}
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});

    if(!name.isEmpty()) {
        auto* label = new QLabel(name + u":", this);
        layout->addWidget(label);
    }

    layout->addWidget(m_slider, 1);
    layout->addWidget(m_spinBox);

    QObject::connect(m_slider, &QSlider::valueChanged, this, &DoubleSliderEditor::sliderValueChanged);
    QObject::connect(m_spinBox, &QDoubleSpinBox::valueChanged, this, &DoubleSliderEditor::spinBoxValueChanged);

    m_slider->setMinimum(0);
    m_slider->setMaximum(1000000);
    m_slider->setSingleStep(0);
}

DoubleSliderEditor::DoubleSliderEditor(QWidget* parent)
    : DoubleSliderEditor{{}, parent}
{ }

double DoubleSliderEditor::value() const
{
    return m_spinBox->value();
}

double DoubleSliderEditor::minimum() const
{
    return m_spinBox->minimum();
}

void DoubleSliderEditor::setMinimum(double min)
{
    m_spinBox->setMinimum(min);
    updateSlider();
}

double DoubleSliderEditor::maximum() const
{
    return m_spinBox->maximum();
}

void DoubleSliderEditor::setMaximum(double max)
{
    m_spinBox->setMaximum(max);
    updateSlider();
}

double DoubleSliderEditor::singleStep() const
{
    return m_spinBox->singleStep();
}

void DoubleSliderEditor::setSingleStep(double step)
{
    m_spinBox->setSingleStep(step);
}

void DoubleSliderEditor::setRange(double min, double max)
{
    m_spinBox->setRange(min, max);
    updateSlider();
}

QString DoubleSliderEditor::prefix() const
{
    return m_spinBox->prefix();
}

void DoubleSliderEditor::setPrefix(const QString& prefix)
{
    m_spinBox->setPrefix(prefix);
}

QString DoubleSliderEditor::suffix() const
{
    return m_spinBox->suffix();
}

void DoubleSliderEditor::setSuffix(const QString& suffix)
{
    m_spinBox->setSuffix(suffix);
}

void DoubleSliderEditor::setValue(double value)
{
    if(value != m_spinBox->value()) {
        m_updatingSpinBox = true;
        m_spinBox->setValue(value);
        m_updatingSpinBox = false;
        updateSlider();
    }
}

void DoubleSliderEditor::sliderValueChanged(int value)
{
    if(m_updatingSlider) {
        return;
    }

    const double min = m_spinBox->minimum();
    const double max = m_spinBox->maximum();

    const double ratio   = static_cast<double>(value) / m_slider->maximum();
    const double spinVal = min + (max - min) * ratio;

    setValue(spinVal);
    emit valueChanged(m_spinBox->value());
}

void DoubleSliderEditor::spinBoxValueChanged(double value)
{
    if(!m_updatingSpinBox) {
        updateSlider();
        emit valueChanged(value);
    }
}

void DoubleSliderEditor::updateSlider()
{
    const double min = m_spinBox->minimum();
    const double max = m_spinBox->maximum();
    double value     = m_spinBox->value();

    const double step = m_spinBox->singleStep();
    value             = std::round(value / step) * step;

    const double ratio  = (value - min) / (max - min);
    const int sliderVal = static_cast<int>(std::round(m_slider->maximum() * ratio));

    if(sliderVal != m_slider->value()) {
        m_slider->setValue(sliderVal);
    }
}
} // namespace Fooyin

#include "gui/widgets/moc_doubleslidereditor.cpp"
