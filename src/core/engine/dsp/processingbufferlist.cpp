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

#include <core/engine/dsp/processingbufferlist.h>

namespace Fooyin {
size_t ProcessingBufferList::count() const
{
    return m_chunks.size();
}

ProcessingBuffer* ProcessingBufferList::item(size_t index)
{
    if(index >= m_chunks.size()) {
        return nullptr;
    }

    return &m_chunks[index];
}

const ProcessingBuffer* ProcessingBufferList::item(size_t index) const
{
    if(index >= m_chunks.size()) {
        return nullptr;
    }

    return &m_chunks[index];
}

void ProcessingBufferList::clear()
{
    m_chunks.clear();
}

void ProcessingBufferList::removeByIdx(size_t index)
{
    if(index >= m_chunks.size()) {
        return;
    }

    m_chunks.erase(m_chunks.begin() + static_cast<ptrdiff_t>(index));
}

ProcessingBuffer* ProcessingBufferList::insertItem(size_t index, const AudioFormat& format, uint64_t startTimeNs,
                                                   size_t sampleCount)
{
    ProcessingBuffer buffer{format, startTimeNs};

    if(sampleCount > 0) {
        buffer.resizeSamples(sampleCount);
    }

    if(index >= m_chunks.size()) {
        m_chunks.push_back(std::move(buffer));
        return &m_chunks.back();
    }

    m_chunks.insert(m_chunks.begin() + static_cast<ptrdiff_t>(index), std::move(buffer));

    return &m_chunks[index];
}

ProcessingBuffer* ProcessingBufferList::addItem(const AudioFormat& format, uint64_t startTimeNs, size_t sampleCount)
{
    return insertItem(m_chunks.size(), format, startTimeNs, sampleCount);
}

void ProcessingBufferList::addChunk(const ProcessingBuffer& buffer)
{
    m_chunks.push_back(buffer);
}

void ProcessingBufferList::setToSingle(const ProcessingBuffer& buffer)
{
    if(m_chunks.empty()) {
        m_chunks.push_back(buffer);
        return;
    }

    m_chunks.resize(1);
    m_chunks[0] = buffer;
}
} // namespace Fooyin
