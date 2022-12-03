/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "core/gui/widgets/widget.h"

#include <QWidget>

class Track;

class ControlWidget : public Widget
{
    Q_OBJECT

public:
    explicit ControlWidget(QWidget* parent = nullptr);
    ~ControlWidget() override;

    void setupUi();
    void setupConnections();

    [[nodiscard]] QString name() const override;
    void layoutEditingMenu(QMenu* menu) override;

signals:
    void stop();

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

    void currentTrackChanged(Track* track);
    void currentPositionChanged(qint64 ms);

private:
    struct Private;
    std::unique_ptr<ControlWidget::Private> p;
};
