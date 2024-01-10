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

extern "C"
{
#include <libavcodec/packet.h>
}

#include <memory>

class AVPacket;

namespace Fooyin {
struct PacketDeleter
{
    void operator()(AVPacket* packet) const
    {
        if(packet) {
            av_packet_free(&packet);
        }
    }
};
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;

class Packet
{
public:
    Packet() = default;
    explicit Packet(PacketPtr packet);

    Packet(Packet&& other) noexcept;
    Packet(const Packet& other)            = delete;
    Packet& operator=(const Packet& other) = delete;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] AVPacket* avPacket() const;

private:
    PacketPtr m_packet;
};
} // namespace Fooyin
