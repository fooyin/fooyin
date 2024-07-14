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
class FFmpegDecoderPrivate;

class FFmpegDecoder : public AudioDecoder
{
public:
    FFmpegDecoder();
    ~FFmpegDecoder() override;

    [[nodiscard]] QStringList supportedExtensions() const override;

    bool init(const QString& source) override;

    void start() override;
    void stop() override;

    [[nodiscard]] bool isSeekable() const override;
    void seek(uint64_t pos) override;

    AudioBuffer readBuffer(size_t bytes) override;

    [[nodiscard]] AudioFormat format() const override;
    [[nodiscard]] Error error() const override;

    static QStringList extensions();

private:
    std::unique_ptr<FFmpegDecoderPrivate> p;
};
} // namespace Fooyin
