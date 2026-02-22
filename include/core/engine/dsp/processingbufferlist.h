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

#include <core/engine/dsp/processingbuffer.h>

#include <vector>

namespace Fooyin {
/*!
 * Mutable list of ProcessingBuffer chunks used across engine processing stages.
 */
class FYCORE_EXPORT ProcessingBufferList
{
public:
    [[nodiscard]] size_t count() const;

    ProcessingBuffer* item(size_t index);
    [[nodiscard]] const ProcessingBuffer* item(size_t index) const;

    void clear();
    void removeByIdx(size_t index);

    ProcessingBuffer* insertItem(size_t index, const AudioFormat& format, uint64_t startTimeNs, size_t sampleCount);
    ProcessingBuffer* addItem(const AudioFormat& format, uint64_t startTimeNs, size_t sampleCount);
    void addChunk(const ProcessingBuffer& buffer);
    void setToSingle(const ProcessingBuffer& buffer);

private:
    std::vector<ProcessingBuffer> m_chunks;
};
} // namespace Fooyin
