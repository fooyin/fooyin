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

class QSlider;
class QSpinBox;

namespace Fooyin {
class FYUTILS_EXPORT SliderEditor : public QWidget
{
    Q_OBJECT

public:
    explicit SliderEditor(const QString& name, QWidget* parent = nullptr);
    explicit SliderEditor(QWidget* parent = nullptr);

    [[nodiscard]] int value() const;
    void setValue(int value);

    [[nodiscard]] int minimum() const;
    void setMinimum(int min);

    [[nodiscard]] int maximum() const;
    void setMaximum(int max);

    [[nodiscard]] int singleStep() const;
    void setSingleStep(int step);

    void setRange(int min, int max);

    [[nodiscard]] QString prefix() const;
    void setPrefix(const QString& prefix);

    [[nodiscard]] QString suffix() const;
    void setSuffix(const QString& suffix);

signals:
    void valueChanged(int value);

private slots:
    void sliderValueChanged(int value);
    void spinBoxValueChanged(int value);

private:
    QSlider* m_slider;
    QSpinBox* m_spinBox;
    bool m_updatingSlider;
    bool m_updatingSpinBox;
};
} // namespace Fooyin
