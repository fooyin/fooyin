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

#include <core/engine/audiodecoder.h>

namespace Fooyin {
class AudioFormat;
class AudioBuffer;

class FFmpegDecoder : public AudioDecoder
{
public:
    FFmpegDecoder();
    ~FFmpegDecoder() override;

    bool init(const QString& source) override;

    void start() override;
    void stop() override;

    AudioFormat format() const override;
    bool isSeekable() const;
    void seek(uint64_t pos) override;

    AudioBuffer readBuffer() override;

    Error error() const override;

private:
    struct Private;
    std::unique_ptr<Private> p;
};
} // namespace Fooyin
