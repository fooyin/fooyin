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

#include <QFont>
#include <QWidget>

#include <concepts>
#include <initializer_list>
#include <ranges>

class QCheckBox;
class QIcon;
class QPushButton;

namespace Fooyin {
class FYGUI_EXPORT FontButton : public QWidget
{
    Q_OBJECT

public:
    explicit FontButton(QWidget* parent = nullptr);
    explicit FontButton(const QString& text, QWidget* parent = nullptr);
    explicit FontButton(const QString& text, bool checkable, QWidget* parent = nullptr);
    explicit FontButton(const QIcon& icon, const QString& text, QWidget* parent = nullptr);
    explicit FontButton(const QIcon& icon, const QString& text, bool checkable, QWidget* parent = nullptr);

    [[nodiscard]] QFont buttonFont() const;
    [[nodiscard]] bool fontChanged() const;

    [[nodiscard]] bool isCheckable() const;
    [[nodiscard]] bool isChecked() const;

    [[nodiscard]] int labelWidthHint() const;
    [[nodiscard]] QString labelText() const;

    void setButtonFont(const QFont& font);
    void setButtonFont(const QString& font);
    void setCheckable(bool checkable);
    void setChecked(bool checked);
    void setLabelText(const QString& text);
    void setLabelWidth(int width);
    void setToolTip(const QString& text);

    static int alignLabels(std::initializer_list<FontButton*> buttons, int minimumWidth = 0)
    {
        return alignLabels<std::initializer_list<FontButton*>>(std::move(buttons), minimumWidth);
    }
    template <std::ranges::forward_range R>
        requires std::convertible_to<std::ranges::range_reference_t<R>, FontButton*>
    static int alignLabels(R&& buttons, int minimumWidth = 0)
    {
        int width{minimumWidth};
        for(const FontButton* button : buttons) {
            width = std::max(width, button->labelWidthHint());
        }
        for(FontButton* button : buttons) {
            button->setLabelWidth(width);
        }
        return width;
    }

Q_SIGNALS:
    void toggled(bool checked);

private:
    void pickFont();
    void updateText();
    void updateButtonState();

    QFont m_font;
    bool m_changed;
    QCheckBox* m_checkBox;
    QPushButton* m_button;
};
} // namespace Fooyin
