/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include <jack/jack.h>
#include <jack/ringbuffer.h>
#include <jack/types.h>

#include <array>
#include <atomic>

namespace Fooyin::Jack {
class JackOutput : public AudioOutput
{
public:
    JackOutput();
    ~JackOutput() override;

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

    int write(std::span<const std::byte> data, int frameCount) override;
    void setPaused(bool pause) override;
    [[nodiscard]] bool supportsVolumeControl() const override;
    void setDevice(const QString& device) override;

    [[nodiscard]] QString error() const override;
    [[nodiscard]] AudioFormat format() const override;

private:
    static int sampleRateCallback(jack_nframes_t rate, void* arg);
    static int process(jack_nframes_t nframes, void* arg);
    static void shutdownCallback(void* arg);

    bool createPorts(int portCount);
    bool createRingbuffer();
    bool connectOutports();
    void closeClient();
    void checkEvents();
    [[nodiscard]] jack_nframes_t availableReadSpace() const noexcept;
    [[nodiscard]] jack_nframes_t availableWriteSpace() const noexcept;
    [[nodiscard]] int channelCount() const noexcept;
    void setError(const QString& message);

    AudioFormat m_format;
    bool m_initialised;
    bool m_active;
    QString m_device;
    QString m_error;
    bool m_failureReported;
    std::atomic_bool m_serverStopped;
    std::atomic_int m_serverRate;
    std::atomic_bool m_paused;
    std::atomic_bool m_resetting;
    std::atomic_bool m_processing;

    struct ClientHandle
    {
        jack_client_t* client{nullptr};

        explicit ClientHandle(jack_client_t* client_)
            : client{client_}
        { }

        ~ClientHandle()
        {
            if(client) {
                jack_client_close(client);
            }
        }

        ClientHandle(const ClientHandle&)            = delete;
        ClientHandle& operator=(const ClientHandle&) = delete;

        explicit operator bool() const
        {
            return client != nullptr;
        }
    };

    jack_client_t* m_client;
    int m_numPorts;
    int m_targetBufferFrames;
    std::array<jack_port_t*, AudioFormat::MaxChannels> m_ports;
    jack_ringbuffer_t* m_ringbuffer;
};
} // namespace Fooyin::Jack
