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

#include "accuraterip.h"

#include "cddatoc.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QTextDocumentFragment>
#include <QtEndian>

#include <cstring>

using namespace Qt::StringLiterals;

namespace Fooyin::Cdda {
namespace {
uint32_t digitSum(uint32_t value)
{
    uint32_t result{0};

    while(true) {
        result += value % 10;
        value /= 10;

        if(value == 0) {
            break;
        }
    }

    return result;
}

uint32_t readLe32(const QByteArray& data, qsizetype offset)
{
    return qFromLittleEndian<uint32_t>(data.constData() + offset);
}

QString normalisedDriveName(QString value)
{
    value = value.simplified().toUpper();

    value.replace("HL-DT-ST"_L1, "LG ELECTRONICS"_L1);
    value.replace("MATSHITA"_L1, "PANASONIC"_L1);
    value.replace("JLMS"_L1, "LITE-ON"_L1);

    static const QRegularExpression regex{u"[^A-Z0-9]"_s};
    value.remove(regex);

    return value;
}
} // namespace

std::expected<AccurateRipDiscId, QString> accurateRipDiscId(const CdToc& toc)
{
    if(const auto valid = validateToc(toc); !valid) {
        return std::unexpected(invalidTocUserMessage());
    }
    if(!std::ranges::all_of(toc.tracks, &CdTocTrack::isAudio)) {
        return std::unexpected(QCoreApplication::translate(
            "Fooyin::Cdda::AccurateRip", "AccurateRip verification of mixed-mode discs is not supported"));
    }

    AccurateRipDiscId result;
    result.trackCount = static_cast<int>(toc.tracks.size());

    for(int index{0}; std::cmp_less(index, toc.tracks.size()); ++index) {
        const auto offset = static_cast<uint32_t>(toc.tracks.at(index).firstSector);
        result.id1 += offset;
        result.id2 += std::max(offset, 1U) * static_cast<uint32_t>(index + 1);
    }

    const auto leadout = static_cast<uint32_t>(toc.leadoutSector);
    result.id1 += leadout;
    result.id2 += std::max(leadout, 1U) * static_cast<uint32_t>(result.trackCount + 1);

    uint32_t cddbSum{0};
    for(const CdTocTrack& track : toc.tracks) {
        cddbSum += digitSum(static_cast<uint32_t>((track.firstSector + LeadInSectors) / SectorsPerSecond));
    }

    const auto duration = static_cast<uint32_t>((toc.leadoutSector / SectorsPerSecond)
                                                - (toc.tracks.front().firstSector / SectorsPerSecond));
    result.cddbId       = ((cddbSum % 0xFFU) << 24) | (duration << 8) | static_cast<uint32_t>(result.trackCount);

    return result;
}

QUrl accurateRipDiscUrl(const AccurateRipDiscId& id)
{
    const QString id1 = QString::number(id.id1, 16).rightJustified(8, u'0');
    return QUrl{u"https://www.accuraterip.com/accuraterip/%1/%2/%3/dBAR-%4-%5-%6-%7.bin"_s.arg(id1.at(7))
                    .arg(id1.at(6))
                    .arg(id1.at(5))
                    .arg(id.trackCount, 3, 10, u'0')
                    .arg(id.id1, 8, 16, u'0')
                    .arg(id.id2, 8, 16, u'0')
                    .arg(id.cddbId, 8, 16, u'0')};
}

std::expected<std::vector<AccurateRipPressing>, QString> parseAccurateRipResponse(const QByteArray& data,
                                                                                  const AccurateRipDiscId& expected)
{
    std::vector<AccurateRipPressing> result;

    qsizetype offset{0};
    while(offset < data.size()) {
        if(data.size() - offset < 13) {
            return std::unexpected(
                QCoreApplication::translate("Fooyin::Cdda::AccurateRip", "Truncated AccurateRip response header"));
        }

        const auto tracks   = static_cast<uint8_t>(data.at(offset));
        const uint32_t id1  = readLe32(data, offset + 1);
        const uint32_t id2  = readLe32(data, offset + 5);
        const uint32_t cddb = readLe32(data, offset + 9);

        offset += 13;

        if(tracks < 1 || tracks > MaximumTracks || data.size() - offset < tracks * 9LL) {
            return std::unexpected(
                QCoreApplication::translate("Fooyin::Cdda::AccurateRip", "Invalid AccurateRip response record"));
        }

        AccurateRipPressing pressing;
        pressing.reserve(tracks);

        for(int track{0}; std::cmp_less(track, tracks); ++track) {
            pressing.push_back({.confidence = static_cast<uint8_t>(data.at(offset)),
                                .crc        = readLe32(data, offset + 1),
                                .crc2       = readLe32(data, offset + 5)});
            offset += 9;
        }

        if(std::cmp_equal(tracks, expected.trackCount) && id1 == expected.id1 && id2 == expected.id2
           && cddb == expected.cddbId) {
            result.push_back(std::move(pressing));
        }
    }

    if(result.empty()) {
        return std::unexpected(QCoreApplication::translate("Fooyin::Cdda::AccurateRip",
                                                           "AccurateRip response did not contain the requested disc"));
    }

    return result;
}

std::vector<AccurateRipDriveOffset> parseAccurateRipDriveOffsets(const QByteArray& html)
{
    static const QRegularExpression row{uR"(<tr\b[^>]*>(.*?)</tr\s*>)"_s,
                                        QRegularExpression::CaseInsensitiveOption
                                            | QRegularExpression::DotMatchesEverythingOption};
    static const QRegularExpression cell{uR"(<t[dh]\b[^>]*>(.*?)</t[dh]\s*>)"_s,
                                         QRegularExpression::CaseInsensitiveOption
                                             | QRegularExpression::DotMatchesEverythingOption};
    static const QRegularExpression signedInteger{uR"(^[+-]?\d+$)"_s};
    static const QRegularExpression unsignedInteger{uR"(^\d+$)"_s};
    static const QRegularExpression percentage{uR"(^(\d+)\s*%$)"_s};

    std::vector<AccurateRipDriveOffset> result;

    auto rowMatch = row.globalMatch(QString::fromUtf8(html));
    while(rowMatch.hasNext()) {
        std::vector<QString> cells;

        const QString rowHtml = rowMatch.next().captured(1);
        auto cellMatch        = cell.globalMatch(rowHtml);
        while(cellMatch.hasNext()) {
            const QString cellHtml = cellMatch.next().captured(1);
            cells.push_back(QTextDocumentFragment::fromHtml(cellHtml).toPlainText().simplified());
        }

        if(cells.size() < 2 || cells.front().isEmpty()) {
            continue;
        }

        const QString& correctionText = cells.at(1);
        const bool purged             = correctionText.compare("[Purged]"_L1, Qt::CaseInsensitive) == 0;
        if(purged) {
            result.push_back({.name = std::move(cells.front()), .purged = true});
            continue;
        }
        if(cells.size() < 4 || !signedInteger.match(correctionText).hasMatch()
           || !unsignedInteger.match(cells.at(2)).hasMatch()) {
            continue;
        }

        const auto percentageMatch = percentage.match(cells.at(3));
        if(!percentageMatch.hasMatch()) {
            continue;
        }

        bool correctionOk{false};
        bool submissionsOk{false};
        bool percentageOk{false};
        const int correction       = correctionText.toInt(&correctionOk);
        const int submissions      = cells.at(2).toInt(&submissionsOk);
        const int agreementPercent = percentageMatch.captured(1).toInt(&percentageOk);
        if(!correctionOk || !submissionsOk || !percentageOk || agreementPercent > 100) {
            continue;
        }

        result.push_back({.name                   = std::move(cells.front()),
                          .correctionSampleFrames = correction,
                          .submissions            = submissions,
                          .agreementPercent       = agreementPercent,
                          .purged                 = false});
    }

    return result;
}

std::optional<AccurateRipDriveOffset> findAccurateRipDriveOffset(const std::vector<AccurateRipDriveOffset>& offsets,
                                                                 const QString& vendor, const QString& model)
{
    std::optional<AccurateRipDriveOffset> result;

    const QString identity = normalisedDriveName(vendor + u' ' + model);
    for(const auto& offset : offsets) {
        if(normalisedDriveName(offset.name) != identity) {
            continue;
        }

        if(result
           && (result->correctionSampleFrames != offset.correctionSampleFrames || result->purged != offset.purged)) {
            return {};
        }

        if(!result || offset.submissions > result->submissions) {
            result = offset;
        }
    }

    return result;
}

AccurateRipVerifier::AccurateRipVerifier(CdToc toc, std::vector<AccurateRipPressing> pressings)
    : m_toc{std::move(toc)}
    , m_pressings{std::move(pressings)}
{ }

AccurateRipVerifier::~AccurateRipVerifier() = default;

AccurateRipVerifier::State* AccurateRipVerifier::stateFor(const Track& track)
{
    const auto state
        = std::ranges::find_if(m_states, [&track](const State& value) { return value.track.sameIdentityAs(track); });
    return state == m_states.end() ? nullptr : &*state;
}

void AccurateRipVerifier::trackStarted(const Track& track, const AudioFormat& format)
{
    const std::scoped_lock lock{m_mutex};

    const int index       = track.subsong();
    const bool validIndex = index >= 0 && std::cmp_less(index, m_toc.tracks.size());
    const uint64_t frames
        = validIndex
            ? static_cast<uint64_t>(m_toc.tracks.at(index).endSectorExclusive - m_toc.tracks.at(index).firstSector)
                  * FramesPerSector
            : 0;

    m_states.push_back({.track       = track,
                        .totalFrames = frames,
                        .validFormat = validIndex && format.sampleFormat() == SampleFormat::S16
                                    && format.sampleRate() == 44100 && format.channelCount() == 2});
}

void AccurateRipVerifier::sourceAudio(const Track& track, const AudioBuffer& buffer)
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
        const uint64_t position                   = ++state->position;
        constexpr uint64_t ChecksumBoundaryFrames = 5L * FramesPerSector;
        const uint64_t first                      = track.subsong() == 0 ? ChecksumBoundaryFrames : 0;
        const uint64_t last = std::cmp_equal(track.subsong() + 1, m_toc.tracks.size())
                                ? state->totalFrames - std::min(state->totalFrames, ChecksumBoundaryFrames)
                                : state->totalFrames;

