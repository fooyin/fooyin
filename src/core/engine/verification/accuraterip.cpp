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

#include <core/engine/verification/accuraterip.h>

#include <QCoreApplication>
#include <QtEndian>

#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>

using namespace Qt::StringLiterals;

namespace Fooyin::AccurateRip {
namespace {
uint32_t digitSum(uint32_t value)
{
    uint32_t result{0};
    while(value != 0) {
        result += value % 10;
        value /= 10;
    }
    return result;
}

uint32_t readLe32(const QByteArray& data, qsizetype offset)
{
    return qFromLittleEndian<uint32_t>(data.constData() + offset);
}

QString errorMessage(const char* text)
{
    return QCoreApplication::translate("Fooyin::AccurateRip", text);
}
} // namespace

std::expected<void, QString> validateLayout(const DiscLayout& layout)
{
    if(layout.tracks.empty() || layout.tracks.size() > MaximumTracks) {
        return std::unexpected(errorMessage("AccurateRip requires between 1 and 99 tracks"));
    }
    if(layout.tracks.front().firstSector != 0 || layout.leadoutSector == 0) {
        return std::unexpected(errorMessage("The album has an invalid CD layout"));
    }

    uint32_t expectedFirst{0};
    for(const TrackLayout& track : layout.tracks) {
        if(track.firstSector != expectedFirst || track.endSectorExclusive <= track.firstSector
           || track.endSectorExclusive > layout.leadoutSector) {
            return std::unexpected(errorMessage("The album has an invalid CD layout"));
        }
        expectedFirst = track.endSectorExclusive;
    }

    if(expectedFirst != layout.leadoutSector) {
        return std::unexpected(errorMessage("The album has an invalid CD lead-out"));
    }

    return {};
}

std::expected<DiscId, QString> discId(const DiscLayout& layout)
{
    if(const auto valid = validateLayout(layout); !valid) {
        return std::unexpected(valid.error());
    }

    DiscId result;
    result.trackCount = static_cast<int>(layout.tracks.size());

    for(size_t index{0}; index < layout.tracks.size(); ++index) {
        const uint32_t offset = layout.tracks.at(index).firstSector;
        result.id1 += offset;
        result.id2 += std::max(offset, 1U) * static_cast<uint32_t>(index + 1);
    }

    result.id1 += layout.leadoutSector;
    result.id2 += std::max(layout.leadoutSector, 1U) * static_cast<uint32_t>(result.trackCount + 1);

    uint32_t cddbSum{0};
    for(const TrackLayout& track : layout.tracks) {
        cddbSum += digitSum((track.firstSector + LeadInSectors) / SectorsPerSecond);
    }

    const uint32_t duration
        = (layout.leadoutSector / SectorsPerSecond) - (layout.tracks.front().firstSector / SectorsPerSecond);
    result.cddbId = ((cddbSum % 0xFFU) << 24) | (duration << 8) | static_cast<uint32_t>(result.trackCount);

    return result;
}

QUrl discUrl(const DiscId& id)
{
    const QString id1 = QString::number(id.id1, 16).rightJustified(8, u'0');
    return QUrl{u"https://www.accuraterip.com/accuraterip/%1/%2/%3/dBAR-%4-%5-%6-%7.bin"_s.arg(id1.at(7))
                    .arg(id1.at(6))
                    .arg(id1.at(5))
                    .arg(id.trackCount, 3, 10, QChar{u'0'})
                    .arg(id.id1, 8, 16, QChar{u'0'})
                    .arg(id.id2, 8, 16, QChar{u'0'})
                    .arg(id.cddbId, 8, 16, QChar{u'0'})};
}

std::expected<std::vector<Pressing>, QString> parseResponse(const QByteArray& data, const DiscId& expected)
{
    std::vector<Pressing> result;

    qsizetype offset{0};
    while(offset < data.size()) {
        if(data.size() - offset < 13) {
            return std::unexpected(errorMessage("Truncated AccurateRip response header"));
        }

        const auto tracks   = static_cast<uint8_t>(data.at(offset));
        const uint32_t id1  = readLe32(data, offset + 1);
        const uint32_t id2  = readLe32(data, offset + 5);
        const uint32_t cddb = readLe32(data, offset + 9);
        offset += 13;

        if(tracks < 1 || tracks > MaximumTracks || data.size() - offset < tracks * 9LL) {
            return std::unexpected(errorMessage("Invalid AccurateRip response record"));
        }

        Pressing pressing;
        pressing.reserve(tracks);

        for(int track{0}; std::cmp_less(track, tracks); ++track) {
            pressing.push_back({.confidence     = static_cast<uint8_t>(data.at(offset)),
                                .crc            = readLe32(data, offset + 1),
                                .offsetChecksum = readLe32(data, offset + 5)});
            offset += 9;
        }

        if(std::cmp_equal(tracks, expected.trackCount) && id1 == expected.id1 && id2 == expected.id2
           && cddb == expected.cddbId) {
            result.push_back(std::move(pressing));
        }
    }

    if(result.empty()) {
        return std::unexpected(errorMessage("AccurateRip response did not contain the requested disc"));
    }

    return result;
}

std::expected<TrackList, QString> prepareAlbumTracks(const TrackList& tracks)
{
    if(tracks.empty() || tracks.size() > MaximumTracks) {
        return std::unexpected(errorMessage("Select a complete album containing between 1 and 99 tracks"));
    }

    struct NumberedTrack
    {
        int number{0};
        Track track;
    };

    std::vector<NumberedTrack> numbered;
    numbered.reserve(tracks.size());

    QString discNumber;
    QString albumKey;
    int declaredTotal{0};

    for(const Track& track : tracks) {
        if(track.isRemote()) {
            return std::unexpected(errorMessage("AccurateRip verification does not support remote streams"));
        }

        if((track.sampleRate() > 0 && track.sampleRate() != 44100) || (track.channels() > 0 && track.channels() != 2)
           || (track.bitDepth() > 0 && track.bitDepth() != 16)
           || track.encoding().compare(u"Lossy"_s, Qt::CaseInsensitive) == 0) {
            return std::unexpected(errorMessage("AccurateRip requires lossless 16-bit, 44.1 kHz stereo audio"));
        }

        bool numberOk{false};
        const int number = track.trackNumber().toInt(&numberOk);
        if(!numberOk || number < 1 || number > MaximumTracks) {
            return std::unexpected(errorMessage("Every selected track must have a valid track number"));
        }

        const QString currentDisc = track.discNumber().trimmed();
        if(!currentDisc.isEmpty()) {
            if(discNumber.isEmpty()) {
                discNumber = currentDisc;
            }
            else if(currentDisc != discNumber) {
                return std::unexpected(errorMessage("The selection contains tracks from more than one disc"));
            }
        }

        const QString currentAlbumKey
            = track.album().trimmed().isEmpty()
                ? track.directory()
                : track.album().trimmed() + u'\n' + track.effectiveAlbumArtist(true).trimmed();
        if(albumKey.isEmpty()) {
            albumKey = currentAlbumKey;
        }
        else if(currentAlbumKey != albumKey) {
            return std::unexpected(errorMessage("The selection contains tracks from more than one album"));
        }

        if(!track.trackTotal().isEmpty()) {
            bool totalOk{false};
            const int total = track.trackTotal().toInt(&totalOk);
            if(!totalOk || total < 1 || (declaredTotal != 0 && declaredTotal != total)) {
                return std::unexpected(errorMessage("The selected tracks have inconsistent track totals"));
            }
            declaredTotal = total;
        }

        numbered.emplace_back(number, track);
    }

    std::ranges::sort(numbered, {}, &NumberedTrack::number);
    if(declaredTotal != 0 && std::cmp_not_equal(declaredTotal, numbered.size())) {
        return std::unexpected(errorMessage("Select every track from the disc before using AccurateRip"));
    }

    TrackList result;
    result.reserve(numbered.size());

    for(size_t index{0}; index < numbered.size(); ++index) {
        if(std::cmp_not_equal(numbered.at(index).number, index + 1)) {
            return std::unexpected(errorMessage("The selection must contain each track number exactly once"));
        }
        result.push_back(std::move(numbered.at(index).track));
    }

    return result;
}

Verifier::Verifier(DiscLayout layout, std::vector<Pressing> pressings, TrackList tracks)
    : m_layout{std::move(layout)}
    , m_pressings{std::move(pressings)}
    , m_tracks{std::move(tracks)}
{ }

Verifier::~Verifier() = default;

std::optional<size_t> Verifier::discTrackIndex(const Track& track) const
{
    const auto found
        = std::ranges::find_if(m_tracks, [&track](const Track& value) { return value.sameIdentityAs(track); });
    if(found == m_tracks.cend()) {
        return {};
    }
    return static_cast<size_t>(std::distance(m_tracks.cbegin(), found));
}

Verifier::State* Verifier::stateFor(const Track& track)
{
    const auto state
        = std::ranges::find_if(m_states, [&track](const State& value) { return value.track.sameIdentityAs(track); });
    return state == m_states.end() ? nullptr : &*state;
}

void Verifier::trackStarted(const Track& track, const AudioFormat& format)
{
    const std::scoped_lock lock{m_mutex};

    const auto index      = discTrackIndex(track);
    const bool validIndex = index && *index < m_layout.tracks.size();
    const uint64_t frames = validIndex ? static_cast<uint64_t>(m_layout.tracks.at(*index).endSectorExclusive
                                                               - m_layout.tracks.at(*index).firstSector)
                                             * FramesPerSector
                                       : 0;
    m_states.push_back({.track          = track,
                        .discTrackIndex = index.value_or(0),
                        .totalFrames    = frames,
                        .validFormat    = validIndex && format.sampleFormat() == SampleFormat::S16
                                       && format.sampleRate() == 44100 && format.channelCount() == 2});
}

void Verifier::sourceAudio(const Track& track, const AudioBuffer& buffer)
{
    const std::scoped_lock lock{m_mutex};

    State* state = stateFor(track);
    if(!state || !state->validFormat) {
        return;
    }

    const auto data = buffer.constData();
    for(size_t offset{0}; offset + sizeof(uint32_t) <= data.size(); offset += sizeof(uint32_t)) {
        int16_t left{0};
        int16_t right{0};
        std::memcpy(&left, data.data() + offset, sizeof(left));
        std::memcpy(&right, data.data() + offset + sizeof(left), sizeof(right));

        const uint32_t sample
            = static_cast<uint16_t>(left) | (static_cast<uint32_t>(static_cast<uint16_t>(right)) << 16);
        const uint64_t position                  = ++state->position;
        static constexpr uint64_t BoundaryFrames = 5L * FramesPerSector;
        const uint64_t first                     = state->discTrackIndex == 0 ? BoundaryFrames : 0;
        const uint64_t last                      = state->discTrackIndex + 1 == m_layout.tracks.size()
                                                     ? state->totalFrames - std::min(state->totalFrames, BoundaryFrames)
                                                     : state->totalFrames;
        if(position >= first && position <= last) {
            const uint64_t product = static_cast<uint64_t>(sample) * position;
            state->crcV1 += static_cast<uint32_t>(product);
            state->crcV2 += static_cast<uint32_t>(product) + static_cast<uint32_t>(product >> 32);
        }
    }
}

void Verifier::trackFinished(const Track& track, bool complete)
{
    const std::scoped_lock lock{m_mutex};

    if(State* state = stateFor(track)) {
        state->complete = complete && state->position == state->totalFrames;
    }
}

std::vector<TrackResult> Verifier::results() const
{
    const std::scoped_lock lock{m_mutex};

    std::vector<TrackResult> result;
    result.reserve(m_states.size());

    for(const State& state : m_states) {
        TrackResult trackResult{.track        = state.track,
                                .status       = VerifyStatus::Incomplete,
                                .crcV1        = state.crcV1,
                                .crcV2        = state.crcV2,
                                .confidence   = 0,
                                .sampleOffset = 0,
                                .databaseCrcs = {}};
        if(!state.validFormat) {
            trackResult.status = VerifyStatus::InvalidFormat;
        }
        else if(!state.complete) {
            trackResult.status = VerifyStatus::Incomplete;
        }
        else {
            trackResult.status = VerifyStatus::Mismatch;
            for(const Pressing& pressing : m_pressings) {
                if(state.discTrackIndex >= pressing.size()) {
                    continue;
                }
                const Checksum& expected = pressing.at(state.discTrackIndex);
                if(!std::ranges::contains(trackResult.databaseCrcs, expected.crc)) {
                    trackResult.databaseCrcs.push_back(expected.crc);
                }
                if(state.crcV1 == expected.crc || state.crcV2 == expected.crc) {
                    trackResult.status = VerifyStatus::Verified;
                    trackResult.confidence += static_cast<int>(expected.confidence);
                }
            }
        }

        result.push_back(std::move(trackResult));
    }

    return result;
}

AlbumVerifier::AlbumVerifier(TrackList tracks)
    : m_tracks{std::move(tracks)}
{ }

AlbumVerifier::~AlbumVerifier() = default;

std::optional<size_t> AlbumVerifier::discTrackIndex(const Track& track) const
{
    const auto found
        = std::ranges::find_if(m_tracks, [&track](const Track& value) { return value.sameIdentityAs(track); });
    if(found == m_tracks.cend()) {
        return {};
    }
    return static_cast<size_t>(std::distance(m_tracks.cbegin(), found));
}

AlbumVerifier::State* AlbumVerifier::stateFor(const Track& track)
{
    const auto state
        = std::ranges::find_if(m_states, [&track](const State& value) { return value.track.sameIdentityAs(track); });
    return state == m_states.end() ? nullptr : &*state;
}

void AlbumVerifier::trackStarted(const Track& track, const AudioFormat& format)
{
    const std::scoped_lock lock{m_mutex};

    const auto index = discTrackIndex(track);
    m_states.push_back({.track          = track,
                        .discTrackIndex = index.value_or(0),
                        .trailing       = {},
                        .offsetWindow   = {},
                        .validFormat    = index && format.sampleFormat() == SampleFormat::S16
                                       && format.sampleRate() == 44100 && format.channelCount() == 2});
}

void AlbumVerifier::sourceAudio(const Track& track, const AudioBuffer& buffer)
{
    const std::scoped_lock lock{m_mutex};

    State* state = stateFor(track);
    if(!state || !state->validFormat) {
        return;
    }

    static constexpr size_t BoundaryFrames      = 5L * FramesPerSector;
    static constexpr uint64_t OffsetWindowStart = (450ULL * FramesPerSector) - MaximumOffset;
    static constexpr uint64_t OffsetWindowEnd   = (450ULL * FramesPerSector) + MaximumOffset + FramesPerSector;
    const bool firstTrack                       = state->discTrackIndex == 0;
    const bool lastTrack                        = state->discTrackIndex + 1 == m_tracks.size();
    const auto data                             = buffer.constData();

    for(size_t offset{0}; offset + sizeof(uint32_t) <= data.size(); offset += sizeof(uint32_t)) {
        int16_t left{0};
        int16_t right{0};
        std::memcpy(&left, data.data() + offset, sizeof(left));
        std::memcpy(&right, data.data() + offset + sizeof(left), sizeof(right));

        const uint32_t sample
            = static_cast<uint16_t>(left) | (static_cast<uint32_t>(static_cast<uint16_t>(right)) << 16);
        const uint64_t position          = ++state->position;
        const uint64_t zeroBasedPosition = position - 1;
        if(zeroBasedPosition >= OffsetWindowStart && zeroBasedPosition < OffsetWindowEnd) {
            state->offsetWindow.push_back(sample);
        }
        if(firstTrack && position < BoundaryFrames) {
            continue;
        }

        const uint64_t product = static_cast<uint64_t>(sample) * position;
        const Contribution contribution{.crcV1 = static_cast<uint32_t>(product),
                                        .crcV2 = static_cast<uint32_t>(product) + static_cast<uint32_t>(product >> 32)};
        if(lastTrack) {
            state->trailing.push_back(contribution);
            if(state->trailing.size() <= BoundaryFrames) {
                continue;
            }
            const Contribution included = state->trailing.front();
            state->trailing.pop_front();
            state->crcV1 += included.crcV1;
            state->crcV2 += included.crcV2;
        }
        else {
            state->crcV1 += contribution.crcV1;
            state->crcV2 += contribution.crcV2;
        }
    }
}

void AlbumVerifier::trackFinished(const Track& track, bool complete)
{
    const std::scoped_lock lock{m_mutex};
    if(State* state = stateFor(track)) {
        state->complete = complete;
    }
}

std::expected<DiscLayout, QString> AlbumVerifier::layout() const
{
    const std::scoped_lock lock{m_mutex};

    if(m_states.size() != m_tracks.size()) {
        return std::unexpected(errorMessage("Not all album tracks were decoded"));
    }

    DiscLayout result;
    result.tracks.reserve(m_states.size());

    uint64_t firstSector{0};
    for(const State& state : m_states) {
        if(!state.validFormat) {
            return std::unexpected(errorMessage("AccurateRip requires 16-bit, 44.1 kHz stereo PCM"));
        }
        if(!state.complete) {
            return std::unexpected(errorMessage("An album track could not be decoded completely"));
        }
        if(state.position == 0 || state.position % FramesPerSector != 0) {
            return std::unexpected(QCoreApplication::translate(
                "Fooyin::AccurateRip", "The selected tracks do not form a correct gapless CD rip."));
        }

        const uint64_t sectors = state.position / FramesPerSector;
        if(firstSector + sectors > std::numeric_limits<uint32_t>::max()) {
            return std::unexpected(errorMessage("The album is too long for an AccurateRip layout"));
        }

        result.tracks.push_back({.firstSector        = static_cast<uint32_t>(firstSector),
                                 .endSectorExclusive = static_cast<uint32_t>(firstSector + sectors)});
        firstSector += sectors;
    }

    result.leadoutSector = static_cast<uint32_t>(firstSector);

    if(const auto valid = validateLayout(result); !valid) {
        return std::unexpected(valid.error());
    }

    return result;
}

OffsetMatch AlbumVerifier::bestOffset(const std::vector<Pressing>& pressings) const
{
    const std::scoped_lock lock{m_mutex};

    static constexpr size_t WindowSize = (2ULL * MaximumOffset) + FramesPerSector;

    std::vector<OffsetMatch> matches(static_cast<size_t>((2 * MaximumOffset) + 1));
    for(int offset{-MaximumOffset}; offset <= MaximumOffset; ++offset) {
        matches.at(static_cast<size_t>(offset) + MaximumOffset).sampleOffset = offset;
    }

    for(const State& state : m_states) {
        if(!state.validFormat || !state.complete || state.offsetWindow.size() != WindowSize) {
            continue;
        }

        uint32_t checksum{0};
        uint32_t sampleSum{0};
        for(int index{0}; index < FramesPerSector; ++index) {
            const uint32_t sample = state.offsetWindow.at(static_cast<size_t>(index));
            checksum += sample * static_cast<uint32_t>(index + 1);
            sampleSum += sample;
        }

        for(int offset{-MaximumOffset}; offset <= MaximumOffset; ++offset) {
            int confidence{0};
            for(const Pressing& pressing : pressings) {
                if(state.discTrackIndex < pressing.size()) {
                    const Checksum& expected = pressing.at(state.discTrackIndex);
                    if(expected.offsetChecksum != 0 && checksum == expected.offsetChecksum) {
                        confidence += static_cast<int>(expected.confidence);
                    }
                }
            }
            if(confidence > 0) {
                OffsetMatch& match = matches.at(static_cast<size_t>(offset) + MaximumOffset);
                ++match.matchingTracks;
                match.confidence += confidence;
            }

            if(offset < MaximumOffset) {
                const auto first  = static_cast<size_t>(offset + MaximumOffset);
                const size_t next = first + FramesPerSector;
                checksum -= sampleSum;
                checksum += state.offsetWindow.at(next) * FramesPerSector;
                sampleSum -= state.offsetWindow.at(first);
                sampleSum += state.offsetWindow.at(next);
            }
        }
    }

    return *std::ranges::max_element(matches, [](const OffsetMatch& lhs, const OffsetMatch& rhs) {
        if(lhs.matchingTracks != rhs.matchingTracks) {
            return lhs.matchingTracks < rhs.matchingTracks;
        }
        if(lhs.confidence != rhs.confidence) {
            return lhs.confidence < rhs.confidence;
        }
        return std::abs(lhs.sampleOffset) > std::abs(rhs.sampleOffset);
    });
}

std::vector<TrackResult> AlbumVerifier::results(const std::vector<Pressing>& pressings) const
{
    const std::scoped_lock lock{m_mutex};

    std::vector<TrackResult> result;
    result.reserve(m_states.size());

    for(const State& state : m_states) {
        TrackResult trackResult{.track        = state.track,
                                .status       = state.validFormat && state.complete ? VerifyStatus::Mismatch
                                              : state.validFormat                   ? VerifyStatus::Incomplete
                                                                                    : VerifyStatus::InvalidFormat,
                                .crcV1        = state.crcV1,
                                .crcV2        = state.crcV2,
                                .confidence   = 0,
                                .sampleOffset = 0,
                                .databaseCrcs = {}};
        if(trackResult.status == VerifyStatus::Mismatch) {
            for(const Pressing& pressing : pressings) {
                if(state.discTrackIndex >= pressing.size()) {
                    continue;
                }

                const Checksum& expected = pressing.at(state.discTrackIndex);
                if(!std::ranges::contains(trackResult.databaseCrcs, expected.crc)) {
                    trackResult.databaseCrcs.push_back(expected.crc);
                }
                if(state.crcV1 == expected.crc || state.crcV2 == expected.crc) {
                    trackResult.status = VerifyStatus::Verified;
                    trackResult.confidence += static_cast<int>(expected.confidence);
                }
            }
        }

        result.push_back(std::move(trackResult));
    }

    return result;
}

OffsetVerifier::OffsetVerifier(DiscLayout layout, std::vector<Pressing> pressings, TrackList tracks, int sampleOffset)
    : m_layout{std::move(layout)}
    , m_pressings{std::move(pressings)}
    , m_tracks{std::move(tracks)}
    , m_sampleOffset{std::clamp(sampleOffset, -MaximumOffset, MaximumOffset)}
{
    m_states.reserve(m_tracks.size());
    for(const Track& track : m_tracks) {
        m_states.push_back({.track = track});
    }
}

OffsetVerifier::~OffsetVerifier() = default;

std::optional<size_t> OffsetVerifier::trackIndex(const Track& track) const
{
    const auto found
        = std::ranges::find_if(m_tracks, [&track](const Track& value) { return value.sameIdentityAs(track); });
    if(found == m_tracks.cend()) {
        return {};
    }
    return static_cast<size_t>(std::distance(m_tracks.cbegin(), found));
}

void OffsetVerifier::trackStarted(const Track& track, const AudioFormat& format)
{
    const std::scoped_lock lock{m_mutex};
    if(const auto index = trackIndex(track); index && *index < m_states.size()) {
        State& state = m_states.at(*index);
        state.validFormat
            = format.sampleFormat() == SampleFormat::S16 && format.sampleRate() == 44100 && format.channelCount() == 2;
    }
}

void OffsetVerifier::sourceAudio(const Track& track, const AudioBuffer& buffer)
{
    const std::scoped_lock lock{m_mutex};
    const auto sourceIndex = trackIndex(track);
    if(!sourceIndex || *sourceIndex >= m_states.size() || !m_states.at(*sourceIndex).validFormat) {
        return;
    }

    State& sourceState        = m_states.at(*sourceIndex);
    const int64_t sourceStart = static_cast<int64_t>(m_layout.tracks.at(*sourceIndex).firstSector) * FramesPerSector;
    const auto trackStart     = [this](size_t index) {
        return static_cast<int64_t>(m_layout.tracks.at(index).firstSector) * FramesPerSector;
    };
    const auto trackEnd = [this](size_t index) {
        return static_cast<int64_t>(m_layout.tracks.at(index).endSectorExclusive) * FramesPerSector;
    };

    const auto data = buffer.constData();
    for(size_t byteOffset{0}; byteOffset + sizeof(uint32_t) <= data.size(); byteOffset += sizeof(uint32_t)) {
        int16_t left{0};
        int16_t right{0};
        std::memcpy(&left, data.data() + byteOffset, sizeof(left));
        std::memcpy(&right, data.data() + byteOffset + sizeof(left), sizeof(right));
        const uint32_t sample
            = static_cast<uint16_t>(left) | (static_cast<uint32_t>(static_cast<uint16_t>(right)) << 16);

        const int64_t sourcePosition  = sourceStart + static_cast<int64_t>(sourceState.sourcePosition++);
        const int64_t shiftedPosition = sourcePosition - m_sampleOffset;
        if(shiftedPosition < 0) {
            continue;
        }

        size_t targetIndex = *sourceIndex;
        if(shiftedPosition < trackStart(targetIndex)) {
            if(targetIndex == 0) {
                continue;
            }
            --targetIndex;
        }
        else if(shiftedPosition >= trackEnd(targetIndex)) {
            if(targetIndex + 1 >= m_states.size()) {
                continue;
            }
            ++targetIndex;
        }

        if(shiftedPosition < trackStart(targetIndex) || shiftedPosition >= trackEnd(targetIndex)) {
            continue;
        }

        const uint64_t position                  = static_cast<uint64_t>(shiftedPosition - trackStart(targetIndex)) + 1;
        static constexpr uint64_t BoundaryFrames = 5ULL * FramesPerSector;
        const auto trackFrames = static_cast<uint64_t>(trackEnd(targetIndex) - trackStart(targetIndex));
        if((targetIndex == 0 && position < BoundaryFrames)
           || (targetIndex + 1 == m_states.size() && position > trackFrames - std::min(trackFrames, BoundaryFrames))) {
            continue;
        }

        const uint64_t product = static_cast<uint64_t>(sample) * position;
        State& targetState     = m_states.at(targetIndex);
        targetState.crcV1 += static_cast<uint32_t>(product);
        targetState.crcV2 += static_cast<uint32_t>(product) + static_cast<uint32_t>(product >> 32);
    }
}

void OffsetVerifier::trackFinished(const Track& track, bool complete)
{
    const std::scoped_lock lock{m_mutex};

    if(const auto index = trackIndex(track); index && *index < m_states.size()) {
        const uint64_t expected      = static_cast<uint64_t>(m_layout.tracks.at(*index).endSectorExclusive
                                                             - m_layout.tracks.at(*index).firstSector)
                                     * FramesPerSector;
        m_states.at(*index).complete = complete && m_states.at(*index).sourcePosition == expected;
    }
}

std::vector<TrackResult> OffsetVerifier::results() const
{
    const std::scoped_lock lock{m_mutex};

    const bool albumComplete
        = std::ranges::all_of(m_states, [](const State& state) { return state.validFormat && state.complete; });

    std::vector<TrackResult> result;
    result.reserve(m_states.size());

    for(size_t index{0}; index < m_states.size(); ++index) {
        const State& state = m_states.at(index);
        TrackResult trackResult{.track        = state.track,
                                .status       = !state.validFormat ? VerifyStatus::InvalidFormat
                                              : !albumComplete     ? VerifyStatus::Incomplete
                                                                   : VerifyStatus::Mismatch,
                                .crcV1        = state.crcV1,
                                .crcV2        = state.crcV2,
                                .confidence   = 0,
                                .sampleOffset = m_sampleOffset,
                                .databaseCrcs = {}};
        if(albumComplete) {
            for(const Pressing& pressing : m_pressings) {
                if(index >= pressing.size()) {
                    continue;
                }

                const Checksum& expected = pressing.at(index);
                if(!std::ranges::contains(trackResult.databaseCrcs, expected.crc)) {
                    trackResult.databaseCrcs.push_back(expected.crc);
                }
                if(state.crcV1 == expected.crc || state.crcV2 == expected.crc) {
                    trackResult.status = VerifyStatus::Verified;
                    trackResult.confidence += static_cast<int>(expected.confidence);
                }
            }
        }

        result.push_back(std::move(trackResult));
    }

    return result;
}
} // namespace Fooyin::AccurateRip
