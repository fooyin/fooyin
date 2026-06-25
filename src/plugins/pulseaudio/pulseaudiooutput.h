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

#include <pulse/pulseaudio.h>

#include <memory>

struct pa_context;
struct pa_operation;
struct pa_stream;
struct pa_threaded_mainloop;

namespace Fooyin::PulseAudio {
class PulseAudioOutput : public AudioOutput
{
public:
    PulseAudioOutput();
    ~PulseAudioOutput() override;

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
    void setVolume(double volume) override;
    [[nodiscard]] bool supportsVolumeControl() const override;
    void setDevice(const QString& device) override;

    [[nodiscard]] QString error() const override;
    [[nodiscard]] AudioFormat format() const override;

private:
    struct PaMainLoopDeleter
    {
        void operator()(pa_threaded_mainloop* loop) const
        {
            if(loop) {
                pa_threaded_mainloop_free(loop);
            }
        }
    };
    using PaMainLoopUPtr = std::unique_ptr<pa_threaded_mainloop, PaMainLoopDeleter>;

    struct PaContextDeleter
    {
        void operator()(pa_context* context) const
        {
            if(context) {
                pa_context_disconnect(context);
                pa_context_unref(context);
            }
        }
    };
    using PaContextUPtr = std::unique_ptr<pa_context, PaContextDeleter>;

    void resetPulse();
    void setError(const QString& message);
    void setPulseError(const QString& message);
    bool initContext();
    bool initPulse();

    static void contextStateCallback(pa_context* context, void* userdata);

    bool waitForOperation(pa_operation* op, pa_threaded_mainloop* mainloop, const QString& message = {});

    AudioFormat m_format;
    bool m_initialised;
    double m_volume;
    QString m_device;
    QString m_error;
    pa_stream* m_stream;
    int m_targetBufferFrames;
    PaMainLoopUPtr m_mainLoop;
    PaContextUPtr m_context;
};
} // namespace Fooyin::PulseAudio
