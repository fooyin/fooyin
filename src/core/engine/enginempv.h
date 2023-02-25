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

#include "engine.h"

class mpv_event;
class mpv_handle;

namespace Fy::Core {
class Track;

namespace Engine {
class EngineMpv : public Engine
{
    Q_OBJECT

public:
    explicit EngineMpv(QObject* parent = nullptr);
    ~EngineMpv() override;

    void engineSetup();
    void processEvents();

    void play() override;
    void stop() override;
    void pause() override;
    void seek(uint64_t pos) override;
    void changeTrack(Track* track) override;
    void setVolume(float value) override;

signals:
    void mpvEvent();

private:
    void handleEvent(mpv_event* event);
    void handlePropertyChange(mpv_event* event);

    int m_posInterval;
    int m_ms;
    uint64_t m_lastTick;
    mpv_handle* m_mpv;
};
} // namespace Engine
} // namespace Fy::Core
