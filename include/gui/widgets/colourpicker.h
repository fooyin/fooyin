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
#include <QWidget>

#include <memory>

namespace Fooyin {
class ColourPickerPrivate;

class FYGUI_EXPORT ColourPicker : public QWidget
{
    Q_OBJECT

public:
    explicit ColourPicker(QWidget* parent = nullptr);
    ~ColourPicker() override;

    [[nodiscard]] QColor colour() const;

    void setColour(const QColor& colour);
    void setOriginalColour(const QColor& colour);

    static void addToHistory(const QColor& colour);

Q_SIGNALS:
    void colourChanged(const QColor& colour);

private:
    std::unique_ptr<ColourPickerPrivate> p;
};
} // namespace Fooyin
