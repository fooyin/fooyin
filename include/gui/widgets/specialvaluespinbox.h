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

#include <QDoubleSpinBox>
#include <QSpinBox>

namespace Fooyin {
class FYGUI_EXPORT SpecialValueSpinBox : public QSpinBox
{
    Q_OBJECT

public:
    using QSpinBox::QSpinBox;

    QString specialValue(int val);
    void addSpecialValue(int val, const QString& text);

    void setSpecialPrefix(const QString& prefix);
    void setSpecialSuffix(const QString& suffix);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    [[nodiscard]] int valueFromText(const QString& text) const override;
    [[nodiscard]] QString textFromValue(int val) const override;

private:
    void updateSize();

    std::unordered_map<int, QString> m_specialValues;
    mutable QSize m_cachedSizeHint;
    mutable QSize m_cachedMinimumSizeHint;
    QString m_prefix;
    QString m_suffix;
};

class FYGUI_EXPORT SpecialValueDoubleSpinBox : public QDoubleSpinBox
{
    Q_OBJECT

public:
    using QDoubleSpinBox::QDoubleSpinBox;

    QString specialValue(double val);
    void addSpecialValue(double val, const QString& text);

    void setSpecialPrefix(const QString& prefix);
    void setSpecialSuffix(const QString& suffix);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    [[nodiscard]] double valueFromText(const QString& text) const override;
    [[nodiscard]] QString textFromValue(double val) const override;

private:
    void updateSize();

    std::unordered_map<double, QString> m_specialValues;
    mutable QSize m_cachedSizeHint;
    mutable QSize m_cachedMinimumSizeHint;
    QString m_prefix;
    QString m_suffix;
};
} // namespace Fooyin
