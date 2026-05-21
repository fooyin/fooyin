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

#include <core/player/playerdefs.h>
#include <gui/fywidget.h>

namespace Fooyin {
class ActionManager;
class PlayerController;
class SettingsManager;
class ToolButton;

class PlayerControl : public FyWidget
{
    Q_OBJECT

public:
    PlayerControl(ActionManager* actionManager, PlayerController* playerController, SettingsManager* settings,
                  QWidget* parent = nullptr);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] QString layoutName() const override;

private:
    void updateButtonStyle() const;
    void updateIcons() const;
    void stateChanged(Player::PlayState state) const;

    ActionManager* m_actionManager;
    PlayerController* m_playerController;
    SettingsManager* m_settings;

    ToolButton* m_stop;
    ToolButton* m_prev;
    ToolButton* m_playPause;
    ToolButton* m_next;
};
} // namespace Fooyin
