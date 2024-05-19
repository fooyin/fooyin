/*
 * Fooyin
 * Copyright © 2024, Luke Taylor <LukeT1@proton.me>
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

#include "pipewirestream.h"

#include <core/engine/audiobuffer.h>

#include <pipewire/keys.h>
#include <spa/param/props.h>

#include <QDebug>

#ifdef __clang__
#pragma clang diagnostic ignored "-Wgnu-statement-expression-from-macro-expansion"
#endif

constexpr auto BufferSize = 8192;

namespace Fooyin::Pipewire {
PipewireStream::PipewireStream(PipewireCore* core, const AudioFormat& format, const QString& device)
{
    struct pw_properties* props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Playback",
                                                    PW_KEY_MEDIA_ROLE, "Music", PW_KEY_APP_ID, "fooyin",
                                                    PW_KEY_APP_ICON_NAME, "fooyin", PW_KEY_APP_NAME, "fooyin", nullptr);

    pw_properties_setf(props, PW_KEY_NODE_RATE, "1/%u", format.sampleRate());
    // pw_properties_setf(props, PW_KEY_NODE_LATENCY, "%u/%u", frames, format.sampleRate());

    if(!device.isEmpty()) {
        pw_properties_setf(props, PW_KEY_TARGET_OBJECT, "%s", device.toUtf8().constData());
    }

    m_stream.reset(pw_stream_new(core->core(), "Playback", props));

    if(!m_stream) {
        qWarning() << "[PW] Failed to create stream";
    }
}

PipewireStream::~PipewireStream()
{
    spa_hook_remove(&m_streamListener);
}

pw_stream_state PipewireStream::state()
{
    return pw_stream_get_state(m_stream.get(), nullptr);
}

int PipewireStream::bufferSize() const
{
    return BufferSize;
}

void PipewireStream::setActive(bool active)
{
    pw_stream_set_active(m_stream.get(), active);
}

void PipewireStream::setVolume(float volume)
{
    if(pw_stream_set_control(m_stream.get(), SPA_PROP_volume, 1, &volume, 0) < 0) {
        qDebug() << "[PW] Failed to set volume";
    }
}

pw_buffer* PipewireStream::dequeueBuffer()
{
    return pw_stream_dequeue_buffer(m_stream.get());
}

void PipewireStream::queueBuffer(pw_buffer* buffer)
{
    pw_stream_queue_buffer(m_stream.get(), buffer);
}

void PipewireStream::flush(bool drain)
{
    pw_stream_flush(m_stream.get(), drain);
}

bool PipewireStream::connect(uint32_t id, const spa_direction& direction, std::vector<const spa_pod*>& params,
                             const pw_stream_flags& flags)
{
    if(pw_stream_connect(m_stream.get(), direction, id, flags, params.data(), params.size()) < 0) {
        qWarning() << "[PW] Failed to connect to stream";
        return false;
    }

    return true;
}

void PipewireStream::addListener(const pw_stream_events& streamEvents, void* data)
{
    pw_stream_add_listener(m_stream.get(), &m_streamListener, &streamEvents, data);
}
} // namespace Fooyin::Pipewire
