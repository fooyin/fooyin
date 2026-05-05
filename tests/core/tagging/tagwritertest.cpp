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

#include "testutils.h"

#include <core/constants.h>
#include <core/coresettings.h>
#include <core/engine/input/ratingtagpolicy.h>
#include <core/engine/input/taglibparser.h>
#include <core/engine/tagdefs.h>
#include <core/track.h>

#include <taglib/id3v2tag.h>
#include <taglib/mp4file.h>
#include <taglib/mpegfile.h>
#include <taglib/opusfile.h>
#include <taglib/popularimeterframe.h>
#include <taglib/textidentificationframe.h>
#include <taglib/vorbisfile.h>

#include <QBuffer>
#include <QImage>

#include <gtest/gtest.h>

// clazy:excludeall=returning-void-expression

constexpr auto Flags = Fooyin::AudioReader::Metadata;

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
class TagWriterTest : public ::testing::Test
{
protected:
    TagLibReader m_parser;
};

namespace {
constexpr auto Mp4AlbumGain    = "----:com.apple.iTunes:replaygain_album_gain";
constexpr auto Mp4AlbumPeak    = "----:com.apple.iTunes:replaygain_album_peak";
constexpr auto Mp4TrackGain    = "----:com.apple.iTunes:replaygain_track_gain";
constexpr auto Mp4TrackPeak    = "----:com.apple.iTunes:replaygain_track_peak";
constexpr auto Mp4AlbumGainAlt = "----:com.apple.iTunes:REPLAYGAIN_ALBUM_GAIN";
constexpr auto Mp4AlbumPeakAlt = "----:com.apple.iTunes:REPLAYGAIN_ALBUM_PEAK";
constexpr auto Mp4TrackGainAlt = "----:com.apple.iTunes:REPLAYGAIN_TRACK_GAIN";
constexpr auto Mp4TrackPeakAlt = "----:com.apple.iTunes:REPLAYGAIN_TRACK_PEAK";

QByteArray createPngCover(const QSize& size)
{
    QImage image(size, QImage::Format_ARGB32);

    for(int y{0}; y < image.height(); ++y) {
        for(int x{0}; x < image.width(); ++x) {
            image.setPixelColor(x, y,
                                QColor::fromRgb((x * 255) / image.width(), (y * 255) / image.height(),
                                                ((x + y) * 255) / (image.width() + image.height())));
        }
    }

    QByteArray data;
    QBuffer buffer{&data};
    if(buffer.open(QIODevice::WriteOnly)) {
        image.save(&buffer, "PNG");
    }

    return data;
}
QString mp4StringItem(const TagLib::MP4::ItemMap& items, const char* key)
{
    if(!items.contains(key)) {
        return {};
    }

    const auto value = items[key].toStringList().toString("\n");
    return QString::fromUtf8(value.toCString(true));
}

QString xiphStringItem(const TagLib::Ogg::FieldListMap& fields, const char* key)
{
    if(!fields.contains(key) || fields[key].isEmpty()) {
        return {};
    }

    return QString::fromUtf8(fields[key].front().toCString(true));
}

void resetTagWriterRatingSettings()
{
    resetRatingSettings("/tmp/fooyin-tagwriter-test-config");
}

TagLib::ID3v2::PopularimeterFrame* popmFrameByOwner(TagLib::ID3v2::Tag* tag, const QString& owner)
{
    if(!tag || !tag->frameListMap().contains("POPM")) {
        return nullptr;
    }

    const TagLib::String ownerString{owner.toUtf8().constData(), TagLib::String::UTF8};
    for(auto* frame : tag->frameListMap()["POPM"]) {
        auto* popmFrame = dynamic_cast<TagLib::ID3v2::PopularimeterFrame*>(frame);
        if(popmFrame && popmFrame->email() == ownerString) {
            return popmFrame;
        }
    }
    return nullptr;
}

QString id3UserTextFrameValue(TagLib::ID3v2::Tag* tag, const char* description)
{
    auto* frame = TagLib::ID3v2::UserTextIdentificationFrame::find(tag, description);
    if(!frame || frame->fieldList().isEmpty()) {
        return {};
    }
    return QString::fromUtf8(frame->fieldList().back().toCString(true));
}
} // namespace

TEST_F(TagWriterTest, AiffWrite)
{
    const QString filepath = u":/audio/audiotest.aiff"_s;
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(source, track));

        track.setId(0);
        track.setTitle(u"TestTitle"_s);
        track.setAlbum({u"TestAlbum"_s});
        track.setAlbumArtists({u"TestAArtist"_s});
        track.setArtists({u"TestArtist"_s});
        track.setDate(u"2023-12-12"_s);
        track.setTrackNumber(u"9"_s);
        track.setTrackTotal(u"99"_s);
        track.setDiscNumber(u"4"_s);
        track.setDiscTotal(u"44"_s);
        track.setGenres({u"TestGenre"_s});
        track.setPerformers({u"TestPerformer"_s});
        track.setComposers({u"testComposer"_s});
        track.setComment(u"TestComment"_s);
        track.addExtraTag(u"WRITETEST"_s, u"Success"_s);
        track.removeExtraTag(u"TEST"_s);

        ASSERT_TRUE(m_parser.writeTrack(source, track, Flags));
    }

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(source, track));

        EXPECT_EQ(track.title(), u"TestTitle"_s);
        EXPECT_EQ(track.album(), u"TestAlbum"_s);
        EXPECT_EQ(track.albumArtist(), u"TestAArtist"_s);
        EXPECT_EQ(track.artist(), u"TestArtist"_s);
        EXPECT_EQ(track.date(), u"2023-12-12"_s);
        EXPECT_EQ(track.trackNumber(), u"9"_s);
        EXPECT_EQ(track.trackTotal(), u"99"_s);
        EXPECT_EQ(track.discNumber(), u"4"_s);
        EXPECT_EQ(track.discTotal(), u"44"_s);
        EXPECT_EQ(track.genre(), u"TestGenre"_s);
        EXPECT_EQ(track.performer(), u"TestPerformer"_s);
        EXPECT_EQ(track.composer(), u"testComposer"_s);
        EXPECT_EQ(track.comment(), u"TestComment"_s);

        const auto testTag = track.extraTag(u"TEST"_s);
        EXPECT_TRUE(testTag.empty());

        const auto writeTag = track.extraTag(u"WRITETEST"_s);
        ASSERT_TRUE(!writeTag.isEmpty());
        EXPECT_EQ(writeTag.front(), u"Success"_s);
    }
}

