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

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>
#include <vector>

namespace Fooyin {
template <typename T>
concept RingBufferValue = std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>;

constexpr auto CacheLine = 64;

enum class RingBufferOverflowPolicy : uint8_t
{
    DropNewest = 0,
    OverwriteOldest
};

/*!
 * SPSC lock-free ring buffer.
 *
 * Thread safety:
 * - `DropNewest`: strict SPSC ownership
 *   - Writer mutates only `m_writePos`.
 *   - Reader mutates only `m_readPos`.
 * - `OverwriteOldest`: writer may also advance `m_readPos` to free space.
 * - Queries (readAvailable/writeAvailable/etc) are safe from any thread.
 * - requestReset() is safe from Reader and Writer thread.
 *
 * Reservation API:
 * - Single-element reserve/commit paths avoid producer-side copy.
 * - Reservation paths are `DropNewest`-only; they must not be used with overwrite semantics.
 * - `commitOne()` is the publish point for writes.
 * - `consumeOne()` is the retire point for reads.
 * - At most one in-flight reservation per side at a time.
 * - Outstanding reservations are invalidated by reset.
 * - Reset linearizes at generation increment.
 */
template <RingBufferValue T>
class LockFreeRingBuffer
{
public:
    struct WriteReservation
    {
        T* data{nullptr};
        size_t index{0};
        uint64_t generation{0};

        [[nodiscard]] explicit operator bool() const
        {
            return data != nullptr;
        }
    };

    struct ReadReservation
    {
        const T* data{nullptr};
        size_t index{0};
        uint64_t generation{0};

        [[nodiscard]] explicit operator bool() const
        {
            return data != nullptr;
        }
    };

    class Reader
    {
    public:
        Reader(const Reader&)                = delete;
        Reader& operator=(const Reader&)     = delete;
        Reader(Reader&&) noexcept            = default;
        Reader& operator=(Reader&&) noexcept = default;

        size_t read(T* data, size_t count)
        {
            return m_ring->readInternal(data, count);
        }

        size_t peek(T* data, size_t count)
        {
            return m_ring->peekInternal(data, count);
        }

        size_t skip(size_t count)
        {
            return m_ring->skipInternal(count);
        }

        [[nodiscard]] ReadReservation reserveOne()
        {
            return m_ring->reserveOneReadInternal();
        }

        void consumeOne(const ReadReservation& reservation)
        {
            m_ring->consumeOneReadInternal(reservation);
        }

        [[nodiscard]] size_t readAvailable()
        {
            return m_ring->readAvailable();
        }

        [[nodiscard]] bool empty()
        {
            return m_ring->empty();
        }

        [[nodiscard]] size_t capacity() const
        {
            return m_ring->capacity();
        }

        void requestReset()
        {
            m_ring->requestReset();
        }

        void reset()
        {
            m_ring->requestReset();
        }

    private:
        friend class LockFreeRingBuffer;

        explicit Reader(LockFreeRingBuffer& ring)
            : m_ring{&ring}
        { }

        LockFreeRingBuffer* m_ring;
    };

    class Writer
    {
    public:
        Writer(const Writer&)                = delete;
        Writer& operator=(const Writer&)     = delete;
        Writer(Writer&&) noexcept            = default;
        Writer& operator=(Writer&&) noexcept = default;

        size_t write(const T* data, size_t count,
                     RingBufferOverflowPolicy policy = RingBufferOverflowPolicy::DropNewest)
        {
            // `OverwriteOldest` is best-effort and may still return a short write
            return m_ring->writeInternal(data, count, policy);
        }

        [[nodiscard]] WriteReservation reserveOne(RingBufferOverflowPolicy policy
                                                  = RingBufferOverflowPolicy::DropNewest)
        {
            return m_ring->reserveOneWriteInternal(policy);
        }

        void commitOne(const WriteReservation& reservation)
        {
            m_ring->commitOneWriteInternal(reservation);
        }

        [[nodiscard]] size_t writeAvailable()
        {
            return m_ring->writeAvailable();
        }

        [[nodiscard]] size_t readAvailable()
        {
            return m_ring->readAvailable();
        }

        [[nodiscard]] bool empty()
        {
            return m_ring->empty();
        }

        [[nodiscard]] bool full()
        {
            return m_ring->full();
        }

        [[nodiscard]] size_t capacity() const
        {
            return m_ring->capacity();
        }

        void requestReset()
        {
            m_ring->requestReset();
        }

        void reset()
        {
            m_ring->requestReset();
        }

    private:
        friend class LockFreeRingBuffer;

        explicit Writer(LockFreeRingBuffer& ring)
            : m_ring{&ring}
        { }

        LockFreeRingBuffer* m_ring;
    };

