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

#include <QObject>
#include <QPoint>

#include <optional>

class QColor;
class QMenu;
class QPainter;
class QPalette;
class QRectF;
class QWidget;

namespace Fooyin {
class ABRepeatControlsPrivate;
class ActionManager;
class PlayerController;
class SettingsManager;
class Track;

class FYGUI_EXPORT ABRepeatControls : public QObject
{
    Q_OBJECT

public:
    ABRepeatControls(PlayerController* playerController, SettingsManager* settings, QWidget* widget);
    ~ABRepeatControls() override;

    static QColor defaultColourA(const QPalette& palette);
    static QColor defaultColourB(const QPalette& palette);

    void setTrack(const Track& track);
    void paint(QPainter& painter, const QRectF& area, Qt::Orientation orientation = Qt::Horizontal,
               bool inverted = false);
    void populateMenu(QMenu* menu, ActionManager* actionManager, std::optional<QPoint> position = {});

    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    std::unique_ptr<ABRepeatControlsPrivate> p;
};
} // namespace Fooyin