TEST_F(TagWriterTest, FlacWrite)
{
    const QString filepath = u":/audio/audiotest.flac"_s;
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack({filepath, &file, nullptr}, track));

        track.setId(0);
        track.setTitle(u"TestTitle"_s);
        track.setAlbum({u"TestAlbum"_s});
        track.setAlbumArtists({u"TestAArtist"_s});
        track.setArtists({u"TestArtist"_s});
        track.setDate(u"2023-12-12"_s);
        track.setTrackNumber(u"9"_s);
        track.setTrackTotal(u"99"_s);
        track.setDiscNumber(u"4"_s);
        track.setDiscTotal(u"44"_s);
        track.setGenres({u"TestGenre"_s});
        track.setPerformers({u"TestPerformer"_s});
        track.setComposers({u"testComposer"_s});
        track.setComment(u"TestComment"_s);
        track.addExtraTag(u"WRITETEST"_s, u"Success"_s);
        track.removeExtraTag(u"TEST"_s);

        ASSERT_TRUE(m_parser.writeTrack(source, track, Flags));
    }

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack({filepath, &file, nullptr}, track));

        EXPECT_EQ(track.title(), u"TestTitle"_s);
        EXPECT_EQ(track.album(), u"TestAlbum"_s);
        EXPECT_EQ(track.albumArtist(), u"TestAArtist"_s);
        EXPECT_EQ(track.artist(), u"TestArtist"_s);
        EXPECT_EQ(track.date(), u"2023-12-12"_s);
        EXPECT_EQ(track.trackNumber(), u"9"_s);
        EXPECT_EQ(track.trackTotal(), u"99"_s);
        EXPECT_EQ(track.discNumber(), u"4"_s);
        EXPECT_EQ(track.discTotal(), u"44"_s);
        EXPECT_EQ(track.genre(), u"TestGenre"_s);
        EXPECT_EQ(track.performer(), u"TestPerformer"_s);
        EXPECT_EQ(track.composer(), u"testComposer"_s);
        EXPECT_EQ(track.comment(), u"TestComment"_s);

        const auto testTag = track.extraTag(u"TEST"_s);
        EXPECT_TRUE(testTag.empty());

        const auto writeTag = track.extraTag(u"WRITETEST"_s);
        ASSERT_TRUE(!writeTag.isEmpty());
        EXPECT_EQ(writeTag.front(), u"Success"_s);
    }
}

TEST_F(TagWriterTest, FlacCoverWrite)
{
    const QString filepath = u":/audio/audiotest.flac"_s;
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    const QByteArray coverData = createPngCover({4000, 4000});
    ASSERT_FALSE(coverData.isEmpty());

    TrackCovers covers;
    covers.emplace(Track::Cover::Front, CoverImage{.mimeType = u"image/png"_s, .data = coverData});

    Track track{file.fileName()};
    ASSERT_TRUE(m_parser.writeCover(source, track, covers, Flags));

    const QByteArray writtenCover = m_parser.readCover(source, track, Track::Cover::Front);
    ASSERT_FALSE(writtenCover.isEmpty());
    EXPECT_EQ(writtenCover, coverData);
}

TEST_F(TagWriterTest, M4aWrite)
{
    const QString filepath = u":/audio/audiotest.m4a"_s;
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(source, track));

        track.setId(0);
        track.setTitle(u"TestTitle"_s);
        track.setAlbum({u"TestAlbum"_s});
        track.setAlbumArtists({u"TestAArtist"_s});
        track.setArtists({u"TestArtist"_s});
        track.setDate(u"2023-12-12"_s);
        track.setTrackNumber(u"9"_s);
        track.setTrackTotal(u"99"_s);
        track.setDiscNumber(u"4"_s);
        track.setDiscTotal(u"44"_s);
        track.setGenres({u"TestGenre"_s});
        track.setPerformers({u"TestPerformer"_s});
        track.setComposers({u"testComposer"_s});
        track.setComment(u"TestComment"_s);
        track.addExtraTag(u"WRITETEST"_s, u"Success"_s);
        track.removeExtraTag(u"TEST"_s);

        ASSERT_TRUE(m_parser.writeTrack(source, track, Flags));
    }

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack({filepath, &file, nullptr}, track));

        EXPECT_EQ(track.title(), u"TestTitle"_s);
        EXPECT_EQ(track.album(), u"TestAlbum"_s);
        EXPECT_EQ(track.albumArtist(), u"TestAArtist"_s);
        EXPECT_EQ(track.artist(), u"TestArtist"_s);
        EXPECT_EQ(track.date(), u"2023-12-12"_s);
        EXPECT_EQ(track.trackNumber(), u"9"_s);
        EXPECT_EQ(track.trackTotal(), u"99"_s);
        EXPECT_EQ(track.discNumber(), u"4"_s);
        EXPECT_EQ(track.discTotal(), u"44"_s);
        EXPECT_EQ(track.genre(), u"TestGenre"_s);
        EXPECT_EQ(track.performer(), u"TestPerformer"_s);
        EXPECT_EQ(track.composer(), u"testComposer"_s);
        EXPECT_EQ(track.comment(), u"TestComment"_s);

        const auto testTag = track.extraTag(u"TEST"_s);
        EXPECT_TRUE(testTag.empty());

        const auto writeTag = track.extraTag(u"WRITETEST"_s);
        ASSERT_TRUE(!writeTag.isEmpty());
        EXPECT_EQ(writeTag.front(), u"Success"_s);
    }
}

