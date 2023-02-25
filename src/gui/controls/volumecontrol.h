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
class ComboIcon;
class Slider;
class HoverMenu;
} // namespace Utils

namespace Core::Player {
class PlayerManager;
}

namespace Gui::Widgets {
class VolumeControl : public QWidget
{
    Q_OBJECT

public:
    explicit VolumeControl(Core::Player::PlayerManager* playerManager, QWidget* parent = nullptr);
    ~VolumeControl() override;

    void setupUi();

    void updateVolume(double value);
    void mute();

signals:
    void volumeUp();
    void volumeDown();
    void volumeChanged(double value);

protected:
    void showVolumeMenu();

private:
    Core::Player::PlayerManager* m_playerManager;

    QHBoxLayout* m_layout;
    Utils::ComboIcon* m_volumeIcon;
    Utils::Slider* m_volumeSlider;
    QHBoxLayout* m_volumeLayout;
    Utils::HoverMenu* m_volumeMenu;
    QSize m_labelSize;
    int m_prevValue;
};
} // namespace Gui::Widgets
} // namespace Fy