    explicit LockFreeRingBuffer(size_t capacity)
        : m_buffer(capacity + 1)
        , m_capacity{capacity + 1}
        , m_writePos{0}
        , m_readPos{0}
        , m_resetRequested{false}
        , m_generation{0}
#ifndef NDEBUG
        , m_writerReservationActive{false}
        , m_readerReservationActive{false}
#endif
    { }

    LockFreeRingBuffer(LockFreeRingBuffer&& other) noexcept
        : m_buffer{std::move(other.m_buffer)}
        , m_capacity{std::exchange(other.m_capacity, 0)}
        , m_writePos{other.m_writePos.exchange(0, std::memory_order_relaxed)}
        , m_readPos{other.m_readPos.exchange(0, std::memory_order_relaxed)}
        , m_resetRequested{other.m_resetRequested.exchange(false, std::memory_order_relaxed)}
        , m_generation{other.m_generation.load(std::memory_order_relaxed)}
#ifndef NDEBUG
        , m_writerReservationActive{false}
        , m_readerReservationActive{false}
#endif
    { }

    LockFreeRingBuffer& operator=(LockFreeRingBuffer&& other) noexcept
    {
        if(this != &other) {
            m_buffer   = std::move(other.m_buffer);
            m_capacity = std::exchange(other.m_capacity, 0);
            m_writePos.store(other.m_writePos.exchange(0, std::memory_order_relaxed), std::memory_order_relaxed);
            m_readPos.store(other.m_readPos.exchange(0, std::memory_order_relaxed), std::memory_order_relaxed);
            m_resetRequested.store(other.m_resetRequested.exchange(false, std::memory_order_relaxed),
                                   std::memory_order_relaxed);
            m_generation.store(other.m_generation.load(std::memory_order_relaxed), std::memory_order_relaxed);
#ifndef NDEBUG
            m_writerReservationActive.store(false, std::memory_order_relaxed);
            m_readerReservationActive.store(false, std::memory_order_relaxed);
#endif
        }
        return *this;
    }

    [[nodiscard]] Reader reader()
    {
        return Reader{*this};
    }

    [[nodiscard]] Writer writer()
    {
        return Writer{*this};
    }

    //! Returns number of elements available for writing (instantaneous snapshot).
    [[nodiscard]] size_t writeAvailable() const
    {
        const size_t writePos = m_writePos.load(std::memory_order_acquire);
        const size_t readPos  = m_readPos.load(std::memory_order_acquire);

        if(writePos >= readPos) {
            return m_capacity - 1 - (writePos - readPos);
        }
        return readPos - writePos - 1;
    }

    //! Returns number of elements available for reading (instantaneous snapshot).
    [[nodiscard]] size_t readAvailable() const
    {
        const size_t writePos = m_writePos.load(std::memory_order_acquire);
        const size_t readPos  = m_readPos.load(std::memory_order_acquire);

        if(writePos >= readPos) {
            return writePos - readPos;
        }
        return m_capacity - readPos + writePos;
    }

    //! Returns total capacity (usable elements)
    [[nodiscard]] size_t capacity() const
    {
        return m_capacity - 1;
    }

    [[nodiscard]] bool empty() const
    {
        return readAvailable() == 0;
    }

    [[nodiscard]] bool full() const
    {
        return writeAvailable() == 0;
    }

    //! Request a reset to be applied on the next read/write operation.
    void requestReset()
    {
        m_resetRequested.store(true, std::memory_order_release);
    }

private:
    size_t writeInternal(const T* data, size_t count, RingBufferOverflowPolicy policy)
    {
        applyResetIfNeeded();

#ifndef NDEBUG
        if(policy == RingBufferOverflowPolicy::OverwriteOldest) {
            assert(!m_readerReservationActive.load(std::memory_order_relaxed)
                   && "LockFreeRingBuffer::writeInternal overwrite-oldest cannot be used while a read reservation is "
                      "active");
        }
#endif

        const size_t usableCapacity = capacity();
        if(!data || count == 0 || usableCapacity == 0) {
            return 0;
        }

        if(policy == RingBufferOverflowPolicy::OverwriteOldest && count > usableCapacity) {
            data += (count - usableCapacity);
            count = usableCapacity;
        }

        // In overwrite mode we try to free enough space first, but this remains
        // best-effort under concurrent reader/writer progress
        [[maybe_unused]] const bool hasRequestedCapacity = ensureWriteCapacityInternal(count, policy);

        const size_t available = writeAvailable();
        const size_t toWrite   = std::min(count, available);

        if(toWrite == 0) {
            return 0;
        }

        const size_t writePos   = m_writePos.load(std::memory_order_relaxed);
        const size_t firstPart  = std::min(toWrite, m_capacity - writePos);
        const size_t secondPart = toWrite - firstPart;

        std::memcpy(m_buffer.data() + writePos, data, firstPart * sizeof(T));

        if(secondPart > 0) {
            std::memcpy(m_buffer.data(), data + firstPart, secondPart * sizeof(T));
        }

        const size_t newWritePos = (writePos + toWrite) % m_capacity;
        m_writePos.store(newWritePos, std::memory_order_release);

        return toWrite;
    }

