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

#include "core/playlist/parsers/cueparser.h"

#include <core/track.h>

#include <gtest/gtest.h>

#include <QDir>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
class CueParserTest : public ::testing::Test
{
public:
    CueParserTest()
        : m_audioLoader{std::make_shared<AudioLoader>()}
        , m_parser{std::make_unique<CueParser>(m_audioLoader)}
    { }

protected:
    std::shared_ptr<AudioLoader> m_audioLoader;
    std::unique_ptr<PlaylistParser> m_parser;
};

TEST_F(CueParserTest, SingleCue)
{
    const QString filepath = u":/playlists/singlefiletest.cue"_s;
    QFile file{filepath};
    if(file.open(QIODevice::ReadOnly)) {
        QDir dir{filepath};
        dir.cdUp();

        const auto readTrack = [](const Track& track) {
            return track;
        };

        const auto tracks = m_parser->readPlaylist(&file, filepath, dir, {readTrack}, false);
        ASSERT_EQ(2, tracks.size());

        EXPECT_EQ(1991, tracks.at(0).year());
        EXPECT_EQ(u"Alternative", tracks.at(0).genre());
        EXPECT_EQ(u"Loveless", tracks.at(0).album());
        EXPECT_EQ(u"Only Shallow", tracks.at(0).title());

        EXPECT_EQ(tracks.at(1).discNumber(), u"1"_s);
        EXPECT_EQ(tracks.at(1).trackNumber(), u"02"_s);
    }
}
} // namespace Fooyin::Testing
