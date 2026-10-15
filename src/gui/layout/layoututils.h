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

#include <gui/fywidget.h>

#include <QCoreApplication>
#include <QPointer>
#include <QRect>
#include <QString>

namespace Fooyin {
class WidgetContainer;

enum class Placement : uint8_t
{
    Insert = 0,
    Fill,
    Split,
    Replace,
};

struct LayoutDropTarget
{
    QPointer<WidgetContainer> container;
    int index{-1};
    QRect preview;
    QString description;
    Placement placement{Placement::Insert};
    QPointer<FyWidget> target;
    Qt::Orientation orientation{Qt::Vertical};
    bool after{false};

    [[nodiscard]] bool isValid() const;

    [[nodiscard]] QRect previewGeometry(QWidget* root, QWidget* surface) const;
};

class LayoutUtils
{
    Q_DECLARE_TR_FUNCTIONS(Fooyin::LayoutDropTarget)

public:
    static QRect relativeGeometry(const QWidget* widget, const QWidget* root);
    static WidgetList layoutWidgets(WidgetContainer* root);
    static LayoutDropTarget resolveDropTarget(FyWidget* source, WidgetContainer* root, const QPoint& position,
                                              bool replace = false);
};
} // namespace Fooyin