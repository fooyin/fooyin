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

#include <gui/widgets/slidereditor.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>

namespace Fooyin {
SliderEditor::SliderEditor(const QString& name, QWidget* parent)
    : QWidget{parent}
    , m_slider{new QSlider(Qt::Horizontal, this)}
    , m_spinBox{new QSpinBox(this)}
    , m_updatingSlider{false}
    , m_updatingSpinBox{false}
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins({});

    if(!name.isEmpty()) {
        auto* label = new QLabel(name + u":", this);
        layout->addWidget(label);
    }

    layout->addWidget(m_slider);
    layout->addWidget(m_spinBox);

    QObject::connect(m_slider, &QSlider::valueChanged, this, &SliderEditor::sliderValueChanged);
    QObject::connect(m_spinBox, &QSpinBox::valueChanged, this, &SliderEditor::spinBoxValueChanged);
}

SliderEditor::SliderEditor(QWidget* parent)
    : SliderEditor{{}, parent}
{ }

int SliderEditor::value() const
{
    return m_spinBox->value();
}

void SliderEditor::setValue(int value)
{
    if(value != m_spinBox->value()) {
        m_updatingSpinBox = true;
        m_spinBox->setValue(value);
        m_updatingSpinBox = false;
        m_slider->setValue(value);
    }
}

int SliderEditor::minimum() const
{
    return m_spinBox->minimum();
}

void SliderEditor::setMinimum(int min)
{
    m_spinBox->setMinimum(min);
    m_slider->setMinimum(min);
}

int SliderEditor::maximum() const
{
    return m_spinBox->maximum();
}

void SliderEditor::setMaximum(int max)
{
    m_spinBox->setMaximum(max);
    m_slider->setMaximum(max);
}

int SliderEditor::singleStep() const
{
    return m_spinBox->singleStep();
}

void SliderEditor::setSingleStep(int step)
{
    m_spinBox->setSingleStep(step);
}

void SliderEditor::setRange(int min, int max)
{
    m_spinBox->setRange(min, max);
    m_slider->setRange(min, max);
}

QString SliderEditor::prefix() const
{
    return m_spinBox->prefix();
}

void SliderEditor::setPrefix(const QString& prefix)
{
    m_spinBox->setPrefix(prefix);
}

QString SliderEditor::suffix() const
{
    return m_spinBox->suffix();
}

void SliderEditor::setSuffix(const QString& suffix)
{
    m_spinBox->setSuffix(suffix);
}

void SliderEditor::sliderValueChanged(int value)
{
    if(!m_updatingSlider) {
        setValue(value);
        emit valueChanged(value);
    }
}

void SliderEditor::spinBoxValueChanged(int value)
{
    if(!m_updatingSpinBox) {
        m_slider->setValue(value);
        emit valueChanged(value);
    }
}
} // namespace Fooyin

#include "gui/widgets/moc_slidereditor.cpp"
