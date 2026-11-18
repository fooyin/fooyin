/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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
#include <QDialog>

namespace Fooyin {
class ColourPicker;

class FYGUI_EXPORT ColourPickerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ColourPickerDialog(const QColor& colour, QWidget* parent = nullptr);

    [[nodiscard]] QColor colour() const;

Q_SIGNALS:
    void colourSelected(const QColor& colour);

private:
    ColourPicker* m_picker;
};
} // namespace Fooyin
