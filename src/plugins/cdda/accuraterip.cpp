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

using namespace Qt::StringLiterals;

namespace Fooyin::Cdda {
namespace {
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

std::expected<AccurateRip::DiscLayout, QString> accurateRipLayout(const CdToc& toc)
{
    if(const auto valid = validateToc(toc); !valid) {
        return std::unexpected(invalidTocUserMessage());
    }

    if(!std::ranges::all_of(toc.tracks, &CdTocTrack::isAudio)) {
        return std::unexpected(QCoreApplication::translate(
            "Fooyin::Cdda::AccurateRip", "AccurateRip verification of mixed-mode discs is not supported"));
    }

    AccurateRip::DiscLayout layout;
    layout.leadoutSector = static_cast<uint32_t>(toc.leadoutSector);
    layout.tracks.reserve(toc.tracks.size());

    for(const CdTocTrack& track : toc.tracks) {
        layout.tracks.push_back({.firstSector        = static_cast<uint32_t>(track.firstSector),
                                 .endSectorExclusive = static_cast<uint32_t>(track.endSectorExclusive)});
    }

    if(const auto valid = AccurateRip::validateLayout(layout); !valid) {
        return std::unexpected(valid.error());
    }

    return layout;
}

std::expected<AccurateRip::DiscId, QString> accurateRipDiscId(const CdToc& toc)
{
    const auto layout = accurateRipLayout(toc);
    return layout ? AccurateRip::discId(*layout) : std::unexpected(layout.error());
}

QUrl accurateRipDiscUrl(const AccurateRip::DiscId& id)
{
    return AccurateRip::discUrl(id);
}

std::expected<std::vector<AccurateRip::Pressing>, QString> parseAccurateRipResponse(const QByteArray& data,
                                                                                    const AccurateRip::DiscId& expected)
{
    return AccurateRip::parseResponse(data, expected);
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
            cells.push_back(QTextDocumentFragment::fromHtml(cellMatch.next().captured(1)).toPlainText().simplified());
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
                          .agreementPercent       = agreementPercent});
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
} // namespace Fooyin::Cdda
