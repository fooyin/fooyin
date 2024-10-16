/*
 * Fooyin
 * Copyright © 2022, Luke Taylor <LukeT1@proton.me>
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

namespace Fooyin {
class SettingsManager;
class PlayerController;
class StatusWidgetPrivate;
class TrackSelectionController;

class StatusWidget : public FyWidget
{
    Q_OBJECT

public:
    StatusWidget(PlayerController* playerController, TrackSelectionController* selectionController,
                 SettingsManager* settings, QWidget* parent = nullptr);
    ~StatusWidget() override;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

    static QString defaultPlayingScript();
    static QString defaultSelectionScript();

    void showMessage(const QString& message);
    void showTempMessage(const QString& message);
    void showTempMessage(const QString& message, int timeout);
    void showStatusTip(const QString& message);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

signals:
    void clicked();

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void timerEvent(QTimerEvent* event) override;

private:
    std::unique_ptr<StatusWidgetPrivate> p;
};
} // namespace Fooyin
