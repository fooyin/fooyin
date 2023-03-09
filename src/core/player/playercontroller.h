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

#include "playermanager.h"

#include <QObject>

namespace Fy {

namespace Utils {
class SettingsManager;
}

namespace Core::Player {
class PlayerController : public PlayerManager
{
    Q_OBJECT

public:
    explicit PlayerController(Utils::SettingsManager* settings, QObject* parent = nullptr);

protected:
    void reset() override;
    void play() override;
    void wakeUp() override;
    void playPause() override;
    void pause() override;
    void previous() override;
    void next() override;
    void stop() override;
    void setCurrentPosition(uint64_t ms) override;
    void changePosition(uint64_t ms) override;
    void changeCurrentTrack(Track* track) override;
    void setPlayMode(PlayMode mode) override;
    void volumeUp() override;
    void volumeDown() override;
    void setVolume(double value) override;

    [[nodiscard]] PlayState playState() const override;
    [[nodiscard]] PlayMode playMode() const override;
    [[nodiscard]] uint64_t currentPosition() const override;
    [[nodiscard]] Track* currentTrack() const override;
    [[nodiscard]] double volume() const override;

private:
    void changePlayMode();

    Utils::SettingsManager* m_settings;

    Track* m_currentTrack;
    uint64_t m_totalDuration;
    PlayState m_playStatus;
    PlayMode m_playMode;
    uint64_t m_position;
    double m_volume;
    bool m_counted;
};
} // namespace Core::Player
} // namespace Fy
