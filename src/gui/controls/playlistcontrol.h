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

#include <QWidget>

class QHBoxLayout;

namespace Fy {

namespace Utils {
class SettingsManager;
class ComboIcon;
} // namespace Utils

namespace Core {
namespace Player {
class PlayerManager;
enum PlayMode : uint8_t;
} // namespace Player
} // namespace Core

namespace Gui::Widgets {
class PlaylistControl : public QWidget
{
    Q_OBJECT

public:
    explicit PlaylistControl(Core::Player::PlayerManager* playerManager, Utils::SettingsManager* settings,
                             QWidget* parent = nullptr);

    void setupUi();

    void playModeChanged(Core::Player::PlayMode mode);

signals:
    void repeatClicked();
    void shuffleClicked();

protected:
    void setMode(Core::Player::PlayMode mode) const;

private:
    Core::Player::PlayerManager* m_playerManager;
    Utils::SettingsManager* m_settings;

    QHBoxLayout* m_layout;
    QSize m_labelSize;
    Utils::ComboIcon* m_repeat;
    Utils::ComboIcon* m_shuffle;
};
} // namespace Gui::Widgets
} // namespace Fy
