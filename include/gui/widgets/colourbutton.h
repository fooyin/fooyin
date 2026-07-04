/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <luket@pm.me>
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

#include <QColor>
#include <QWidget>

#include <concepts>
#include <initializer_list>
#include <ranges>

class QCheckBox;

namespace Fooyin {
class ColourSwatch;

class FYGUI_EXPORT ColourButton : public QWidget
{
    Q_OBJECT

public:
    explicit ColourButton(QWidget* parent = nullptr);
    explicit ColourButton(bool checkable, QWidget* parent = nullptr);
    explicit ColourButton(const QColor& colour, QWidget* parent = nullptr);
    explicit ColourButton(const QString& text, const QColor& colour, QWidget* parent = nullptr);
    explicit ColourButton(const QString& text, bool checkable, QWidget* parent = nullptr);
    explicit ColourButton(const QString& text, const QColor& colour, bool checkable, QWidget* parent = nullptr);

    [[nodiscard]] QColor colour() const;
    [[nodiscard]] bool colourChanged() const;

    [[nodiscard]] bool isCheckable() const;
    [[nodiscard]] bool isChecked() const;

    [[nodiscard]] int labelWidthHint() const;
    [[nodiscard]] QString text() const;

    void setColour(const QColor& colour);
    void setCheckable(bool checkable);
    void setChecked(bool checked);
    void setLabelWidth(int width);
    void setText(const QString& text);
    void setToolTip(const QString& text);

    static int alignLabels(std::initializer_list<ColourButton*> buttons, int minimumWidth = 0)
    {
        return alignLabels<std::initializer_list<ColourButton*>>(std::move(buttons), minimumWidth);
    }
    template <std::ranges::forward_range R>
        requires std::convertible_to<std::ranges::range_reference_t<R>, ColourButton*>
    static int alignLabels(R&& buttons, int minimumWidth = 0)
    {
        int width{minimumWidth};
        for(const ColourButton* button : buttons) {
            width = std::max(width, button->labelWidthHint());
        }
        for(ColourButton* button : buttons) {
            button->setLabelWidth(width);
        }
        return width;
    }

Q_SIGNALS:
    void clicked();
    void colourUpdated(const QColor& colour);
    void toggled(bool checked);

private:
    void pickColour();
    void updateButtonState();

    bool m_changed;
    QCheckBox* m_checkBox;
    ColourSwatch* m_swatch;
};
} // namespace Fooyin