TEST_F(TagWriterTest, M4aWriteReplayGain)
{
    const QString filepath = u":/audio/audiotest.m4a"_s;
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(source, track));

        track.setId(0);
        track.setRGTrackGain(-7.25F);
        track.setRGTrackPeak(0.987654F);
        track.setRGAlbumGain(-6.50F);
        track.setRGAlbumPeak(0.876543F);

        ASSERT_TRUE(m_parser.writeTrack(source, track, Flags));
    }

    ASSERT_TRUE(file.flush());

    const QByteArray localPath = file.fileName().toLocal8Bit();
    {
        const TagLib::MP4::File mp4File(localPath.constData());
        ASSERT_TRUE(mp4File.isValid());
        ASSERT_TRUE(mp4File.tag());

        const auto& items = mp4File.tag()->itemMap();
        EXPECT_EQ(mp4StringItem(items, Mp4TrackGain), QStringLiteral("-7.25 dB"));
        EXPECT_EQ(mp4StringItem(items, Mp4TrackPeak), QStringLiteral("0.987654"));
        EXPECT_EQ(mp4StringItem(items, Mp4AlbumGain), QStringLiteral("-6.50 dB"));
        EXPECT_EQ(mp4StringItem(items, Mp4AlbumPeak), QStringLiteral("0.876543"));

        EXPECT_FALSE(items.contains(Mp4TrackGainAlt));
        EXPECT_FALSE(items.contains(Mp4TrackPeakAlt));
        EXPECT_FALSE(items.contains(Mp4AlbumGainAlt));
        EXPECT_FALSE(items.contains(Mp4AlbumPeakAlt));
    }
}

TEST_F(TagWriterTest, M4aWriteHonoursTextRatingTagSettings)
{
    resetTagWriterRatingSettings();

    const QString filepath = u":/audio/audiotest.m4a"_s;
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    {
        FySettings settings;
        settings.setValue(RatingSettings::WriteTag, u"RATING"_s);
        settings.setValue(RatingSettings::WriteScale, u"OneToTen"_s);
        settings.sync();
    }

    Track track{file.fileName()};
    ASSERT_TRUE(m_parser.readTrack(source, track));
    track.setId(0);
    track.setRating(0.7F);
    ASSERT_TRUE(m_parser.writeTrack(source, track, Fooyin::AudioReader::Rating));
    ASSERT_TRUE(file.flush());

    {
        TagLib::MP4::File mp4File(file.fileName().toLocal8Bit().constData());
        ASSERT_TRUE(mp4File.isValid());
        ASSERT_NE(mp4File.tag(), nullptr);

        const auto items = mp4File.tag()->itemMap();
        EXPECT_EQ(mp4StringItem(items, Fooyin::Mp4::RatingAlt2), u"7"_s);
        EXPECT_FALSE(items.contains(Fooyin::Mp4::RatingAlt));
    }

    resetTagWriterRatingSettings();
}

TEST_F(TagWriterTest, Mp3Write)
{
    const QString filepath = u":/audio/audiotest.mp3"_s;
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(source, track));

        track.setId(0);
        track.setTitle(u"TestTitle"_s);
        track.setAlbum({u"TestAlbum"_s});
        track.setAlbumArtists({u"TestAArtist"_s});
        track.setArtists({u"TestArtist"_s});
        track.setDate(u"2023-12-12"_s);
        track.setTrackNumber(u"9"_s);
        track.setTrackTotal(u"99"_s);
        track.setDiscNumber(u"4"_s);
        track.setDiscTotal(u"44"_s);
        track.setGenres({u"TestGenre"_s});
        track.setPerformers({u"TestPerformer"_s});
        track.setComposers({u"testComposer"_s});
        track.setComment(u"TestComment"_s);
        track.addExtraTag(u"WRITETEST"_s, u"Success"_s);
        track.removeExtraTag(u"TEST"_s);

        ASSERT_TRUE(m_parser.writeTrack(source, track, Flags));
    }

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack({filepath, &file, nullptr}, track));

        EXPECT_EQ(track.title(), u"TestTitle"_s);
        EXPECT_EQ(track.album(), u"TestAlbum"_s);
        EXPECT_EQ(track.albumArtist(), u"TestAArtist"_s);
        EXPECT_EQ(track.artist(), u"TestArtist"_s);
        EXPECT_EQ(track.date(), u"2023-12-12"_s);
        EXPECT_EQ(track.trackNumber(), u"9"_s);
        EXPECT_EQ(track.trackTotal(), u"99"_s);
        EXPECT_EQ(track.discNumber(), u"4"_s);
        EXPECT_EQ(track.discTotal(), u"44"_s);
        EXPECT_EQ(track.genre(), u"TestGenre"_s);
        EXPECT_EQ(track.performer(), u"TestPerformer"_s);
        EXPECT_EQ(track.composer(), u"testComposer"_s);
        EXPECT_EQ(track.comment(), u"TestComment"_s);

        const auto testTag = track.extraTag(u"TEST"_s);
        EXPECT_TRUE(testTag.empty());

        const auto writeTag = track.extraTag(u"WRITETEST"_s);
        ASSERT_TRUE(!writeTag.isEmpty());
        EXPECT_EQ(writeTag.front(), u"Success"_s);
    }
}

