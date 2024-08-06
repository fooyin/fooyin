/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/engine/audiooutput.h>

#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_events.h>

#include <QBasicTimer>
#include <QString>

namespace Fooyin::Sdl {
class SdlOutput : public AudioOutput
{
public:
    SdlOutput();

    bool init(const AudioFormat& format) override;
    void uninit() override;
    void reset() override;
    void start() override;
    void drain() override;

    [[nodiscard]] bool initialised() const override;
    [[nodiscard]] QString device() const override;
    [[nodiscard]] int bufferSize() const override;
    OutputState currentState() override;
    [[nodiscard]] OutputDevices getAllDevices(bool isCurrentOutput) override;

    int write(const AudioBuffer& buffer) override;
    void setPaused(bool pause) override;
    void setVolume(double volume) override;
    void setDevice(const QString& device) override;

    [[nodiscard]] QString error() const override;
    [[nodiscard]] AudioFormat format() const override;

protected:
    void timerEvent(QTimerEvent* event) override;

private:
    void checkEvents();

    AudioFormat m_format;
    int m_bufferSize;
    bool m_initialised;
    QString m_device;
    double m_volume;

    SDL_AudioSpec m_desiredSpec;
    SDL_AudioSpec m_obtainedSpec;
    SDL_AudioDeviceID m_audioDeviceId;

    SDL_Event m_event;
    QBasicTimer m_eventTimer;
};
} // namespace Fooyin::Sdl
