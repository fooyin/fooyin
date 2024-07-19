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

#include "core/playlist/parsers/m3uparser.h"

#include <core/track.h>

#include <gtest/gtest.h>

#include <QDir>

namespace Fooyin::Testing {
class M3uParserTest : public ::testing::Test
{
public:
    M3uParserTest()
        : m_audioLoader{std::make_shared<AudioLoader>()}
        , m_parser{std::make_unique<M3uParser>(m_audioLoader)}
    { }

protected:
    std::shared_ptr<AudioLoader> m_audioLoader;
    std::unique_ptr<PlaylistParser> m_parser;
};

TEST_F(M3uParserTest, StandardM3u)
{
    const QString filepath = QStringLiteral(":/playlists/standardtest.m3u");
    QFile file{filepath};
    if(file.open(QIODevice::ReadOnly)) {
        QDir dir{filepath};
        dir.cdUp();

        const auto tracks = m_parser->readPlaylist(&file, filepath, dir, false);
        ASSERT_EQ(3, tracks.size());
    }
}

TEST_F(M3uParserTest, ExtendedM3u)
{
    const QString filepath = QStringLiteral(":/playlists/extendedtest.m3u");
    QFile file{filepath};
    if(file.open(QIODevice::ReadOnly)) {
        QDir dir{filepath};
        dir.cdUp();

        const auto tracks = m_parser->readPlaylist(&file, filepath, dir, false);
        ASSERT_EQ(7, tracks.size());

        EXPECT_EQ(u"Rotten Apple", tracks.at(0).title());
        EXPECT_EQ(u"Alice in Chains", tracks.at(0).artist());
        EXPECT_EQ(u"Nutshell", tracks.at(1).title());
    }
}
} // namespace Fooyin::Testing
