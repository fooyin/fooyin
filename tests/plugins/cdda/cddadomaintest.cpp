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

#include "cddatoc.h"
#include "cddaurl.h"

#include <gtest/gtest.h>

#include <limits>
#include <vector>

using namespace Qt::StringLiterals;

namespace Fooyin::Cdda {
namespace {
CdToc tocFromMusicBrainzOffsets(const std::vector<int>& offsets, int leadoutOffset)
{
    CdToc toc;
    toc.firstTrackNumber = 1;
    toc.lastTrackNumber  = static_cast<int>(offsets.size());
    toc.leadoutSector    = leadoutOffset - LeadInSectors;

    for(int index{0}; std::cmp_less(index, offsets.size()); ++index) {
        const int endOffset = std::cmp_less(index + 1, offsets.size()) ? offsets.at(index + 1) : leadoutOffset;
        toc.tracks.push_back({.number             = index + 1,
                              .firstSector        = offsets.at(index) - LeadInSectors,
                              .endSectorExclusive = endOffset - LeadInSectors,
                              .isAudio            = true});
    }

    return toc;
}

CdToc simpleToc()
{
    return {.firstTrackNumber = 1,
            .lastTrackNumber  = 3,
            .leadoutSector    = 300,
            .tracks           = {{.number = 1, .firstSector = 0, .endSectorExclusive = 100, .isAudio = true},
                                 {.number = 2, .firstSector = 100, .endSectorExclusive = 200, .isAudio = true},
                                 {.number = 3, .firstSector = 200, .endSectorExclusive = 300, .isAudio = true}}};
}
} // namespace

TEST(CddaTocTest, CalculatesPublishedMusicBrainzDiscIdAndToc)
{
    const CdToc toc = tocFromMusicBrainzOffsets(
        {150, 22767, 41887, 58317, 72102, 91375, 104652, 115380, 132165, 143932, 159870, 174597}, 267257);

    const auto discId = musicBrainzDiscId(toc);
    ASSERT_TRUE(discId.has_value());
    EXPECT_EQ(u"I5l9cCSFccLKFEKS.7wqSZAorPU-"_s, *discId);

    const auto queryToc = musicBrainzToc(toc);
    ASSERT_TRUE(queryToc.has_value());
    EXPECT_EQ(u"1+12+267257+150+22767+41887+58317+72102+91375+104652+115380+132165+143932+159870+174597"_s, *queryToc);
}

TEST(CddaTocTest, MapsSubsongsAcrossMixedModeTracks)
{
    const CdToc toc{
        .firstTrackNumber = 1,
        .lastTrackNumber  = 4,
        .leadoutSector    = 400,
        .tracks           = {{.number = 1, .firstSector = 0, .endSectorExclusive = 100, .isAudio = false},
                             {.number = 2, .firstSector = 100, .endSectorExclusive = 200, .isAudio = true},
                             {.number = 3, .firstSector = 200, .endSectorExclusive = 300, .isAudio = false},
                             {.number = 4, .firstSector = 300, .endSectorExclusive = 400, .isAudio = true}},
    };

    ASSERT_TRUE(validateToc(toc).has_value());
    EXPECT_EQ((std::vector<CdTocTrack>{{2, 100, 200, true}, {4, 300, 400, true}}), audioTracks(toc));
    EXPECT_EQ((CdTocTrack{2, 100, 200, true}), audioTrackForSubsong(toc, 0));
    EXPECT_EQ((CdTocTrack{4, 300, 400, true}), audioTrackForSubsong(toc, 1));
    EXPECT_FALSE(audioTrackForSubsong(toc, -1).has_value());
    EXPECT_FALSE(audioTrackForSubsong(toc, 2).has_value());
}

TEST(CddaTocTest, RejectsMalformedTocs)
{
    CdToc toc            = simpleToc();
    toc.tracks[1].number = 1;
    EXPECT_EQ(std::unexpected(CdTocError::InvalidTrackNumbers), validateToc(toc));

    toc                       = simpleToc();
    toc.tracks[1].firstSector = 101;
    EXPECT_EQ(std::unexpected(CdTocError::NoncontiguousTrackSectorRanges), validateToc(toc));

    toc               = simpleToc();
    toc.leadoutSector = 301;
    EXPECT_EQ(std::unexpected(CdTocError::FinalTrackBeforeLeadout), validateToc(toc));

    toc = simpleToc();
    for(auto& track : toc.tracks) {
        track.isAudio = false;
    }
    EXPECT_EQ(std::unexpected(CdTocError::NoAudioTracks), validateToc(toc));
}

TEST(CddaTocTest, ConvertsSectorUnitsAndDetectsOverflow)
{
    EXPECT_EQ(588, framesForSectors(1).value());
    EXPECT_EQ(2352, bytesForSectors(1).value());
    EXPECT_EQ(1000, durationForSectors(75).value());
    EXPECT_EQ(13, durationForSectors(1).value());

    EXPECT_FALSE(framesForSectors(std::numeric_limits<uint64_t>::max()).has_value());
    EXPECT_FALSE(bytesForSectors(std::numeric_limits<uint64_t>::max()).has_value());
    EXPECT_FALSE(durationForSectors(std::numeric_limits<uint64_t>::max()).has_value());
}

TEST(CddaUrlTest, RoundTripsCaseSensitiveDiscId)
{
    const QString discId = u"I5l9cCSFccLKFEKS.7wqSZAorPU-"_s;
    const QString path   = cddaUrl(discId);
    EXPECT_EQ(u"cdda:///I5l9cCSFccLKFEKS.7wqSZAorPU-"_s, path);

    const auto parsed = discIdFromCddaUrl(path);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(discId, *parsed);

    const auto differentlyCased = discIdFromCddaUrl(path.toLower());
    ASSERT_TRUE(differentlyCased.has_value());
    EXPECT_NE(discId, *differentlyCased);
}

TEST(CddaUrlTest, RejectsMalformedOrNonCanonicalUrls)
{
    EXPECT_TRUE(cddaUrl({}).isEmpty());
    EXPECT_FALSE(discIdFromCddaUrl(u"/music/disc.cdda"_s).has_value());
    EXPECT_FALSE(discIdFromCddaUrl(u"https://example.com/disc/I5l9cCSFccLKFEKS.7wqSZAorPU-/disc.cdda"_s).has_value());
    EXPECT_FALSE(discIdFromCddaUrl(u"cdda:///too-short"_s).has_value());
    EXPECT_FALSE(discIdFromCddaUrl(u"cdda:///I5l9cCSFccLKFEKS.7wqSZAorPU-?track=1"_s).has_value());
    EXPECT_FALSE(discIdFromCddaUrl(u"cdda:///I5l9cCSFccLKFEKS.7wqSZAorPU-/track"_s).has_value());
    EXPECT_FALSE(discIdFromCddaUrl(u"cdda://I5l9cCSFccLKFEKS.7wqSZAorPU-"_s).has_value());
}
} // namespace Fooyin::Cdda
