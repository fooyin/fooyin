/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

#include <utils/logslider.h>

#include <cmath>

namespace Fooyin {
LogSlider::LogSlider(Qt::Orientation type, QWidget* parent)
    : Slider{type, parent}
    , m_scale{100.0}
{
    QObject::connect(this, &QSlider::valueChanged, this,
                     [this](double value) { emit logValueChanged(std::pow(10, (value / m_scale))); });
}

void LogSlider::setRange(double min, double max)
{
    const auto logMin = static_cast<int>(std::log10(min) * m_scale);
    const auto logMax = static_cast<int>(std::log10(max) * m_scale);

    QSlider::setRange(logMin, logMax);
}

void LogSlider::setScale(double scale)
{
    m_scale = scale;
}

void LogSlider::setNaturalValue(double value)
{
    QSlider::setValue(static_cast<int>(std::log10(value) * m_scale));
}
} // namespace Fooyin

#include "utils/moc_logslider.cpp"
