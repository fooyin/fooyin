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

#include "pipewirecontext.h"
#include "pipewirecore.h"
#include "pipewireregistry.h"
#include "pipewirestream.h"
#include "pipewirethreadloop.h"

#include <memory>

namespace Fooyin::Pipewire {
class PipeWireOutput : public AudioOutput
{
public:
    bool init(const AudioFormat& format) override;
    void uninit() override;
    void reset() override;
    void start() override;
    void drain() override;

    [[nodiscard]] bool initialised() const override;
    [[nodiscard]] QString device() const override;
    [[nodiscard]] OutputDevices getAllDevices() override;

    OutputState currentState() override;
    [[nodiscard]] int bufferSize() const override;
    int write(const AudioBuffer& buffer) override;
    void setPaused(bool pause) override;

    void setVolume(double volume) override;
    void setDevice(const QString& device) override;

    [[nodiscard]] QString error() const override;
    [[nodiscard]] AudioFormat format() const override;

private:
    bool initCore();
    bool initStream();
    static void process(void* userData);
    static void handleStateChanged(void* userdata, pw_stream_state old, pw_stream_state state, const char* /*error*/);
    static void drained(void* userdata);

    QString m_device;
    float m_volume{1.0};
    AudioFormat m_format;

    AudioBuffer m_buffer;
    uint32_t m_bufferPos{0};

    std::unique_ptr<PipewireThreadLoop> m_loop;
    std::unique_ptr<PipewireContext> m_context;
    std::unique_ptr<PipewireCore> m_core;
    std::unique_ptr<PipewireStream> m_stream;
    std::unique_ptr<PipewireRegistry> m_registry;
};
} // namespace Fooyin::Pipewire
