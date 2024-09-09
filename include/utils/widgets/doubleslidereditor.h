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

#include "fyutils_export.h"

#include <QWidget>

class QDoubleSpinBox;
class QSlider;

namespace Fooyin {
class FYUTILS_EXPORT DoubleSliderEditor : public QWidget
{
    Q_OBJECT

public:
    explicit DoubleSliderEditor(QWidget* parent = nullptr);

    [[nodiscard]] double value() const;
    [[nodiscard]] double minimum() const;
    [[nodiscard]] double maximum() const;
    [[nodiscard]] double singleStep() const;
    [[nodiscard]] QString prefix() const;
    [[nodiscard]] QString suffix() const;

    void setValue(double value);
    void setMinimum(double min);
    void setMaximum(double max);
    void setSingleStep(double step);
    void setRange(double min, double max);
    void setPrefix(const QString& prefix);
    void setSuffix(const QString& suffix);

signals:
    void valueChanged(double value);

public slots:
    void sliderValueChanged(int value);
    void spinBoxValueChanged(double value);

private:
    void updateSlider();

    QSlider* m_slider;
    QDoubleSpinBox* m_spinBox;
    bool m_updatingSlider;
    bool m_updatingSpinBox;
};
} // namespace Fooyin
