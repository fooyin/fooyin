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

#include "ffmpegworker.h"

class AVFormatContext;

namespace Fy::Core::Engine::FFmpeg {
class AudioClock;
class Packet;
class Codec;
class Frame;

class Decoder : public EngineWorker
{
    Q_OBJECT

public:
    Decoder(AVFormatContext* context, const Codec& codec, int streamIndex, AudioClock* clock,
            QObject* parent = nullptr);
    ~Decoder();

    void seek(uint64_t position);

    void decode(Packet& packet);
    void onFrameProcessed();

    uint64_t position() const;

signals:
    void requestHandleFrame(Frame& frame);

private:
    bool canDoNextStep() const override;
    void doNextStep() override;

    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fy::Core::Engine::FFmpeg
