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

namespace Fooyin {
class SettingsManager;
class ClickableLabel;
class Slider;
class Track;
class PlayerManager;
enum class PlayState;

class ProgressWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ProgressWidget(PlayerManager* playerManager, SettingsManager* settings, QWidget* parent = nullptr);

signals:
    void movedSlider(int pos);

private:
    void reset();

    void changeTrack(const Track& track);

    void setCurrentPosition(uint64_t ms);
    void updateTime(uint64_t elapsed);

    void stateChanged(PlayState state);
    void toggleRemaining();
    void sliderDropped();

    PlayerManager* m_playerManager;
    SettingsManager* m_settings;

    QHBoxLayout* m_layout;
    Slider* m_slider;
    ClickableLabel* m_elapsed;
    ClickableLabel* m_total;
    uint64_t m_max;
    bool m_elapsedTotal;
};
} // namespace Fooyin