    [[nodiscard]] WriteReservation reserveOneWriteInternal(RingBufferOverflowPolicy policy)
    {
        applyResetIfNeeded();

#ifndef NDEBUG
        assert(!m_writerReservationActive.load(std::memory_order_relaxed)
               && "LockFreeRingBuffer writer reservation already active");
#endif

        if(policy == RingBufferOverflowPolicy::OverwriteOldest) {
            // Reservation/commit is intentionally DropNewest-only
            return {};
        }

        if(capacity() == 0) {
            return {};
        }

        const uint64_t generation = m_generation.load(std::memory_order_acquire);

        while(true) {
            const size_t writePos = m_writePos.load(std::memory_order_relaxed);
            size_t readPos        = m_readPos.load(std::memory_order_acquire);
            const size_t nextPos  = incrementInternal(writePos);

            if(nextPos == readPos) {
                return {};
            }

#ifndef NDEBUG
            const bool alreadyReserved = m_writerReservationActive.exchange(true, std::memory_order_relaxed);
            assert(!alreadyReserved && "LockFreeRingBuffer duplicate writer reservation");
#endif
            return WriteReservation{.data = m_buffer.data() + writePos, .index = writePos, .generation = generation};
        }
    }

    void commitOneWriteInternal(const WriteReservation& reservation)
    {
        if(!reservation) {
            return;
        }

        applyResetIfNeeded();

        const bool generationMatches = reservation.generation == m_generation.load(std::memory_order_acquire);

#ifndef NDEBUG
        const bool hadReservation = m_writerReservationActive.exchange(false, std::memory_order_relaxed);
        if(generationMatches) {
            assert(hadReservation && "LockFreeRingBuffer writer commit without reservation");
        }
#endif

        if(!generationMatches) {
            return;
        }

        const size_t writePos = m_writePos.load(std::memory_order_relaxed);
#ifndef NDEBUG
        assert(reservation.index == writePos && "LockFreeRingBuffer writer commit out of order");
#endif
        if(reservation.index != writePos) {
            return;
        }

        m_writePos.store(incrementInternal(reservation.index), std::memory_order_release);
    }

    size_t readInternal(T* data, size_t count)
    {
        applyResetIfNeeded();

        const size_t available = readAvailable();
        const size_t toRead    = std::min(count, available);

        if(toRead == 0) {
            return 0;
        }

        const size_t readPos    = m_readPos.load(std::memory_order_relaxed);
        const size_t firstPart  = std::min(toRead, m_capacity - readPos);
        const size_t secondPart = toRead - firstPart;

        std::memcpy(data, m_buffer.data() + readPos, firstPart * sizeof(T));

        if(secondPart > 0) {
            std::memcpy(data + firstPart, m_buffer.data(), secondPart * sizeof(T));
        }

        const size_t newReadPos = (readPos + toRead) % m_capacity;
        m_readPos.store(newReadPos, std::memory_order_release);

        return toRead;
    }

    [[nodiscard]] ReadReservation reserveOneReadInternal()
    {
        applyResetIfNeeded();

#ifndef NDEBUG
        assert(!m_readerReservationActive.load(std::memory_order_relaxed)
               && "LockFreeRingBuffer reader reservation already active");
#endif

        const size_t readPos  = m_readPos.load(std::memory_order_relaxed);
        const size_t writePos = m_writePos.load(std::memory_order_acquire);
        if(readPos == writePos) {
            return {};
        }

        const uint64_t generation = m_generation.load(std::memory_order_acquire);
#ifndef NDEBUG
        const bool alreadyReserved = m_readerReservationActive.exchange(true, std::memory_order_relaxed);
        assert(!alreadyReserved && "LockFreeRingBuffer duplicate reader reservation");
#endif
        return ReadReservation{.data = m_buffer.data() + readPos, .index = readPos, .generation = generation};
    }