TEST_F(TagWriterTest, Mp3WriteHonoursPopmSettings)
{
    resetTagWriterRatingSettings();

    const QString filepath = u":/audio/audiotest.mp3"_s;
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    {
        FySettings settings;
        settings.setValue(RatingSettings::WriteId3Popm, true);
        settings.setValue(RatingSettings::PopmOwner, u"owner@example.com"_s);
        settings.setValue(RatingSettings::PopmMapping, u"CommonFiveStar"_s);
        settings.sync();
        EXPECT_EQ(settings.status(), QSettings::NoError);
    }
    EXPECT_EQ(ratingTagPolicy().popmOwner, u"owner@example.com"_s);

    Track track{file.fileName()};
    ASSERT_TRUE(m_parser.readTrack(source, track));
    track.setId(0);
    track.setRating(0.8F);
    ASSERT_TRUE(m_parser.writeTrack(source, track, Fooyin::AudioReader::Rating));
    ASSERT_TRUE(file.flush());

    {
        TagLib::MPEG::File mp3File(file.fileName().toLocal8Bit().constData());
        ASSERT_TRUE(mp3File.isValid());
        ASSERT_NE(mp3File.ID3v2Tag(), nullptr);

        auto* frame = popmFrameByOwner(mp3File.ID3v2Tag(), u"owner@example.com"_s);
        ASSERT_NE(frame, nullptr);
        EXPECT_EQ(QString::fromUtf8(frame->email().toCString(true)), u"owner@example.com"_s);
        EXPECT_EQ(frame->rating(), 196);
    }

    resetTagWriterRatingSettings();
}

TEST_F(TagWriterTest, Mp3WriteCanSkipPopm)
{
    resetTagWriterRatingSettings();

    const QString filepath = u":/audio/audiotest.mp3"_s;
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    {
        TagLib::MPEG::File mp3File(file.fileName().toLocal8Bit().constData());
        ASSERT_TRUE(mp3File.isValid());
        ASSERT_NE(mp3File.ID3v2Tag(true), nullptr);
        mp3File.ID3v2Tag()->removeFrames("POPM");
        ASSERT_TRUE(mp3File.save());
    }

    {
        FySettings settings;
        settings.setValue(RatingSettings::WriteId3Popm, false);
        settings.setValue(RatingSettings::PopmOwner, u"owner@example.com"_s);
        settings.sync();
    }

    Track track{file.fileName()};
    ASSERT_TRUE(m_parser.readTrack(source, track));
    track.setId(0);
    track.setRating(0.8F);
    ASSERT_TRUE(m_parser.writeTrack(source, track, Fooyin::AudioReader::Rating));
    ASSERT_TRUE(file.flush());

    {
        TagLib::MPEG::File mp3File(file.fileName().toLocal8Bit().constData());
        ASSERT_TRUE(mp3File.isValid());
        ASSERT_NE(mp3File.ID3v2Tag(), nullptr);
        EXPECT_EQ(popmFrameByOwner(mp3File.ID3v2Tag(), u"owner@example.com"_s), nullptr);
    }

    resetTagWriterRatingSettings();
}

TEST_F(TagWriterTest, Mp3RemoveRatingClearsTextRatingAndPopm)
{
    resetTagWriterRatingSettings();

    const QString filepath = u":/audio/audiotest.mp3"_s;
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    {
        FySettings settings;
        settings.setValue(RatingSettings::WriteId3Popm, true);
        settings.setValue(RatingSettings::PopmOwner, u"owner@example.com"_s);
        settings.setValue(RatingSettings::WriteTag, u"RATING"_s);
        settings.setValue(RatingSettings::WriteScale, u"OneToTen"_s);
        settings.sync();
    }

    Track track{file.fileName()};
    ASSERT_TRUE(m_parser.readTrack(source, track));
    track.setId(0);
    track.setRating(0.7F);
    ASSERT_TRUE(m_parser.writeTrack(source, track, Fooyin::AudioReader::Rating));
    ASSERT_TRUE(file.flush());

    track.setRating(0.0F);
    ASSERT_TRUE(m_parser.writeTrack(source, track, Fooyin::AudioReader::Rating));
    ASSERT_TRUE(file.flush());

    {
        TagLib::MPEG::File mp3File(file.fileName().toLocal8Bit().constData());
        ASSERT_TRUE(mp3File.isValid());
        ASSERT_NE(mp3File.ID3v2Tag(), nullptr);

        EXPECT_TRUE(id3UserTextFrameValue(mp3File.ID3v2Tag(), "RATING").isEmpty());
        EXPECT_FALSE(mp3File.ID3v2Tag()->frameListMap().contains("FMPS_Rating"));
        EXPECT_EQ(popmFrameByOwner(mp3File.ID3v2Tag(), u"owner@example.com"_s), nullptr);
    }

    resetTagWriterRatingSettings();
}

TEST_F(TagWriterTest, Mp3WriteHonoursTextRatingTagSettings)
{
    resetTagWriterRatingSettings();

    const QString filepath = u":/audio/audiotest.mp3"_s;
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    {
        FySettings settings;
        settings.setValue(RatingSettings::WriteId3Popm, false);
        settings.setValue(RatingSettings::WriteTag, u"RATING"_s);
        settings.setValue(RatingSettings::WriteScale, u"OneToTen"_s);
        settings.sync();
    }

    Track track{file.fileName()};
    ASSERT_TRUE(m_parser.readTrack(source, track));
    track.setId(0);
    track.setRating(0.7F);
    ASSERT_TRUE(m_parser.writeTrack(source, track, Fooyin::AudioReader::Rating));
    ASSERT_TRUE(file.flush());

    {
        TagLib::MPEG::File mp3File(file.fileName().toLocal8Bit().constData());
        ASSERT_TRUE(mp3File.isValid());
        ASSERT_NE(mp3File.ID3v2Tag(), nullptr);

        EXPECT_EQ(id3UserTextFrameValue(mp3File.ID3v2Tag(), "RATING"), u"7"_s);
        EXPECT_FALSE(mp3File.ID3v2Tag()->frameListMap().contains("FMPS_Rating"));
    }

    resetTagWriterRatingSettings();
}

