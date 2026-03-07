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

#include <core/track.h>
#include <core/trackmetadatastore.h>

#include <gtest/gtest.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
TEST(TrackTest, DerivesPathFieldsForRegularFiles)
{
    const Track track{u"/music/Artist/Album/track01.FlAc"_s};

    EXPECT_EQ(track.filename(), u"track01"_s);
    EXPECT_EQ(track.directory(), u"Album"_s);
    EXPECT_EQ(track.extension(), u"flac"_s);
    EXPECT_EQ(track.effectiveTitle(), u"track01"_s);
}

TEST(TrackTest, DerivesPathFieldsForArchiveFiles)
{
    const QString archivePath = u"/music/archive.zip"_s;
    const QString innerPath   = u"disc1/track02.OGG"_s;
    const QString path        = u"unpack://zip|%1|file://%2!%3"_s.arg(archivePath.size()).arg(archivePath, innerPath);

    const Track track{path};

    EXPECT_TRUE(track.isInArchive());
    EXPECT_EQ(track.archivePath(), archivePath);
    EXPECT_EQ(track.pathInArchive(), innerPath);
    EXPECT_EQ(track.filename(), u"track02"_s);
    EXPECT_EQ(track.directory(), u"disc1"_s);
    EXPECT_EQ(track.extension(), u"ogg"_s);
    EXPECT_EQ(track.prettyFilepath(), u"/music/archive.zip/disc1/track02.OGG"_s);
}

TEST(TrackTest, UsesCompactMetadataAccessors)
{
    Track track;
    track.setArtists({u"Artist A"_s, u"Artist B"_s});
    track.setAlbumArtists({u"Album Artist"_s});
    track.setGenres({u"Ambient"_s, u"Drone"_s});

    EXPECT_TRUE(track.hasArtists());
    EXPECT_TRUE(track.hasAlbumArtists());
    EXPECT_TRUE(track.hasGenres());
    EXPECT_EQ(track.artistCount(), 2);
    EXPECT_EQ(track.artistAt(1), u"Artist B"_s);
    EXPECT_EQ(track.albumArtistCount(), 1);
    EXPECT_EQ(track.albumArtistAt(0), u"Album Artist"_s);
    EXPECT_EQ(track.genreCount(), 2);
    EXPECT_EQ(track.genreAt(0), u"Ambient"_s);
    EXPECT_EQ(track.artistsJoined(u" / "_s), u"Artist A / Artist B"_s);
    EXPECT_EQ(track.albumArtistsJoined(u" / "_s), u"Album Artist"_s);
    EXPECT_EQ(track.genresJoined(u" / "_s), u"Ambient / Drone"_s);
}

TEST(TrackTest, LazilyLoadsExtraTagsFromSerialisedData)
{
    Track source;
    source.addExtraTag(u"custom"_s, u"value"_s);
    source.addExtraTag(u"custom"_s, u"value2"_s);

    const QByteArray blob = source.serialiseExtraTags();

    Track loaded;
    loaded.storeExtraTags(blob);

    EXPECT_EQ(loaded.serialiseExtraTags(), blob);
    EXPECT_EQ(loaded.extraTag(u"CUSTOM"_s), (QStringList{u"value"_s, u"value2"_s}));

    loaded.addExtraTag(u"another"_s, u"entry"_s);
    EXPECT_EQ(loaded.extraTag(u"ANOTHER"_s), (QStringList{u"entry"_s}));
}

TEST(TrackTest, MigratesPooledMetadataToExplicitStore)
{
    Track track;
    track.setArtists({u"Artist A"_s, u"Artist B"_s});
    track.setAlbum(u"Shared Album"_s);
    track.setGenres({u"Ambient"_s});
    track.addExtraTag(u"custom"_s, u"value"_s);

    auto sharedStore = std::make_shared<TrackMetadataStore>();
    track.setMetadataStore(sharedStore);

    EXPECT_EQ(track.metadataStore(), sharedStore);
    EXPECT_EQ(track.artists(), (QStringList{u"Artist A"_s, u"Artist B"_s}));
    EXPECT_EQ(track.album(), u"Shared Album"_s);
    EXPECT_EQ(track.genre(), u"Ambient"_s);
    EXPECT_EQ(track.extraTag(u"CUSTOM"_s), (QStringList{u"value"_s}));

    EXPECT_EQ(sharedStore->values(StringPool::Domain::Artist), (QStringList{u"Artist A"_s, u"Artist B"_s}));
    EXPECT_EQ(sharedStore->values(StringPool::Domain::Album), (QStringList{u"Shared Album"_s}));
    EXPECT_EQ(sharedStore->values(StringPool::Domain::ExtraTagKey), (QStringList{u"CUSTOM"_s}));
}
} // namespace Fooyin::Testing
