/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include <cstddef>
#include <vector>

namespace Fooyin {
/*!
 * Lightweight single-thread sample FIFO used in mixer scratch paths.
 *
 * Unlike `LockFreeRingBuffer`, this container is not thread-safe and is only
 * used from the audio thread for temporary buffering.
 */
class FYCORE_EXPORT SampleRingBuffer
{
public:
    SampleRingBuffer();

    //! Remove all buffered samples.
    void clear();

    //! Current number of buffered samples.
    [[nodiscard]] size_t size() const;
    //! Ensure capacity can hold at least `minCapacity` samples.
    void ensureCapacity(size_t minCapacity);

    //! Append `count` samples from `src`.
    void push(const double* src, size_t count);
    //! Append `count` zero-valued samples.
    void pushZeros(size_t count);
    //! Pop up to `count` samples into `dst`, returning actual popped count.
    size_t pop(double* dst, size_t count);

private:
    std::vector<double> m_buffer;
    size_t m_head;
    size_t m_size;
};
} // namespace Fooyin
