/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include "gui/fywidget.h"

#include <core/typedefs.h>

#include <QWidget>

class QHBoxLayout;

namespace Core::Player {
class PlayerManager;
} // namespace Core::Player

namespace Gui::Widgets {
class ClickableLabel;

class StatusWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit StatusWidget(Core::Player::PlayerManager* playerManager, QWidget* parent = nullptr);
    ~StatusWidget() override = default;

    [[nodiscard]] QString name() const override;

signals:
    void clicked();

protected:
    void setupUi();
    void contextMenuEvent(QContextMenuEvent* event) override;

    void labelClicked();
    void reloadStatus();
    void stateChanged(Core::Player::PlayState state);

private:
    Core::Player::PlayerManager* m_playerManager;

    QHBoxLayout* m_layout;
    ClickableLabel* m_iconLabel;
    QPixmap m_icon;
    ClickableLabel* m_playing;
};
} // namespace Gui::Widgets
