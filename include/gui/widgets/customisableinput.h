/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <gui/widgets/expandableinputbox.h>

namespace Fooyin {
class CustomisableInputPrivate;

class FYGUI_EXPORT CustomisableInput : public ExpandableInput
{
    Q_OBJECT

public:
    enum StateFlag
    {
        FontChanged   = 1 << 0,
        ColourChanged = 1 << 1,
    };
    Q_DECLARE_FLAGS(State, StateFlag)

    explicit CustomisableInput(QWidget* parent = nullptr);
    explicit CustomisableInput(Attributes attributes, QWidget* parent = nullptr);
    ~CustomisableInput() override;

    [[nodiscard]] QString text() const override;
    [[nodiscard]] QFont font() const;
    [[nodiscard]] QColor colour() const;
    [[nodiscard]] State state() const;

    void setText(const QString& text) override;
    void setFont(const QFont& font);
    void setColour(const QColor& colour);
    void setState(State state);

    void setReadOnly(bool readOnly) override;

    void resetState();

private:
    std::unique_ptr<CustomisableInputPrivate> p;
};
} // namespace Fooyin
