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
#include <vector>

class QGradient;
class QLinearGradient;
class QScrollArea;
class QVBoxLayout;

namespace Fooyin {
FYGUI_EXPORT void setGradientColours(QGradient& gradient, const std::vector<QColor>& colours);
FYGUI_EXPORT QLinearGradient linearGradient(const QRectF& rect, Qt::Orientation orientation,
                                            const std::vector<QColor>& colours);

class ColourButton;
class GradientEditorPrivate;

class FYGUI_EXPORT GradientEditor : public QWidget
{
    Q_OBJECT

public:
    explicit GradientEditor(QWidget* parent = nullptr);
    ~GradientEditor() override;

    [[nodiscard]] std::vector<QColor> colours() const;
    [[nodiscard]] Qt::Orientation orientation() const;
    [[nodiscard]] bool isOrientationControlVisible() const;

    void setColours(const std::vector<QColor>& colours);
    void setOrientation(Qt::Orientation orientation);
    void setOrientationControlVisible(bool visible);

Q_SIGNALS:
    void coloursChanged();
    void orientationChanged(Qt::Orientation orientation);

private:
    std::unique_ptr<GradientEditorPrivate> p;
};
} // namespace Fooyin
