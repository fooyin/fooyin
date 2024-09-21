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

#include "fygui_export.h"

#include <QWidget>

class QDoubleSpinBox;
class QSlider;

namespace Fooyin {
class FYGUI_EXPORT DoubleSliderEditor : public QWidget
{
    Q_OBJECT

public:
    explicit DoubleSliderEditor(const QString& name, QWidget* parent = nullptr);
    explicit DoubleSliderEditor(QWidget* parent = nullptr);

    [[nodiscard]] double value() const;
    void setValue(double value);

    [[nodiscard]] double minimum() const;
    void setMinimum(double min);

    [[nodiscard]] double maximum() const;
    void setMaximum(double max);

    [[nodiscard]] double singleStep() const;
    void setSingleStep(double step);

    void setRange(double min, double max);

    [[nodiscard]] QString prefix() const;
    void setPrefix(const QString& prefix);

    [[nodiscard]] QString suffix() const;
    void setSuffix(const QString& suffix);

signals:
    void valueChanged(double value);

private slots:
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