    void consumeOneReadInternal(const ReadReservation& reservation)
    {
        if(!reservation) {
            return;
        }

        applyResetIfNeeded();

        const bool generationMatches = reservation.generation == m_generation.load(std::memory_order_acquire);

#ifndef NDEBUG
        const bool hadReservation = m_readerReservationActive.exchange(false, std::memory_order_relaxed);
        if(generationMatches) {
            assert(hadReservation && "LockFreeRingBuffer reader consume without reservation");
        }
#endif

        if(!generationMatches) {
            return;
        }

        const size_t readPos = m_readPos.load(std::memory_order_relaxed);
#ifndef NDEBUG
        assert(reservation.index == readPos && "LockFreeRingBuffer reader consume out of order");
#endif
        if(reservation.index != readPos) {
            return;
        }

        m_readPos.store(incrementInternal(reservation.index), std::memory_order_release);
    }

    size_t peekInternal(T* data, size_t count)
    {
        applyResetIfNeeded();

        const size_t available = readAvailable();
        const size_t toPeek    = std::min(count, available);

        if(toPeek == 0) {
            return 0;
        }

        const size_t readPos    = m_readPos.load(std::memory_order_relaxed);
        const size_t firstPart  = std::min(toPeek, m_capacity - readPos);
        const size_t secondPart = toPeek - firstPart;

        std::memcpy(data, m_buffer.data() + readPos, firstPart * sizeof(T));
        if(secondPart > 0) {
            std::memcpy(data + firstPart, m_buffer.data(), secondPart * sizeof(T));
        }

        return toPeek;
    }

    size_t skipInternal(size_t count)
    {
        applyResetIfNeeded();

        const size_t available = readAvailable();
        const size_t toSkip    = std::min(count, available);

        if(toSkip == 0) {
            return 0;
        }

        const size_t readPos    = m_readPos.load(std::memory_order_relaxed);
        const size_t newReadPos = (readPos + toSkip) % m_capacity;
        m_readPos.store(newReadPos, std::memory_order_release);

        return toSkip;
    }

    void resetInternal()
    {
        m_resetRequested.store(false, std::memory_order_release);
        m_generation.fetch_add(1, std::memory_order_acq_rel);
        m_writePos.store(0, std::memory_order_release);
        m_readPos.store(0, std::memory_order_release);
#ifndef NDEBUG
        m_writerReservationActive.store(false, std::memory_order_relaxed);
        m_readerReservationActive.store(false, std::memory_order_relaxed);
#endif
    }

    void applyResetIfNeeded()
    {
        if(m_resetRequested.exchange(false, std::memory_order_acq_rel)) {
            m_generation.fetch_add(1, std::memory_order_acq_rel);
            m_writePos.store(0, std::memory_order_release);
            m_readPos.store(0, std::memory_order_release);
#ifndef NDEBUG
            m_writerReservationActive.store(false, std::memory_order_relaxed);
            m_readerReservationActive.store(false, std::memory_order_relaxed);
#endif
        }
    }

    bool ensureWriteCapacityInternal(size_t required, RingBufferOverflowPolicy policy)
    {
        if(required == 0) {
            return true;
        }

        if(policy == RingBufferOverflowPolicy::DropNewest) {
            return writeAvailable() >= required;
        }

        size_t available = writeAvailable();
        while(available < required) {
            const size_t needToDrop = required - available;
            if(dropOldestInternal(needToDrop) == 0) {
                break;
            }
            available = writeAvailable();
        }

        return available >= required;
    }

    size_t dropOldestInternal(size_t count)
    {
        size_t dropped{0};

        while(dropped < count) {
            size_t readPos        = m_readPos.load(std::memory_order_acquire);
            const size_t writePos = m_writePos.load(std::memory_order_acquire);
            const size_t readable = distanceInternal(readPos, writePos);
            if(readable == 0) {
                break;
            }

            const size_t step       = std::min(count - dropped, readable);
            const size_t newReadPos = (readPos + step) % m_capacity;
            if(m_readPos.compare_exchange_weak(readPos, newReadPos, std::memory_order_acq_rel,
                                               std::memory_order_acquire)) {
                dropped += step;
            }
        }

        return dropped;
    }

    [[nodiscard]] size_t incrementInternal(size_t pos) const
    {
        return (pos + 1) % m_capacity;
    }

    [[nodiscard]] size_t distanceInternal(size_t from, size_t to) const
    {
        if(to >= from) {
            return to - from;
        }
        return m_capacity - from + to;
    }

    std::vector<T> m_buffer;
    size_t m_capacity;

    alignas(CacheLine) std::atomic<size_t> m_writePos;
    alignas(CacheLine) std::atomic<size_t> m_readPos;
    alignas(CacheLine) std::atomic<bool> m_resetRequested;
    alignas(CacheLine) std::atomic<uint64_t> m_generation;
#ifndef NDEBUG
    alignas(CacheLine) std::atomic<bool> m_writerReservationActive;
    alignas(CacheLine) std::atomic<bool> m_readerReservationActive;
#endif
};
} // namespace Fooyin