        if(position >= first && position <= last) {
            const uint64_t product = static_cast<uint64_t>(sample) * position;
            state->crcV1 += static_cast<uint32_t>(product);
            state->crcV2 += static_cast<uint32_t>(product) + static_cast<uint32_t>(product >> 32);
        }
    }
}

void AccurateRipVerifier::trackFinished(const Track& track, bool complete)
{
    const std::scoped_lock lock{m_mutex};

    if(State* state = stateFor(track)) {
        state->complete = complete && state->position == state->totalFrames;
    }
}

std::vector<AccurateRipTrackResult> AccurateRipVerifier::results() const
{
    const std::scoped_lock lock{m_mutex};

    std::vector<AccurateRipTrackResult> result;
    result.reserve(m_states.size());

    for(const State& state : m_states) {
        AccurateRipTrackResult trackResult{.track        = state.track,
                                           .status       = AccurateRipVerifyStatus::Incomplete,
                                           .crcV1        = state.crcV1,
                                           .crcV2        = state.crcV2,
                                           .confidence   = 0,
                                           .databaseCrcs = {}};
        if(!state.validFormat) {
            trackResult.status = AccurateRipVerifyStatus::InvalidFormat;
        }
        else if(!state.complete) {
            trackResult.status = AccurateRipVerifyStatus::Incomplete;
        }
        else {
            trackResult.status = AccurateRipVerifyStatus::Mismatch;
            const int index    = state.track.subsong();
            for(const auto& pressing : m_pressings) {
                if(index < 0 || std::cmp_greater_equal(index, pressing.size())) {
                    continue;
                }
                const auto& expected = pressing.at(index);
                if(!std::ranges::contains(trackResult.databaseCrcs, expected.crc)) {
                    trackResult.databaseCrcs.push_back(expected.crc);
                }
                if(state.crcV1 == expected.crc || state.crcV2 == expected.crc) {
                    trackResult.status = AccurateRipVerifyStatus::Verified;
                    trackResult.confidence += static_cast<int>(expected.confidence);
                }
            }
        }
        result.push_back(std::move(trackResult));
    }

    return result;
}
} // namespace Fooyin::Cdda
