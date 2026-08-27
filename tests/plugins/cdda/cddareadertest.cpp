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
 */

#include "cddareader.h"

#include "cddatoc.h"
#include "cddaurl.h"

#include <gtest/gtest.h>

#include <memory>

using namespace Qt::StringLiterals;

namespace Fooyin::Cdda {
namespace {
CdToc mixedModeToc()
{
    return {.firstTrackNumber = 1,
            .lastTrackNumber  = 4,
            .leadoutSector    = 450,
            .tracks           = {{.number = 1, .firstSector = 0, .endSectorExclusive = 75, .isAudio = false},
                                 {.number = 2, .firstSector = 75, .endSectorExclusive = 150, .isAudio = true},
                                 {.number = 3, .firstSector = 150, .endSectorExclusive = 300, .isAudio = false},
                                 {.number = 4, .firstSector = 300, .endSectorExclusive = 450, .isAudio = true}}};
}

struct BackendState
{
    CdToc toc{mixedModeToc()};
    CdText cdText;
    std::optional<CdError> error;
    int openCalls{0};
    int tocCalls{0};
};

class FakeSession : public CdDriveSession
{
public:
    explicit FakeSession(std::shared_ptr<BackendState> state)
        : m_state{std::move(state)}
    { }

    std::expected<CdToc, CdError> readToc() override
    {
        ++m_state->tocCalls;
        if(m_state->error) {
            return std::unexpected(*m_state->error);
        }
        return m_state->toc;
    }

    CdText readCdText(const CdToc& /*toc*/) override
    {
        return m_state->cdText;
    }

    std::expected<CdSectorRead, CdError> readAudioSectors(int /*firstSector*/, int count) override
    {
        return CdSectorRead{.pcm = QByteArray(count * BytesPerSector, '\0'), .sectorsRead = count};
    }

    void cancel() override { }

private:
    std::shared_ptr<BackendState> m_state;
};

class FakeBackend : public CdDriveBackend
{
public:
    explicit FakeBackend(std::shared_ptr<BackendState> state)
        : m_state{std::move(state)}
    { }

    std::vector<CdDriveInfo> drives() override
    {
        return {{.id                 = u"drive-a"_s,
                 .displayName        = u"Test drive"_s,
                 .settingsKey        = u"test-drive"_s,
                 .supportsSpeedLimit = false,
                 .vendor             = {},
                 .model              = {},
                 .revision           = {}}};
    }

    std::expected<OpenedDrive, CdError> open(const QString& driveId) override
    {
        if(driveId != u"drive-a"_s) {
            return std::unexpected(
                CdError{.code = CdDriveError::Unsupported, .message = u"Unknown drive"_s, .platformCode = 0});
        }
        ++m_state->openCalls;
        return OpenedDrive{.drive = drives().front(), .session = std::make_unique<FakeSession>(m_state)};
    }

private:
    std::shared_ptr<BackendState> m_state;
};

struct ReaderFixture
{
    ReaderFixture()
        : state{std::make_shared<BackendState>()}
        , discId{musicBrainzDiscId(state->toc).value()}
        , filepath{cddaUrl(discId)}
        , manager{std::make_shared<CdDriveManager>(std::make_unique<FakeBackend>(state))}
        , reader{manager}
    { }

