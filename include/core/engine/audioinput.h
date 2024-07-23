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

#include "fycore_export.h"

#include <core/engine/audiobuffer.h>
#include <core/track.h>

#include <QObject>

namespace Fooyin {
class FYCORE_EXPORT AudioInput
{
    Q_GADGET

public:
    enum DecoderFlag : uint8_t
    {
        None              = 0,
        NoSeeking         = 1 << 0,
        NoLooping         = 1 << 1,
        NoInfiniteLooping = 1 << 2,
    };
    Q_DECLARE_FLAGS(DecoderOptions, DecoderFlag)
    Q_FLAG(DecoderOptions)

    enum WriteFlag : uint8_t
    {
        Metadata  = 0,
        Rating    = 1 << 0,
        Playcount = 1 << 1,
    };
    Q_DECLARE_FLAGS(WriteOptions, WriteFlag)

    enum class Error : uint8_t
    {
        NoError = 0,
        ResourceError,
        FormatError,
        AccessDeniedError,
        NotSupportedError
    };

    virtual ~AudioInput() = default;

    [[nodiscard]] virtual QStringList supportedExtensions() const = 0;
    [[nodiscard]] virtual bool canReadCover() const               = 0;
    [[nodiscard]] virtual bool canWriteMetaData() const           = 0;
    [[nodiscard]] virtual bool isSeekable() const                 = 0;

    virtual bool init(const QString& source, DecoderOptions options) = 0;
    virtual void start()                                             = 0;
    virtual void stop()                                              = 0;

    virtual void seek(uint64_t pos) = 0;

    virtual AudioBuffer readBuffer(size_t bytes) = 0;

    [[nodiscard]] virtual bool readMetaData(Track& track) = 0;
    [[nodiscard]] virtual QByteArray readCover(const Track& track, Track::Cover cover);
    [[nodiscard]] virtual bool writeMetaData(const Track& track, WriteOptions options);

    [[nodiscard]] virtual AudioFormat format() const = 0;
    [[nodiscard]] virtual Error error() const        = 0;
};
using InputCreator = std::function<std::unique_ptr<AudioInput>()>;
} // namespace Fooyin

Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::AudioInput::DecoderOptions)
Q_DECLARE_OPERATORS_FOR_FLAGS(Fooyin::AudioInput::WriteOptions)