TEST_F(TagWriterTest, OggWriteHonoursTextRatingTagSettings)
{
    resetTagWriterRatingSettings();

    const QString filepath = u":/audio/audiotest.ogg"_s;
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    {
        FySettings settings;
        settings.setValue(RatingSettings::WriteId3Popm, true);
        settings.setValue(RatingSettings::WriteTag, u"RATING"_s);
        settings.setValue(RatingSettings::WriteScale, u"OneToTen"_s);
        settings.sync();
        EXPECT_EQ(settings.status(), QSettings::NoError);
    }
    EXPECT_EQ(ratingTagPolicy().effectiveWriteTag(), u"RATING"_s);

    Track track{file.fileName()};
    ASSERT_TRUE(m_parser.readTrack(source, track));
    track.setId(0);
    track.setRating(0.7F);
    ASSERT_TRUE(m_parser.writeTrack(source, track, Fooyin::AudioReader::Rating));
    ASSERT_TRUE(file.flush());

    {
        TagLib::Ogg::Vorbis::File oggFile(file.fileName().toLocal8Bit().constData());
        ASSERT_TRUE(oggFile.isValid());
        ASSERT_NE(oggFile.tag(), nullptr);

        const auto fields = oggFile.tag()->fieldListMap();
        ASSERT_TRUE(fields.contains("RATING"));
        EXPECT_EQ(QString::fromUtf8(fields["RATING"].front().toCString(true)), u"7"_s);
        EXPECT_FALSE(fields.contains("FMPS_RATING"));
    }

    resetTagWriterRatingSettings();
}

TEST_F(TagWriterTest, OggWriteConfiguredRatingScales)
{
    struct Case
    {
        QString tag;
        QString scale;
        QString expectedValue;
        float expectedRating;
    };

    const std::vector<Case> cases{
        {.tag = u"FMPS_RATING"_s, .scale = u"Normalized01"_s, .expectedValue = u"0.7"_s, .expectedRating = 0.7F},
        {.tag = u"RATING"_s, .scale = u"OneToFive"_s, .expectedValue = u"4"_s, .expectedRating = 0.8F},
        {.tag = u"RATING"_s, .scale = u"OneToTen"_s, .expectedValue = u"7"_s, .expectedRating = 0.7F},
        {.tag = u"RATING"_s, .scale = u"OneToHundred"_s, .expectedValue = u"70"_s, .expectedRating = 0.7F},
        {.tag = u"MY_RATING"_s, .scale = u"OneToTen"_s, .expectedValue = u"7"_s, .expectedRating = 0.7F},
    };

    for(const auto& testCase : cases) {
        resetTagWriterRatingSettings();

        const QString filepath = u":/audio/audiotest.ogg"_s;
        TempResource file{filepath};
        file.checkValid();

        AudioSource source;
        source.filepath = file.fileName();
        source.device   = &file;

        {
            FySettings settings;
            settings.setValue(RatingSettings::WriteId3Popm, false);
            settings.setValue(RatingSettings::WriteTag, testCase.tag);
            settings.setValue(RatingSettings::WriteScale, testCase.scale);
            settings.sync();
        }

        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(source, track));
        track.setId(0);
        track.setRating(0.7F);
        ASSERT_TRUE(m_parser.writeTrack(source, track, Fooyin::AudioReader::Rating));
        ASSERT_TRUE(file.flush());

        {
            TagLib::Ogg::Vorbis::File oggFile(file.fileName().toLocal8Bit().constData());
            ASSERT_TRUE(oggFile.isValid());
            ASSERT_NE(oggFile.tag(), nullptr);

            const auto fields = oggFile.tag()->fieldListMap();
            EXPECT_EQ(xiphStringItem(fields, testCase.tag.toLatin1().constData()), testCase.expectedValue);
        }

        {
            FySettings settings;
            settings.setValue(RatingSettings::ReadTag, testCase.tag);
            settings.setValue(RatingSettings::ReadScale, testCase.scale);
            settings.sync();

            Track rereadTrack{file.fileName()};
            ASSERT_TRUE(m_parser.readTrack(source, rereadTrack));
            EXPECT_FLOAT_EQ(rereadTrack.rating(), testCase.expectedRating);
            EXPECT_EQ(rereadTrack.rawRatingTag(testCase.tag), testCase.expectedValue);
        }
    }

    resetTagWriterRatingSettings();
}

TEST_F(TagWriterTest, OggRemoveRatingClearsWrittenRawRatingOnReread)
{
    resetTagWriterRatingSettings();

    const QString filepath = u":/audio/audiotest.ogg"_s;
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    {
        FySettings settings;
        settings.setValue(RatingSettings::WriteId3Popm, false);
        settings.setValue(RatingSettings::WriteTag, u"MY_RATING"_s);
        settings.setValue(RatingSettings::WriteScale, u"OneToTen"_s);
        settings.setValue(RatingSettings::ReadTag, u"MY_RATING"_s);
        settings.setValue(RatingSettings::ReadScale, u"OneToTen"_s);
        settings.sync();
    }

    Track track{file.fileName()};
    ASSERT_TRUE(m_parser.readTrack(source, track));
    track.setId(0);
    track.setRating(0.7F);
    ASSERT_TRUE(m_parser.writeTrack(source, track, Fooyin::AudioReader::Rating));
    ASSERT_TRUE(file.flush());

    Track rereadTrack{file.fileName()};
    ASSERT_TRUE(m_parser.readTrack(source, rereadTrack));
    ASSERT_EQ(rereadTrack.rawRatingTag(u"MY_RATING"_s), u"7"_s);

    rereadTrack.setRating(0.0F);
    ASSERT_TRUE(m_parser.writeTrack(source, rereadTrack, Fooyin::AudioReader::Rating));
    ASSERT_TRUE(file.flush());

    Track clearedTrack{file.fileName()};
    ASSERT_TRUE(m_parser.readTrack(source, clearedTrack));
    EXPECT_LE(clearedTrack.rating(), 0.0F);
    EXPECT_EQ(clearedTrack.rawRatingTag(u"MY_RATING"_s), QString{});

    {
        TagLib::Ogg::Vorbis::File oggFile(file.fileName().toLocal8Bit().constData());
        ASSERT_TRUE(oggFile.isValid());
        ASSERT_NE(oggFile.tag(), nullptr);
        EXPECT_FALSE(oggFile.tag()->fieldListMap().contains("MY_RATING"));
    }

    resetTagWriterRatingSettings();
}

