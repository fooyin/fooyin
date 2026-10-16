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

#include <gui/widgets/backgroundconfig.h>

#include <QObject>

class QPainter;
class QPixmap;
class QWidget;

namespace Fooyin {
class WidgetBackgroundPrivate;

class FYGUI_EXPORT WidgetBackground : public QObject
{
public:
    explicit WidgetBackground(QWidget* viewport);

    void setConfig(const BackgroundConfig& config);
    void setPixmap(const QPixmap& pixmap);

    void draw(QPainter& painter);

    [[nodiscard]] bool hasImage() const;

    bool eventFilter(QObject* watched, QEvent* event) override;

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    std::unique_ptr<WidgetBackgroundPrivate> p;
};
} // namespace Fooyin
