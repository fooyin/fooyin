/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <QWidget>

class QLineEdit;

namespace Fy::Gui::Settings {
class PresetInput : public QWidget
{
public:
    enum StateFlag
    {
        FontChanged   = 1 << 0,
        ColourChanged = 1 << 1,
    };
    Q_DECLARE_FLAGS(State, StateFlag)

    explicit PresetInput(QWidget* parent = nullptr);

    [[nodiscard]] QString text() const;
    [[nodiscard]] QFont font() const;
    [[nodiscard]] QColor colour() const;
    [[nodiscard]] State state() const;

    void setReadOnly(bool readOnly);

    void setText(const QString& text);
    void setFont(const QFont& font);
    void setColour(const QColor& colour);
    void setState(State state);

    void resetState();

private:
    void showContextMenu(const QPoint& pos);
    void showFontDialog();
    void showColourDialog();

    QLineEdit* m_editBlock;

    QFont m_font;
    QColor m_colour;

    State m_state;
};
using PresetInputList = std::vector<PresetInput*>;

Q_DECLARE_OPERATORS_FOR_FLAGS(PresetInput::State)
} // namespace Fy::Gui::Settings