TEST_F(TagWriterTest, OggWrite)
{
    const QString filepath = u":/audio/audiotest.ogg"_s;
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(source, track));

        track.setId(0);
        track.setTitle(u"TestTitle"_s);
        track.setAlbum({u"TestAlbum"_s});
        track.setAlbumArtists({u"TestAArtist"_s});
        track.setArtists({u"TestArtist"_s});
        track.setDate(u"2023-12-12"_s);
        track.setTrackNumber(u"9"_s);
        track.setTrackTotal(u"99"_s);
        track.setDiscNumber(u"4"_s);
        track.setDiscTotal(u"44"_s);
        track.setGenres({u"TestGenre"_s});
        track.setPerformers({u"TestPerformer"_s});
        track.setComposers({u"testComposer"_s});
        track.setComment(u"TestComment"_s);
        track.addExtraTag(u"WRITETEST"_s, u"Success"_s);
        track.removeExtraTag(u"TEST"_s);

        ASSERT_TRUE(m_parser.writeTrack(source, track, Flags));
    }

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack({filepath, &file, nullptr}, track));

        EXPECT_EQ(track.title(), u"TestTitle"_s);
        EXPECT_EQ(track.album(), u"TestAlbum"_s);
        EXPECT_EQ(track.albumArtist(), u"TestAArtist"_s);
        EXPECT_EQ(track.artist(), u"TestArtist"_s);
        EXPECT_EQ(track.date(), u"2023-12-12"_s);
        EXPECT_EQ(track.trackNumber(), u"9"_s);
        EXPECT_EQ(track.trackTotal(), u"99"_s);
        EXPECT_EQ(track.discNumber(), u"4"_s);
        EXPECT_EQ(track.discTotal(), u"44"_s);
        EXPECT_EQ(track.genre(), u"TestGenre"_s);
        EXPECT_EQ(track.performer(), u"TestPerformer"_s);
        EXPECT_EQ(track.composer(), u"testComposer"_s);
        EXPECT_EQ(track.comment(), u"TestComment"_s);

        const auto testTag = track.extraTag(u"TEST"_s);
        EXPECT_TRUE(testTag.empty());

        const auto writeTag = track.extraTag(u"WRITETEST"_s);
        ASSERT_TRUE(!writeTag.isEmpty());
        EXPECT_EQ(writeTag.front(), u"Success"_s);
    }
}

TEST_F(TagWriterTest, OpusWrite)
{
    const QString filepath = u":/audio/audiotest.opus"_s;
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(source, track));

        track.setId(0);
        track.setTitle(u"TestTitle"_s);
        track.setAlbum({u"TestAlbum"_s});
        track.setAlbumArtists({u"TestAArtist"_s});
        track.setArtists({u"TestArtist"_s});
        track.setDate(u"2023-12-12"_s);
        track.setTrackNumber(u"9"_s);
        track.setTrackTotal(u"99"_s);
        track.setDiscNumber(u"4"_s);
        track.setDiscTotal(u"44"_s);
        track.setGenres({u"TestGenre"_s});
        track.setPerformers({u"TestPerformer"_s});
        track.setComposers({u"testComposer"_s});
        track.setComment(u"TestComment"_s);
        track.addExtraTag(u"WRITETEST"_s, u"Success"_s);
        track.removeExtraTag(u"TEST"_s);

        ASSERT_TRUE(m_parser.writeTrack(source, track, Flags));
    }

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack({filepath, &file, nullptr}, track));

        EXPECT_EQ(track.title(), u"TestTitle"_s);
        EXPECT_EQ(track.album(), u"TestAlbum"_s);
        EXPECT_EQ(track.albumArtist(), u"TestAArtist"_s);
        EXPECT_EQ(track.artist(), u"TestArtist"_s);
        EXPECT_EQ(track.date(), u"2023-12-12"_s);
        EXPECT_EQ(track.trackNumber(), u"9"_s);
        EXPECT_EQ(track.trackTotal(), u"99"_s);
        EXPECT_EQ(track.discNumber(), u"4"_s);
        EXPECT_EQ(track.discTotal(), u"44"_s);
        EXPECT_EQ(track.genre(), u"TestGenre"_s);
        EXPECT_EQ(track.performer(), u"TestPerformer"_s);
        EXPECT_EQ(track.composer(), u"testComposer"_s);
        EXPECT_EQ(track.comment(), u"TestComment"_s);

        const auto testTag = track.extraTag(u"TEST"_s);
        EXPECT_TRUE(testTag.empty());

        const auto writeTag = track.extraTag(u"WRITETEST"_s);
        ASSERT_TRUE(!writeTag.isEmpty());
        EXPECT_EQ(writeTag.front(), u"Success"_s);
    }
}

TEST_F(TagWriterTest, OpusR128ReadAsReplayGain)
{
    const QString filepath = QStringLiteral(":/audio/audiotest.opus");
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    const QByteArray localPath = file.fileName().toLocal8Bit();
    {
        TagLib::Ogg::Opus::File opusFile(localPath.constData());
        ASSERT_TRUE(opusFile.isValid());
        ASSERT_TRUE(opusFile.tag());

        opusFile.tag()->addField("R128_TRACK_GAIN", "0", true);
        opusFile.tag()->addField("R128_ALBUM_GAIN", "256", true);
        ASSERT_TRUE(opusFile.save());
    }

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(source, track));

        ASSERT_TRUE(track.hasTrackGain());
        ASSERT_TRUE(track.hasAlbumGain());
        EXPECT_FLOAT_EQ(track.rgTrackGain(), 5.0F);
        EXPECT_FLOAT_EQ(track.rgAlbumGain(), 6.0F);
        EXPECT_TRUE(track.extraTag(QStringLiteral("R128_TRACK_GAIN")).isEmpty());
        EXPECT_TRUE(track.extraTag(QStringLiteral("R128_ALBUM_GAIN")).isEmpty());
    }
}

