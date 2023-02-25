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

#include <QWidget>

class QHBoxLayout;

namespace Fy {

namespace Utils {
class SettingsManager;
}

namespace Core {
class Track;

namespace Player {
class PlayerManager;
}
} // namespace Core

namespace Gui::Widgets {
class PlayerControl;
class PlaylistControl;
class VolumeControl;
class ProgressWidget;

class ControlWidget : public FyWidget
{
    Q_OBJECT

public:
    explicit ControlWidget(Core::Player::PlayerManager* playerManager, Utils::SettingsManager* settings,
                           QWidget* parent = nullptr);

    void setupUi();
    void setupConnections();

    [[nodiscard]] QString name() const override;

private:
    Core::Player::PlayerManager* m_playerManager;
    Utils::SettingsManager* m_settings;

    QHBoxLayout* m_layout;
    PlayerControl* m_playerControls;
    PlaylistControl* m_playlistControls;
    VolumeControl* m_volumeControls;
    ProgressWidget* m_progress;
};
} // namespace Gui::Widgets
} // namespace Fy
