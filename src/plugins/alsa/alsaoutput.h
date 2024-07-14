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

#include "core/engine/audiooutput.h"

#include <alsa/asoundlib.h>

namespace Fooyin::Alsa {
struct PcmHandleDeleter
{
    void operator()(snd_pcm_t* handle) const
    {
        if(handle) {
            snd_pcm_close(handle);
        }
    }
};
using PcmHandleUPtr = std::unique_ptr<snd_pcm_t, PcmHandleDeleter>;

class AlsaOutput : public AudioOutput
{
public:
    AlsaOutput();
    ~AlsaOutput() override;

    bool init(const AudioFormat& format) override;
    void uninit() override;
    void reset() override;
    void start() override;
    void drain() override;

    [[nodiscard]] bool initialised() const override;
    [[nodiscard]] QString device() const override;
    [[nodiscard]] int bufferSize() const override;
    OutputState currentState() override;
    [[nodiscard]] OutputDevices getAllDevices() override;

    int write(const AudioBuffer& buffer) override;
    void setPaused(bool pause) override;
    void setVolume(double volume) override;
    void setDevice(const QString& device) override;

    [[nodiscard]] QString error() const override;

private:
    void resetAlsa();
    bool initAlsa();

    bool checkError(int error, const QString& message);
    [[nodiscard]] bool formatSupported(snd_pcm_format_t requestedFormat, snd_pcm_hw_params_t* hwParams);
    void getHardwareDevices(OutputDevices& devices);
    bool attemptRecovery(snd_pcm_status_t* status);
    bool recoverState(OutputState* state = nullptr);

    AudioFormat m_format;

    bool m_initialised;
    bool m_pausable;
    bool m_started;

    QString m_device;
    double m_volume;
    QString m_error;

    PcmHandleUPtr m_pcmHandle;
    snd_pcm_uframes_t m_bufferSize;
    snd_pcm_uframes_t m_periodSize;
};
} // namespace Fooyin::Alsa