TEST_F(TagWriterTest, OpusWriteUsesR128TagsInsteadOfReplayGainTags)
{
    const QString filepath = QStringLiteral(":/audio/audiotest.opus");
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(source, track));

        track.setId(0);
        track.setRGTrackGain(5.0F);
        track.setRGAlbumGain(6.0F);
        track.setRGTrackPeak(0.25F);
        track.setRGAlbumPeak(0.5F);
        track.setExtraProperty(QString::fromLatin1(Constants::OpusHeaderGainQ78), QStringLiteral("-3205"));

        ASSERT_TRUE(m_parser.writeTrack(source, track, Flags));
    }

    const QByteArray localPath = file.fileName().toLocal8Bit();
    const TagLib::Ogg::Opus::File opusFile(localPath.constData());
    ASSERT_TRUE(opusFile.isValid());
    ASSERT_TRUE(opusFile.tag());

    const auto& fields = opusFile.tag()->fieldListMap();
    ASSERT_TRUE(fields.contains("R128_TRACK_GAIN"));
    ASSERT_TRUE(fields.contains("R128_ALBUM_GAIN"));
    EXPECT_EQ(QString::fromUtf8(fields["R128_TRACK_GAIN"].front().toCString(true)), QStringLiteral("0"));
    EXPECT_EQ(QString::fromUtf8(fields["R128_ALBUM_GAIN"].front().toCString(true)), QStringLiteral("256"));

    EXPECT_FALSE(fields.contains("REPLAYGAIN_TRACK_GAIN"));
    EXPECT_FALSE(fields.contains("REPLAYGAIN_ALBUM_GAIN"));

    QFile headerFile{file.fileName()};
    ASSERT_TRUE(headerFile.open(QIODevice::ReadOnly));
    const auto headerGain = readOpusHeaderGainQ78(&headerFile);
    ASSERT_TRUE(headerGain.has_value());
    EXPECT_EQ(*headerGain, -3205);

    Track rereadTrack{file.fileName()};
    ASSERT_TRUE(m_parser.readTrack(source, rereadTrack));
    EXPECT_TRUE(rereadTrack.hasOpusHeaderGain());
    EXPECT_NEAR(rereadTrack.opusHeaderGainDb(), -12.5195F, 0.0002F);
    EXPECT_FLOAT_EQ(rereadTrack.rgTrackGain(), 5.0F);
    EXPECT_FLOAT_EQ(rereadTrack.rgAlbumGain(), 6.0F);
}

TEST_F(TagWriterTest, OpusWriteTrackHeaderModeUsesOpusR128Offset)
{
    const QString filepath = QStringLiteral(":/audio/audiotest.opus");
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(source, track));

        track.setRGTrackGain(-0.25F);

        const Track preparedTrack = prepareOpusRGWriteTrack(track, OpusRGWriteMode::Track);
        ASSERT_TRUE(m_parser.writeTrack(source, preparedTrack, Flags));
    }

    const QByteArray localPath = file.fileName().toLocal8Bit();
    const TagLib::Ogg::Opus::File opusFile(localPath.constData());
    ASSERT_TRUE(opusFile.isValid());
    ASSERT_TRUE(opusFile.tag());

    const auto& fields = opusFile.tag()->fieldListMap();
    ASSERT_TRUE(fields.contains("R128_TRACK_GAIN"));
    EXPECT_EQ(QString::fromUtf8(fields["R128_TRACK_GAIN"].front().toCString(true)), QStringLiteral("0"));

    QFile headerFile{file.fileName()};
    ASSERT_TRUE(headerFile.open(QIODevice::ReadOnly));
    const auto headerGain = readOpusHeaderGainQ78(&headerFile);
    ASSERT_TRUE(headerGain.has_value());
    EXPECT_EQ(*headerGain, -1344);

    Track rereadTrack{file.fileName()};
    ASSERT_TRUE(m_parser.readTrack(source, rereadTrack));
    EXPECT_NEAR(rereadTrack.opusHeaderGainDb(), -5.25F, 0.0002F);
    EXPECT_FLOAT_EQ(rereadTrack.rgTrackGain(), 5.0F);
    EXPECT_NEAR(rereadTrack.effectiveRGTrackGain(), -0.25F, 0.0002F);
}

TEST_F(TagWriterTest, OpusReadRejectsInvalidHeaderCrc)
{
    const QString filepath = QStringLiteral(":/audio/audiotest.opus");
    TempResource file{filepath};
    file.checkValid();

    QFile headerFile{file.fileName()};
    ASSERT_TRUE(headerFile.open(QIODevice::ReadWrite));
    ASSERT_TRUE(headerFile.seek(22));

    char crcByte{0};
    ASSERT_EQ(headerFile.read(&crcByte, 1), 1);
    ASSERT_TRUE(headerFile.seek(22));

    crcByte ^= static_cast<char>(0x01);
    ASSERT_EQ(headerFile.write(&crcByte, 1), 1);
    ASSERT_TRUE(headerFile.flush());

    ASSERT_TRUE(headerFile.seek(0));
    EXPECT_FALSE(readOpusHeaderGainQ78(&headerFile).has_value());
}

