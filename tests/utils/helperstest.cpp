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

#include <utils/helpers.h>
#include <utils/stringutils.h>

#include <gtest/gtest.h>

#include <QByteArray>
#include <QStringList>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
TEST(UtilsHelpersTest, FindUniqueStringUsesFirstAvailableSuffix)
{
    const QStringList names{u"Playlist"_s, u"Playlist (2)"_s};

    const QString uniqueName = Utils::findUniqueString(u"Playlist"_s, names, [](const auto& name) { return name; });

    EXPECT_EQ(uniqueName, u"Playlist (1)"_s);
}

TEST(UtilsHelpersTest, FindUniqueStringTreatsSuffixSpacingVariantsAsExisting)
{
    const QStringList names{u"Playlist"_s, u"Playlist(1)"_s, u"Playlist   (2)  "_s};

    const QString uniqueName = Utils::findUniqueString(u"Playlist"_s, names, [](const auto& name) { return name; });

    EXPECT_EQ(uniqueName, u"Playlist (3)"_s);
}

TEST(UtilsStringTest, DetectEncodingRejectsInvalidTopCandidate)
{
    const QByteArray data
        = QByteArray::fromHex("5b74693aa5a4a5f3a5d5a5a7a5eba5ce2028496e6665726e6f292028a1b6d1d7d1d7cffbb7c0b6d3a1b75456"
                              "b6afbbadc6accdb7c7fa295d0a5b61723a4d72732e20475245454e204150504c455d0a5b"
                              "616c3aa5a4a5f3a5d5a5a7a5eba5ce2028496e6665726e6f295d0a5b62793a5d0a5b6f66667365743a305d0a"
                              "5b6b616e613a31a4b731a4aaa4aa31a4e2a4ea31a4e2a4c831a4ad31a4ada4e7a4af31a4"
                              "aaa4aa31a4e2a4ea31a4e2a4c831a4ad31a4c631a4e4a4df31a4dca4af31a4a2a4eb31a4ca31a4d231a4d331"
                              "a4c8a4a631a4bf31a4e6a4e131a4a2a4f331a4bfa4a431a4af31a4b731a4b2a4ad31a4d6"
                              "31a4bda4af31a4e6a4a831a4c631a4e431a4dca4af31a4a2a4eb31a4ca31a4dfa4c131a4c8a4ad31a4b7a4e3"
                              "a4af31a4cca4af31a4c4a4c431a4bfa4c031a4dba4cea4aa31a4bf31a4b7a4eba4d931a4"
                              "dba4a631a4aaa4e231a4c031a4e4a4b531a4a8a4a431a4a8a4f331a4ca31a4ca31a4a431a4efa4e931a4aba4"
                              "aca4e431a4d2a4aba4ea31a4bf31a4dca4af31a4a4a4cea4c131a4d231a4ad31a4d231a4"
                              "a2a4eb31a4ca31a4bc31a4dca4af31a4b731a4b3a4a631a4ada4e5a4a631a4e431a4e6a4e131a4a2a4f331a4"
                              "bfa4a431a4af31a4c131a4b7a4ad31a4d631a4bda4af31a4e6a4a831a4b7a4e7a4af31a4"
                              "b9a4ac31a4c431a4af31a4a431a4b4a4a631a4ab31a4caa4ab31a4ada4ba31a4a2a4c831a4bfa4c032a4dfa4"
                              "caa4e231a4bf31a4d2a4aba4ea31a4dba4a631a4c631a4c831a4a2a4bfa4e931a4e831a4"
                              "bea4e931a4ef31a4b7a4eba4d931a4dba4a631a4b5a4a8a4ae31a4aba4bf31a4afa4eb31a4a8a4a431a4a8a4"
                              "f331a4ca31a4ca31a4a431a4afa4eb31a4ca31a4d5a4a631a4bba4f331a4b7a4dc31a4ef"
                              "31a4dca4af31a4a4a4cea4c131a4a4a4baa4df31a4dea4e231a4c4a4c531a4c4a4ca31a4dea4ca31a4bda4c4"
                              "31a4aea4e7a4a631a4c4a4bf31a4b7a4c431a4eca4f331a4a2a4bd31a4b1a4c431a4d9a4"
                              "c431a4e1a4f331a4c931a4afa4b531a4b831a4b4a4af31a4aaa4c831a4c731a4aca4f331a4b031a4a4a4bf31"
                              "a4c831a4de31a4dba4a631a4bca4f331a4d631a4dca4af31a4bfa4aba4e931a4e2a4ce31"
                              "a4a8a4a431a4a8a4f331a4ca31a4ca31a4a431a4dca4af31a4a4a4cea4c131a4d231a4ad31a4d231a4a2a4eb"
                              "5d0a5b30303a30302e39395da5a4a5f3a5d5a5a7a5eba5ce202d204d72732e2047524545"
                              "4e204150504c450a5b30303a30362e30305db4caa3bab4f3c9add4aad9460a5b30303a31322e31375dc7faa3"
                              "bab4f3c9add4aad9460a5b30303a32322e34325dd5d5a4e9a4b9a4cfe99c0a5b30303a32"
                              "332e38305d8357a4e9a4cf9a69a4ad9154a4eca4c6a4ada4bfc8d5a1a9a4e2ccd4ccad0a5b30303a32372e33"
                              "385d89f4a4cfb0b2cca9a4cac4baa4e9a4b7a4c0a4ac0a5b30303a33302e31335db4ccbc"
                              "a4b2bbd7e3b9caa4cba5bfa5e9a5bfa5e90a5b30303a33322e36305dd5d5a4e9a4b9a4cfcfa8a4df0a5b3030"
                              "3a33342e31365d8357a4e9a4ce9a69a4ad9154a4eca4c6a4a4a4bfb5c0a4cfa4c9a4b3a4"
                              "c00a5b30303a33372e37365d9572a4cfa4bfa4dea4cbb05ea4c0a4ac0a5b30303a34302e35365dcec2a4e2a4"
                              "eaa4cbb0fca4dea4ecd6bb0a5b30303a34332e30335dd1d7a4acc1a2a4c40a5b30303a34");

    EXPECT_EQ(Utils::detectEncoding(data), "GB18030");
}

