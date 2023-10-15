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

#include "fycore_export.h"

#include <core/player/playermanager.h>

#include <QObject>

namespace Fy {
namespace Utils {
class SettingsManager;
}

namespace Core {
class Track;

namespace Player {
class FYCORE_EXPORT PlayerController : public PlayerManager
{
    Q_OBJECT

public:
    explicit PlayerController(Utils::SettingsManager* settings, QObject* parent = nullptr);
    ~PlayerController();

    [[nodiscard]] PlayState playState() const override;
    [[nodiscard]] PlayMode playMode() const override;
    [[nodiscard]] uint64_t currentPosition() const override;
    [[nodiscard]] Track currentTrack() const override;

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
    void changeCurrentTrack(const Track& track) override;
    void setPlayMode(PlayMode mode) override;

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Player
} // namespace Core
} // namespace Fy
