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
#include <QVariant>
#include <QWidget>

#include <functional>
#include <memory>
#include <vector>

namespace Fooyin {
class GradientEditorPrivate;

class FYGUI_EXPORT GradientEditor : public QWidget
{
    Q_OBJECT

public:
    struct ColourStop
    {
        double position;
        QColor colour;
        QVariant data;
    };

    using ColourEditorFactory = std::function<QWidget*(int, const ColourStop&, QWidget*)>;

    explicit GradientEditor(QWidget* parent = nullptr);
    ~GradientEditor() override;

    [[nodiscard]] int selectedStop() const;
    [[nodiscard]] int colourEditorIndex(const QWidget* editor) const;
    [[nodiscard]] std::vector<ColourStop> colourStops() const;

    void setColourStops(const std::vector<ColourStop>& stops);
    void setColourEditorFactory(ColourEditorFactory factory);
    void updateColourStop(int index, const ColourStop& stop);
    void setSelectedStop(int index);
    void spaceStopsEvenly();

Q_SIGNALS:
    void stopsChanged();
    void selectedStopChanged(int index);

private:
    std::unique_ptr<GradientEditorPrivate> p;
};
} // namespace Fooyin
