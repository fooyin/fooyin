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

#include <gui/brushsource.h>

#include <QWidget>

#include <memory>

class QResizeEvent;

namespace Fooyin {
class BrushSourceButtonPrivate;

class FYGUI_EXPORT BrushSourceButton : public QWidget
{
    Q_OBJECT

public:
    explicit BrushSourceButton(QWidget* parent = nullptr, bool gradients = false);
    ~BrushSourceButton() override;

    [[nodiscard]] const BrushSource& value() const;
    [[nodiscard]] bool choosingSource() const;

    void setValue(const BrushSource& brush, const QPalette& palette, QPalette::ColorGroup group);
    void setDirectionControlVisible(bool visible);
    void setSwatchOnly(bool swatchOnly);

    void openPicker();

Q_SIGNALS:
    void valueChanged();
    void editingStarted();
    void editingFinished(bool accepted);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    std::unique_ptr<BrushSourceButtonPrivate> p;
};
} // namespace Fooyin