TEST_F(TagWriterTest, OpusRemoveReplayGainClearsAllReplayGainTags)
{
    const QString filepath = QStringLiteral(":/audio/audiotest.opus");
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    const QByteArray localPath = file.fileName().toLocal8Bit();
    {
        TagLib::Ogg::Opus::File opusFile(localPath.constData());
        ASSERT_TRUE(opusFile.isValid());
        ASSERT_TRUE(opusFile.tag());

        opusFile.tag()->addField("REPLAYGAIN_TRACK_GAIN", "-7.00 dB", true);
        opusFile.tag()->addField("REPLAYGAIN_ALBUM_GAIN", "-6.00 dB", true);
        opusFile.tag()->addField("REPLAYGAIN_TRACK_PEAK", "0.25", true);
        opusFile.tag()->addField("REPLAYGAIN_ALBUM_PEAK", "0.50", true);
        opusFile.tag()->addField("R128_TRACK_GAIN", "0", true);
        opusFile.tag()->addField("R128_ALBUM_GAIN", "256", true);
        ASSERT_TRUE(opusFile.save());
    }

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(source, track));

        track.clearRGInfo();
        track.removeExtraTag(QStringLiteral("REPLAYGAIN_TRACK_GAIN"));
        track.removeExtraTag(QStringLiteral("REPLAYGAIN_ALBUM_GAIN"));
        track.removeExtraTag(QStringLiteral("REPLAYGAIN_TRACK_PEAK"));
        track.removeExtraTag(QStringLiteral("REPLAYGAIN_ALBUM_PEAK"));
        track.removeExtraTag(QStringLiteral("R128_TRACK_GAIN"));
        track.removeExtraTag(QStringLiteral("R128_ALBUM_GAIN"));

        ASSERT_TRUE(m_parser.writeTrack(source, track, Flags));
    }

    {
        TagLib::Ogg::Opus::File opusFile(localPath.constData());
        ASSERT_TRUE(opusFile.isValid());
        ASSERT_TRUE(opusFile.tag());

        const auto& fields = opusFile.tag()->fieldListMap();
        EXPECT_FALSE(fields.contains("REPLAYGAIN_TRACK_GAIN"));
        EXPECT_FALSE(fields.contains("REPLAYGAIN_ALBUM_GAIN"));
        EXPECT_FALSE(fields.contains("REPLAYGAIN_TRACK_PEAK"));
        EXPECT_FALSE(fields.contains("REPLAYGAIN_ALBUM_PEAK"));
        EXPECT_FALSE(fields.contains("R128_TRACK_GAIN"));
        EXPECT_FALSE(fields.contains("R128_ALBUM_GAIN"));
    }

    {
        Track rereadTrack{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(source, rereadTrack));
        EXPECT_FALSE(rereadTrack.hasTrackGain());
        EXPECT_FALSE(rereadTrack.hasAlbumGain());
        EXPECT_FALSE(rereadTrack.hasTrackPeak());
        EXPECT_FALSE(rereadTrack.hasAlbumPeak());
    }
}

TEST_F(TagWriterTest, OpusWriteCanClearHeaderGain)
{
    const QString filepath = QStringLiteral(":/audio/audiotest.opus");
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(source, track));

        track.setExtraProperty(QString::fromLatin1(Constants::OpusHeaderGainQ78), QStringLiteral("0"));
        ASSERT_TRUE(m_parser.writeTrack(source, track, Flags));
    }

    QFile headerFile{file.fileName()};
    ASSERT_TRUE(headerFile.open(QIODevice::ReadOnly));
    const auto headerGain = readOpusHeaderGainQ78(&headerFile);
    ASSERT_TRUE(headerGain.has_value());
    EXPECT_EQ(*headerGain, 0);

    Track rereadTrack{file.fileName()};
    ASSERT_TRUE(m_parser.readTrack(source, rereadTrack));
    EXPECT_FALSE(rereadTrack.hasOpusHeaderGain());
}

TEST_F(TagWriterTest, WavWrite)
{
    const QString filepath = u":/audio/audiotest.wav"_s;
    TempResource file{filepath};
    file.checkValid();

    AudioSource source;
    source.filepath = file.fileName();
    source.device   = &file;

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(source, track));

        track.setId(0);
        track.setTitle(u"TestTitle"_s);
        track.setAlbum({u"TestAlbum"_s});
        track.setAlbumArtists({u"TestAArtist"_s});
        track.setArtists({u"TestArtist"_s});
        track.setDate(u"2023-12-12"_s);
        track.setTrackNumber(u"9"_s);
        track.setTrackTotal(u"99"_s);
        track.setDiscNumber(u"4"_s);
        track.setDiscTotal(u"44"_s);
        track.setGenres({u"TestGenre"_s});
        track.setPerformers({u"TestPerformer"_s});
        track.setComposers({u"testComposer"_s});
        track.setComment(u"TestComment"_s);
        track.addExtraTag(u"WRITETEST"_s, u"Success"_s);
        track.removeExtraTag(u"TEST"_s);

        ASSERT_TRUE(m_parser.writeTrack(source, track, Flags));
    }

    {
        Track track{file.fileName()};
        ASSERT_TRUE(m_parser.readTrack(source, track));

        EXPECT_EQ(track.title(), u"TestTitle"_s);
        EXPECT_EQ(track.album(), u"TestAlbum"_s);
        EXPECT_EQ(track.albumArtist(), u"TestAArtist"_s);
        EXPECT_EQ(track.artist(), u"TestArtist"_s);
        EXPECT_EQ(track.date(), u"2023-12-12"_s);
        EXPECT_EQ(track.trackNumber(), u"9"_s);
        EXPECT_EQ(track.trackTotal(), u"99"_s);
        EXPECT_EQ(track.discNumber(), u"4"_s);
        EXPECT_EQ(track.discTotal(), u"44"_s);
        EXPECT_EQ(track.genre(), u"TestGenre"_s);
        EXPECT_EQ(track.performer(), u"TestPerformer"_s);
        EXPECT_EQ(track.composer(), u"testComposer"_s);
        EXPECT_EQ(track.comment(), u"TestComment"_s);

        const auto testTag = track.extraTag(u"TEST"_s);
        EXPECT_TRUE(testTag.empty());

        const auto writeTag = track.extraTag(u"WRITETEST"_s);
        ASSERT_TRUE(!writeTag.isEmpty());
        EXPECT_EQ(writeTag.front(), u"Success"_s);
    }
}
} // namespace Fooyin::Testing
