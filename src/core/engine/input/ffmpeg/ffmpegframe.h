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

#include "fycore_export.h"

#include <QSharedDataPointer>

#if defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#endif

extern "C"
{
#include <libavutil/frame.h>
}

#if defined(__GNUG__)
#pragma GCC diagnostic pop
#elif defined(__clang__)
#pragma clang diagnostic pop
#endif

namespace Fooyin {
class FramePrivate;

class FYCORE_EXPORT Frame
{
public:
    Frame();
    explicit Frame(AVRational timeBase);
    ~Frame();

    Frame(const Frame& other);
    Frame& operator=(const Frame& other);
    Frame(Frame&& other) noexcept;

    [[nodiscard]] bool isValid() const;

    [[nodiscard]] AVFrame* avFrame() const;

    [[nodiscard]] int channelCount() const;
    [[nodiscard]] int sampleRate() const;
    [[nodiscard]] AVSampleFormat format() const;

    [[nodiscard]] int sampleCount() const;

    [[nodiscard]] uint64_t ptsMs() const;
    [[nodiscard]] uint64_t durationMs() const;

    [[nodiscard]] uint64_t end() const;

private:
    QSharedDataPointer<FramePrivate> p;
};
} // namespace Fooyin
