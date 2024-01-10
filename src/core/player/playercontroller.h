/*
 * Fooyin
 * Copyright 2022-2024, Luke Taylor <LukeT1@proton.me>
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

namespace Fooyin {
class SettingsManager;
class Track;

class FYCORE_EXPORT PlayerController : public PlayerManager
{
    Q_OBJECT

public:
    explicit PlayerController(SettingsManager* settings, QObject* parent = nullptr);
    ~PlayerController() override;

    [[nodiscard]] PlayState playState() const override;
    [[nodiscard]] Playlist::PlayModes playMode() const override;
    [[nodiscard]] uint64_t currentPosition() const override;
    [[nodiscard]] Track currentTrack() const override;

    void reset() override;
    void play() override;
    void playPause() override;
    void pause() override;
    void previous() override;
    void next() override;
    void stop() override;
    void setCurrentPosition(uint64_t ms) override;
    void changePosition(uint64_t ms) override;
    void changeCurrentTrack(const Track& track) override;
    void setPlayMode(Playlist::PlayModes mode) override;

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
