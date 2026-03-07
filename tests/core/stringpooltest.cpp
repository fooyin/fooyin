/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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

#include <core/stringpool.h>
#include <core/trackmetadatastore.h>

#include <gtest/gtest.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
TEST(StringPoolTest, ReusesIdsForEquivalentStrings)
{
    StringPool pool;

    const auto firstAlbum  = pool.internId(StringPool::Domain::Album, u"Shared Album"_s);
    const auto secondAlbum = pool.internId(StringPool::Domain::Album, QString{u"Shared Album"_s});
    EXPECT_EQ(firstAlbum, secondAlbum);

    const auto firstCodec  = pool.internId(StringPool::Domain::Codec, u"FLAC"_s);
    const auto secondCodec = pool.internId(StringPool::Domain::Codec, QString{u"FLAC"_s});
    EXPECT_EQ(firstCodec, secondCodec);

    const auto firstEncoding  = pool.internId(StringPool::Domain::Encoding, u"UTF-8"_s);
    const auto secondEncoding = pool.internId(StringPool::Domain::Encoding, QString{u"UTF-8"_s});
    EXPECT_EQ(firstEncoding, secondEncoding);
}

TEST(StringPoolTest, ResolvesPackedLists)
{
    StringPool pool;

    const auto ref = pool.internList(StringPool::Domain::Artist, {u"Shared Artist"_s, u"Featured Artist"_s});

    EXPECT_FALSE(ref.isEmpty());
    EXPECT_EQ(ref.size, 2);
    EXPECT_EQ(pool.valueAt(StringPool::Domain::Artist, ref, 0), u"Shared Artist"_s);
    EXPECT_EQ(pool.valueAt(StringPool::Domain::Artist, ref, 1), u"Featured Artist"_s);
    EXPECT_TRUE(pool.contains(StringPool::Domain::Artist, ref, u"Featured Artist"_s));
    EXPECT_FALSE(pool.contains(StringPool::Domain::Artist, ref, u"Missing Artist"_s));
    EXPECT_EQ(pool.resolveList(StringPool::Domain::Artist, ref),
              (QStringList{u"Shared Artist"_s, u"Featured Artist"_s}));
    EXPECT_EQ(pool.joined(StringPool::Domain::Artist, ref, u"; "_s), u"Shared Artist; Featured Artist"_s);
}
} // namespace Fooyin::Testing