    std::shared_ptr<BackendState> state;
    QString discId;
    QString filepath;
    std::shared_ptr<CdDriveManager> manager;
    CddaReader reader;
};
} // namespace

TEST(CddaReaderTest, AdvertisesOnlyCddaSchemeAndNoWriteCapabilities)
{
    const ReaderFixture fixture;

    EXPECT_TRUE(fixture.reader.extensions().isEmpty());
    EXPECT_EQ((QStringList{u"cdda"_s}), fixture.reader.supportedSchemes());
    EXPECT_FALSE(fixture.reader.canReadCover());
    EXPECT_FALSE(fixture.reader.canWriteCover());
    EXPECT_FALSE(fixture.reader.canWriteMetaData());
    EXPECT_EQ(0, fixture.reader.subsongCount());
}

TEST(CddaReaderTest, MapsSubsongsAcrossMixedModePhysicalTracks)
{
    ReaderFixture fixture;
    const AudioSource source{.filepath = fixture.filepath};
    ASSERT_TRUE(fixture.reader.init(source)) << fixture.reader.lastError().toStdString();
    EXPECT_EQ(2, fixture.reader.subsongCount());
    EXPECT_EQ(1, fixture.state->openCalls);
    EXPECT_EQ(1, fixture.state->tocCalls);

    Track first{fixture.filepath, 0};
    ASSERT_TRUE(fixture.reader.readTrack(source, first)) << fixture.reader.lastError().toStdString();
    EXPECT_EQ(u"Track 02"_s, first.title());
    EXPECT_EQ(u"Audio CD"_s, first.album());
    EXPECT_EQ(u"2"_s, first.trackNumber());
    EXPECT_EQ(u"2"_s, first.trackTotal());
    EXPECT_EQ(1000, first.duration());

    Track second{fixture.filepath, 1};
    ASSERT_TRUE(fixture.reader.readTrack(source, second)) << fixture.reader.lastError().toStdString();
    EXPECT_EQ(u"Track 04"_s, second.title());
    EXPECT_EQ(u"4"_s, second.trackNumber());
    EXPECT_EQ(2000, second.duration());
    EXPECT_EQ(44100, second.sampleRate());
    EXPECT_EQ(2, second.channels());
    EXPECT_EQ(16, second.bitDepth());
    EXPECT_EQ(1411, second.bitrate());
    EXPECT_EQ(u"CDDA"_s, second.codec());
    EXPECT_EQ(u"Lossless"_s, second.encoding());
    const auto properties = second.extraProperties();
    EXPECT_EQ(fixture.discId, properties.value(u"_CDDA_DISC_ID"_s));
    EXPECT_EQ(u"4"_s, properties.value(u"_CDDA_TRACK_NUMBER"_s));

    // Reading metadata doesn't takes a drive lease or read audio sectors
    EXPECT_EQ(1, fixture.state->openCalls);
}

TEST(CddaReaderTest, AppliesCdTextBeforeGenericFallbacks)
{
    ReaderFixture fixture;
    fixture.state->cdText.disc = {.title     = u"Disc title"_s,
                                  .performer = u"Album artist"_s,
                                  .genre     = u"Disc genre"_s,
                                  .composer  = u"Disc composer"_s,
                                  .message   = u"Disc message"_s,
                                  .isrc      = {}};
    fixture.state->cdText.tracks.emplace(2, CdTextFields{.title     = u"Track title"_s,
                                                         .performer = u"Track artist"_s,
                                                         .genre     = {},
                                                         .composer  = u"Track composer"_s,
                                                         .message   = u"Track message"_s,
                                                         .isrc      = u"GBTEST0000001"_s});

    const auto observations = fixture.manager->observations(true);
    ASSERT_EQ(1, observations.size());
    ASSERT_TRUE(fixture.manager->readCdText(observations.front()).has_value());

    const AudioSource source{.filepath = fixture.filepath};
    ASSERT_TRUE(fixture.reader.init(source)) << fixture.reader.lastError().toStdString();

    Track first{fixture.filepath, 0};
    ASSERT_TRUE(fixture.reader.readTrack(source, first));
    EXPECT_EQ(u"Track title"_s, first.title());
    EXPECT_EQ(u"Disc title"_s, first.album());
    EXPECT_EQ((QStringList{u"Track artist"_s}), first.artists());
    EXPECT_EQ((QStringList{u"Album artist"_s}), first.albumArtists());
    EXPECT_EQ((QStringList{u"Disc genre"_s}), first.genres());
    EXPECT_EQ((QStringList{u"Track composer"_s}), first.composers());
    EXPECT_EQ(u"Track message"_s, first.comment());
    EXPECT_EQ((QStringList{u"GBTEST0000001"_s}), first.extraTag(u"ISRC"_s));

    Track second{fixture.filepath, 1};
    ASSERT_TRUE(fixture.reader.readTrack(source, second));
    EXPECT_EQ(u"Track 04"_s, second.title());
    EXPECT_EQ((QStringList{u"Album artist"_s}), second.artists());
    EXPECT_EQ((QStringList{u"Disc composer"_s}), second.composers());
    EXPECT_EQ(u"Disc message"_s, second.comment());
}

TEST(CddaReaderTest, KeepsExistingFallbackFields)
{
    ReaderFixture fixture;
    const AudioSource source{.filepath = fixture.filepath};
    ASSERT_TRUE(fixture.reader.init(source));

    Track track{fixture.filepath, 0};
    track.setTitle(u"Known title"_s);
    track.setAlbum(u"Known album"_s);
    track.setTrackNumber(u"A1"_s);
    track.setTrackTotal(u"9"_s);
    ASSERT_TRUE(fixture.reader.readTrack(source, track));

    EXPECT_EQ(u"Known title"_s, track.title());
    EXPECT_EQ(u"Known album"_s, track.album());
    EXPECT_EQ(u"A1"_s, track.trackNumber());
    EXPECT_EQ(u"9"_s, track.trackTotal());
    EXPECT_EQ(1000, track.duration());
}

TEST(CddaReaderTest, RejectsMissingMediaAndOutOfRangeSubsongs)
{
    ReaderFixture fixture;
    EXPECT_FALSE(fixture.reader.init({.filepath = u"cdda:///invalid"_s}));
    EXPECT_FALSE(fixture.reader.lastError().isEmpty());

    fixture.state->error = CdError{.code = CdDriveError::NoMedia, .message = u"No disc"_s, .platformCode = 0};
    fixture.manager->invalidateAll();
    EXPECT_FALSE(fixture.reader.init({.filepath = fixture.filepath}));
    EXPECT_TRUE(fixture.reader.lastError().contains(u"not inserted"_s));

    fixture.state->error.reset();
    fixture.manager->invalidateAll();
    const AudioSource source{.filepath = fixture.filepath};
    ASSERT_TRUE(fixture.reader.init(source));

    Track invalidSubsong{fixture.filepath, 2};
    EXPECT_FALSE(fixture.reader.readTrack(source, invalidSubsong));
    Track wrongSource{u"cdda:///AAAAAAAAAAAAAAAAAAAAAAAAAAAA"_s, 0};
    EXPECT_FALSE(fixture.reader.readTrack(source, wrongSource));
}
} // namespace Fooyin::Cdda