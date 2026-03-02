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

#include <core/engine/lockfreeringbuffer.h>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>

namespace Fooyin::Testing {
TEST(LockFreeRingBufferTest, DropNewestKeepsExistingItemsWhenFull)
{
    LockFreeRingBuffer<int> ring{3};
    auto writer = ring.writer();
    auto reader = ring.reader();

    const std::array<int, 3> initial{1, 2, 3};
    EXPECT_EQ(writer.write(initial.data(), initial.size()), initial.size());

    const int next = 4;
    EXPECT_EQ(writer.write(&next, 1), 0U);

    std::array<int, 3> out{};
    EXPECT_EQ(reader.read(out.data(), out.size()), out.size());
    EXPECT_EQ(out, initial);
}

TEST(LockFreeRingBufferTest, OverwriteOldestDropsEarliestItems)
{
    LockFreeRingBuffer<int> ring{3};
    auto writer = ring.writer();
    auto reader = ring.reader();

    const std::array<int, 3> initial{1, 2, 3};
    const std::array<int, 2> update{4, 5};
    EXPECT_EQ(writer.write(initial.data(), initial.size()), initial.size());
    EXPECT_EQ(writer.write(update.data(), update.size(), RingBufferOverflowPolicy::OverwriteOldest), update.size());

    std::array<int, 3> out{};
    EXPECT_EQ(reader.read(out.data(), out.size()), out.size());
    EXPECT_EQ(out, (std::array<int, 3>{3, 4, 5}));
}

TEST(LockFreeRingBufferTest, OverwriteOldestLargeWriteKeepsNewestWindow)
{
    LockFreeRingBuffer<int> ring{3};
    auto writer = ring.writer();
    auto reader = ring.reader();

    const std::array<int, 2> initial{1, 2};
    const std::array<int, 4> update{3, 4, 5, 6};
    EXPECT_EQ(writer.write(initial.data(), initial.size()), initial.size());
    EXPECT_EQ(writer.write(update.data(), update.size(), RingBufferOverflowPolicy::OverwriteOldest), 3U);

    std::array<int, 3> out{};
    EXPECT_EQ(reader.read(out.data(), out.size()), out.size());
    EXPECT_EQ(out, (std::array<int, 3>{4, 5, 6}));
}

TEST(LockFreeRingBufferTest, ReserveAndCommitAvoidsWriteCopyForSingleItems)
{
    LockFreeRingBuffer<int> ring{2};
    auto writer = ring.writer();
    auto reader = ring.reader();

    const auto firstReservation = writer.reserveOne();
    ASSERT_TRUE(firstReservation);
    *firstReservation.data = 10;
    writer.commitOne(firstReservation);

    const auto secondReservation = writer.reserveOne();
    ASSERT_TRUE(secondReservation);
    *secondReservation.data = 20;
    writer.commitOne(secondReservation);

    EXPECT_FALSE(writer.reserveOne());

    std::array<int, 2> out{};
    EXPECT_EQ(reader.read(out.data(), out.size()), out.size());
    EXPECT_EQ(out, (std::array<int, 2>{10, 20}));
}

TEST(LockFreeRingBufferTest, ReserveOneRejectsOverwriteOldest)
{
    LockFreeRingBuffer<int> ring{2};
    auto writer = ring.writer();
    auto reader = ring.reader();

    const std::array<int, 2> initial{1, 2};
    EXPECT_EQ(writer.write(initial.data(), initial.size()), initial.size());

    const auto reservation = writer.reserveOne(RingBufferOverflowPolicy::OverwriteOldest);
    EXPECT_FALSE(reservation);

    std::array<int, 2> out{};
    EXPECT_EQ(reader.read(out.data(), out.size()), out.size());
    EXPECT_EQ(out, initial);
}

TEST(LockFreeRingBufferTest, StaleWriteReservationIsDroppedAfterReset)
{
    LockFreeRingBuffer<int> ring{2};
    auto writer = ring.writer();
    auto reader = ring.reader();

    const auto reservation = writer.reserveOne();
    ASSERT_TRUE(reservation);
    *reservation.data = 99;

    writer.requestReset();
    writer.commitOne(reservation);

    int out{0};
    EXPECT_EQ(reader.read(&out, 1), 0U);

    const auto freshReservation = writer.reserveOne();
    ASSERT_TRUE(freshReservation);
    *freshReservation.data = 7;
    writer.commitOne(freshReservation);

    EXPECT_EQ(reader.read(&out, 1), 1U);
    EXPECT_EQ(out, 7);
}

TEST(LockFreeRingBufferTest, StaleReadReservationIsDroppedAfterReset)
{
    LockFreeRingBuffer<int> ring{2};
    auto writer = ring.writer();
    auto reader = ring.reader();

    const int initial{42};
    EXPECT_EQ(writer.write(&initial, 1), 1U);

    const auto reservation = reader.reserveOne();
    ASSERT_TRUE(reservation);

    reader.requestReset();
    reader.consumeOne(reservation);

    int out{0};
    EXPECT_EQ(reader.read(&out, 1), 0U);

    const int fresh{8};
    EXPECT_EQ(writer.write(&fresh, 1), 1U);
    EXPECT_EQ(reader.read(&out, 1), 1U);
    EXPECT_EQ(out, 8);
}
} // namespace Fooyin::Testing
