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

#pragma once

#include "pipewirecore.h"

#include <pipewire/stream.h>

#include <memory>
#include <vector>

#include <QString>

namespace Fooyin {
class AudioFormat;

namespace Pipewire {
class PipewireStream
{
public:
    PipewireStream(PipewireCore* core, const AudioFormat& format, const QString& device = {});
    ~PipewireStream();

    pw_stream_state state();

    void setActive(bool active);
    void setVolume(float volume);

    pw_buffer* dequeueBuffer();
    void queueBuffer(pw_buffer* buffer);
    void flush(bool drain);

    bool connect(uint32_t id, const pw_direction& direction, std::vector<const spa_pod*>& params,
                 const pw_stream_flags& flags);
    void addListener(const pw_stream_events& streamEvents, void* data);

private:
    struct PwStreamDeleter
    {
        void operator()(pw_stream* stream) const
        {
            if(stream) {
                pw_stream_disconnect(stream);
                pw_stream_destroy(stream);
            }
        }
    };
    using PwStreamUPtr = std::unique_ptr<pw_stream, PwStreamDeleter>;

    spa_hook m_streamListener;
    PwStreamUPtr m_stream;
};
} // namespace Pipewire
} // namespace Fooyin
