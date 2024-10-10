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

#include <core/scripting/scriptparser.h>
#include <core/track.h>

#include <gtest/gtest.h>

#include <QDateTime>

namespace Fooyin::Testing {
class ScriptParserTest : public ::testing::Test
{
protected:
    ScriptParser m_parser;
};

TEST_F(ScriptParserTest, BasicLiteral)
{
    const auto script = m_parser.parse(QStringLiteral("I am a test."));
    EXPECT_EQ(u"I am a test.", m_parser.evaluate(script));
}

TEST_F(ScriptParserTest, EscapeComment)
{
    const auto script1 = m_parser.parse(QStringLiteral(R"("I am a \% test.")"));
    EXPECT_EQ(u"I am a % test.", m_parser.evaluate(script1));

    const auto script2 = m_parser.parse(QStringLiteral(R"("I am an \"escape test.")"));
    EXPECT_EQ(u"I am an \"escape test.", m_parser.evaluate(script2));
}

TEST_F(ScriptParserTest, Quote)
{
    const auto script = m_parser.parse(QStringLiteral(R"("I %am% a $test$.")"));
    EXPECT_EQ(u"I %am% a $test$.", m_parser.evaluate(script));
}

TEST_F(ScriptParserTest, StringTest)
{
    EXPECT_EQ(u"01", m_parser.evaluate(QStringLiteral("$num(1,2)")));
    EXPECT_EQ(u"04", m_parser.evaluate(QStringLiteral("$num(04,2)")));
    EXPECT_EQ(u"A replace cesc", m_parser.evaluate(QStringLiteral("$replace(A replace test,t,c)")));
    EXPECT_EQ(u"test", m_parser.evaluate(QStringLiteral("$slice(A slice test,8)")));
    EXPECT_EQ(u"slice", m_parser.evaluate(QStringLiteral("$slice(A slice test,2,5)")));
    EXPECT_EQ(u"A chop", m_parser.evaluate(QStringLiteral("$chop(A chop test,5)")));
    EXPECT_EQ(u"L", m_parser.evaluate(QStringLiteral("$left(Left test,1)")));
    EXPECT_EQ(u"est", m_parser.evaluate(QStringLiteral("$right(Right test,3)")));
    EXPECT_EQ(u"true", m_parser.evaluate(QStringLiteral("$if($stricmp(cmp,cMp),true,false)")));
    EXPECT_EQ(u"false", m_parser.evaluate(QStringLiteral("$if($strcmp(cmp,cMp),true,false)")));
}

TEST_F(ScriptParserTest, MathTest)
{
    EXPECT_EQ(3, m_parser.evaluate(QStringLiteral("$add(1,2)")).toInt());
    EXPECT_EQ(2, m_parser.evaluate(QStringLiteral("$sub(10,8)")).toInt());
    EXPECT_EQ(99, m_parser.evaluate(QStringLiteral("$mul(3,33)")).toInt());
    EXPECT_EQ(11, m_parser.evaluate(QStringLiteral("$div(33,3)")).toInt());
    EXPECT_EQ(1, m_parser.evaluate(QStringLiteral("$mod(10,3)")).toInt());
    EXPECT_EQ(2, m_parser.evaluate(QStringLiteral("$min(3,2,3,9,23,100,4)")).toInt());
    EXPECT_EQ(100, m_parser.evaluate(QStringLiteral("$max(3,2,3,9,23,100,4)")).toInt());
}

TEST_F(ScriptParserTest, ConditionalTest)
{
    EXPECT_EQ(u"true", m_parser.evaluate(QStringLiteral("$ifequal(1,1,true,false)")));
    EXPECT_EQ(u"false", m_parser.evaluate(QStringLiteral("$ifgreater(23,32,true,false)")));
    EXPECT_EQ(u"true", m_parser.evaluate(QStringLiteral("$iflonger(aaa,2,true,false)")));
}

TEST_F(ScriptParserTest, MetadataTest)
{
    Track track;
    track.setTitle(QStringLiteral("A Test"));

    EXPECT_EQ(u"A Test", m_parser.evaluate(QStringLiteral("%title%"), track));
    EXPECT_EQ(u"A Test", m_parser.evaluate(QStringLiteral("%title%[ - %album%]"), track));

    track.setAlbum(QStringLiteral("A Test Album"));

    EXPECT_EQ(u"A Test - A Test Album", m_parser.evaluate(QStringLiteral("%title%[ - %album%]"), track));

    track.setGenres({QStringLiteral("Pop"), QStringLiteral("Rock")});

    EXPECT_EQ(u"Pop, Rock", m_parser.evaluate(QStringLiteral("%genre%"), track));
    EXPECT_EQ(u"Pop\037Rock", m_parser.evaluate(QStringLiteral("%<genre>%"), track));

    track.setArtists({QStringLiteral("Me"), QStringLiteral("You")});

    EXPECT_EQ(u"Pop, Rock - Me, You", m_parser.evaluate(QStringLiteral("%genre% - %artist%"), track));
    EXPECT_EQ(u"Pop - Me\037Rock - Me\037Pop - You\037Rock - You",
              m_parser.evaluate(QStringLiteral("%<genre>% - %<artist>%"), track));

    EXPECT_EQ(u"", m_parser.evaluate(QStringLiteral("[%disc% - %track%]"), track));
}

TEST_F(ScriptParserTest, TrackListTest)
{
    TrackList tracks;

    Track track1;
    track1.setGenres({QStringLiteral("Pop")});
    track1.setDuration(2000);
    tracks.push_back(track1);

    Track track2;
    track2.setGenres({QStringLiteral("Rock")});
    track2.setDuration(3000);
    tracks.push_back(track2);

    EXPECT_EQ(u"2", m_parser.evaluate(QStringLiteral("%trackcount%"), tracks));
    EXPECT_EQ(u"00:05", m_parser.evaluate(QStringLiteral("%playtime%"), tracks));
    EXPECT_EQ(u"Pop / Rock", m_parser.evaluate(QStringLiteral("%genres%"), tracks));
}

TEST_F(ScriptParserTest, MetaTest)
{
    Track track;
    track.setArtists({QStringLiteral("The Verve")});

    EXPECT_EQ(u"The Verve", m_parser.evaluate(QStringLiteral("%albumartist%"), track));
    EXPECT_EQ(u"", m_parser.evaluate(QStringLiteral("$meta(albumartist)"), track));
    EXPECT_EQ(u"The Verve", m_parser.evaluate(QStringLiteral("$meta(artist)"), track));
}

TEST_F(ScriptParserTest, InfoTest)
{
    Track track;
    track.setChannels(2);

    EXPECT_EQ(u"Stereo", m_parser.evaluate(QStringLiteral("%channels%"), track));
    EXPECT_EQ(u"2", m_parser.evaluate(QStringLiteral("$info(channels)"), track));
}

TEST_F(ScriptParserTest, QueryTest)
{
    TrackList tracks;

    Track track1;
    track1.setId(0);
    track1.setTitle(QStringLiteral("Wandering Horizon"));
    track1.setAlbum(QStringLiteral("Electric Dreams"));
    track1.setAlbumArtists({QStringLiteral("Solar Artist 1"), QStringLiteral("Stellar Artist 2")});
    track1.setArtists({QStringLiteral("Lunar Sound 1"), QStringLiteral("Galactic Echo 2")});
    track1.setDate(QStringLiteral("2021-05-17"));
    track1.setTrackNumber(QStringLiteral("7"));
    track1.setDiscNumber(QStringLiteral("2"));
    track1.setBitDepth(24);
    track1.setSampleRate(44100);
    track1.setBitrate(1000);
    track1.setComment(QStringLiteral("Awesome track with deep beats"));
    track1.setComposers({QStringLiteral("Sound Designer 1"), QStringLiteral("Master Composer 2")});
    track1.setPerformers({QStringLiteral("Vocalist 1"), QStringLiteral("Instrumentalist 2")});
    track1.setDuration(210000);
    track1.setFileSize(45600000);
    track1.setDate(QStringLiteral("1991-03-29"));
    track1.setFirstPlayed(QDateTime::currentDateTime().addMonths(-2).toMSecsSinceEpoch());
    track1.setLastPlayed(QDateTime::currentMSecsSinceEpoch());
    track1.setPlayCount(1);
    tracks.push_back(track1);

    Track track2;
    track2.setId(1);
    track2.setTitle(QStringLiteral("Celestial Waves"));
    track2.setAlbum(QStringLiteral("Chasing Light"));
    track2.setAlbumArtists({QStringLiteral("Horizon Band"), QStringLiteral("Sunset Group")});
    track2.setArtists({QStringLiteral("Ocean Vibes"), QStringLiteral("Sky Whisper")});
    track2.setDate(QStringLiteral("2023-11-12"));
    track2.setTrackNumber(QStringLiteral("4"));
    track2.setDiscNumber(QStringLiteral("1"));
    track2.setBitDepth(24);
    track2.setSampleRate(48000);
    track2.setBitrate(950);
    track2.setComment(QStringLiteral("A serene journey through sound"));
    track2.setComposers({QStringLiteral("Melody Creator 1"), QStringLiteral("Harmony Producer 2")});
    track2.setDuration(195000);
    track2.setFileSize(32000000);
    track2.setDate(QStringLiteral("2010-09-15"));
    track2.setFirstPlayed(QDateTime::currentDateTime().addYears(-3).toMSecsSinceEpoch());
    track2.setLastPlayed(QDateTime::currentDateTime().addDays(-3).toMSecsSinceEpoch());
    track2.setPlayCount(8);
    tracks.push_back(track2);

    // Basic operator tests
    EXPECT_EQ(1, m_parser.filter(QStringLiteral("$info(duration)=210000"), tracks).size());
    EXPECT_EQ(1, m_parser.filter(QStringLiteral("playcount>1"), tracks).size());
    EXPECT_EQ(1, m_parser.filter(QStringLiteral("playcount GREATER 1"), tracks).size());
    EXPECT_EQ(0, m_parser.filter(QStringLiteral("playcount LESS 1"), tracks).size());
    EXPECT_EQ(2, m_parser.filter(QStringLiteral("playcount>=1"), tracks).size());
    EXPECT_EQ(0, m_parser.filter(QStringLiteral("playcount>=A"), tracks).size());

    // Logical operator tests
    EXPECT_EQ(1, m_parser.filter(QStringLiteral("title=Wandering Horizon AND genre MISSING"), tracks).size());
    EXPECT_EQ(2, m_parser.filter(QStringLiteral("playcount=1 OR playcount=8"), tracks).size());
    EXPECT_EQ(2, m_parser.filter(QStringLiteral("playcount=1 XOR playcount=8"), tracks).size());
    EXPECT_EQ(0, m_parser.filter(QStringLiteral("playcount=1 XOR playcount=1"), tracks).size());

    // Negation test
    EXPECT_EQ(1, m_parser.filter(QStringLiteral("!playcount=1"), tracks).size());
    EXPECT_EQ(0, m_parser.filter(QStringLiteral("NOT playcount>=1"), tracks).size());

    // PRESENT/MISSING keyword tests
    EXPECT_EQ(1, m_parser.filter(QStringLiteral("performer PRESENT"), tracks).size());
    EXPECT_EQ(1, m_parser.filter(QStringLiteral("performer MISSING"), tracks).size());

    // String matching tests
    EXPECT_EQ(1, m_parser.filter(QStringLiteral("title:wandering hor"), tracks).size());
    EXPECT_EQ(0, m_parser.filter(QStringLiteral("title=wandering hor"), tracks).size());
    EXPECT_EQ(2, m_parser.filter(QStringLiteral("title:Wa"), tracks).size());

    // Date comparisons
    EXPECT_EQ(1, m_parser.filter(QStringLiteral("date BEFORE 2000"), tracks).size());
    EXPECT_EQ(1, m_parser.filter(QStringLiteral("date AFTER 2000"), tracks).size());
    EXPECT_EQ(1, m_parser.filter(QStringLiteral("firstplayed SINCE 2022"), tracks).size());
    EXPECT_EQ(1, m_parser.filter(QStringLiteral("lastplayed DURING LAST MINUTE"), tracks).size());
    EXPECT_EQ(0, m_parser.filter(QStringLiteral("lastplayed DURING 2"), tracks).size());

    // Grouping and complex queries
    EXPECT_EQ(2, m_parser.filter(QStringLiteral("(playcount>=1 AND bitrate>500) OR title:Celestial"), tracks).size());
    EXPECT_EQ(2, m_parser.filter(QStringLiteral("(playcount>=1 AND (bitrate>500 OR bitrate=950))"), tracks).size());
    EXPECT_EQ(1, m_parser.filter(QStringLiteral("(playcount=8 OR (bitrate>950 AND duration>200000))"), tracks).size());
    EXPECT_EQ(0, m_parser.filter(QStringLiteral("(playcount=8 AND (bitrate<900 OR duration<180000))"), tracks).size());

    QString query = QStringLiteral("(title:Wand AND album:Elec) OR (title:Celest AND playcount=8)");
    EXPECT_EQ(2, m_parser.filter(query, tracks).size());
    query = QStringLiteral("(title:Wandering OR (playcount=1 AND bitrate>900))");
    EXPECT_EQ(1, m_parser.filter(query, tracks).size());
    query = QStringLiteral("((playcount>=1 AND bitrate>500) OR title:Celest) AND (duration_ms>180000)");
    EXPECT_EQ(2, m_parser.filter(query, tracks).size());
}
} // namespace Fooyin::Testing
