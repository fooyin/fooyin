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

#include <core/engine/audiobuffer.h>

namespace Fooyin {
class AudioDecoder
{
public:
    enum class Error : uint32_t
    {
        NoError,
        ResourceError,
        FormatError,
        AccessDeniedError,
        NotSupportedError
    };

    virtual ~AudioDecoder() = default;

    [[nodiscard]] virtual QStringList supportedExtensions() const = 0;

    virtual bool init(const QString& source) = 0;
    virtual void start()                     = 0;
    virtual void stop()                      = 0;

    [[nodiscard]] virtual bool isSeekable() const = 0;
    virtual void seek(uint64_t pos)               = 0;

    virtual AudioBuffer readBuffer(size_t bytes) = 0;

    [[nodiscard]] virtual AudioFormat format() const = 0;
    [[nodiscard]] virtual Error error() const        = 0;
};
using DecoderCreator = std::function<std::unique_ptr<AudioDecoder>()>;
} // namespace Fooyin