TEST(UtilsStringTest, DetectEncodingPrefersCyrillicCandidateOverLatinFallback)
{
    const QByteArray data
        = QByteArray::fromHex("52454d2047454e524520526f636b0d0a52454d204441544520323031350d0a52454d20444953434944203541"
                              "3046463231380d0a52454d20434f4d4d454e5420224578616374417564696f436f707920"
                              "76312e36220d0a504552464f524d45522022caedff5a7a220d0a5449544c452022cff0e5e4e2e5f1f2ede8ea"
                              "220d0a46494c452022caedff5a7a202d20cff0e5e4e2e5f1f2ede8ea2e666c6163222057"
                              "4156450d0a2020545241434b20303120415544494f0d0a202020205449544c452022c4e6eeeae5f0202d20ed"
                              "e0f7e0ebee220d0a20202020504552464f524d45522022caedff5a7a220d0a2020202049"
                              "4e4445582030312030303a30303a30300d0a2020545241434b20303220415544494f0d0a202020205449544c"
                              "452022c4e6eeeae5f0202d20eae0f0f2e020f1f3e4fce1fb220d0a20202020504552464f"
                              "524d45522022caedff5a7a220d0a20202020494e4445582030312030313a33343a36370d0a2020545241434b"
                              "20303320415544494f0d0a202020205449544c452022cfe0f1f1e0e6e8f0220d0a202020");

    EXPECT_EQ(Utils::detectEncoding(data, {.preferredFallbackEncoding = "windows-1251"}), "windows-1251");
}
} // namespace Fooyin::Testing
