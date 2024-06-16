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

#include <core/playlist/playlist.h>
#include <gui/fywidget.h>

class QHBoxLayout;

namespace Fooyin {
class PlayerController;
class SettingsManager;
class ToolButton;

class PlaylistControl : public FyWidget
{
    Q_OBJECT

public:
    explicit PlaylistControl(PlayerController* playerController, SettingsManager* settings, QWidget* parent = nullptr);

    QString name() const override;
    QString layoutName() const override;

private:
    void updateButtonStyle() const;
    void setupMenus();
    void shuffleClicked() const;
    void setMode(Playlist::PlayModes mode) const;

    PlayerController* m_playerController;
    SettingsManager* m_settings;

    ToolButton* m_repeat;
    ToolButton* m_shuffle;
};
} // namespace Fooyin
