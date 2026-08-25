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
 *
 */

#include "metadatamatcher.h"

#include <utils/stringutils.h>

#include <QRegularExpression>

#include <cmath>
#include <set>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
int leadingNumber(const QString& value)
{
    static const QRegularExpression expression{uR"(^\s*(\d+))"_s};
    const auto match = expression.match(value);
    return match.hasMatch() ? match.captured(1).toInt() : 0;
}

int candidateScore(const Track& local, const ReleaseTrack& remote, size_t localIndex, size_t remoteIndex,
                   size_t localCount, size_t remoteCount)
{
    int score{0};

    const QString localTitle  = normaliseMatchText(local.effectiveTitle());
    const QString remoteTitle = normaliseMatchText(remote.title);
    if(!localTitle.isEmpty() && !remoteTitle.isEmpty()) {
        score += (Utils::similarityRatio(localTitle, remoteTitle, Qt::CaseInsensitive) * 55) / 100;
    }

    if(local.duration() > 0 && remote.durationMs > 0) {
        const auto difference = std::llabs(static_cast<int64_t>(local.duration()) - remote.durationMs);
        if(difference <= 2000) {
            score += 25;
        }
        else if(difference <= 10000) {
            score += static_cast<int>(25 - ((difference - 2000) * 15 / 8000));
        }
        else if(difference <= 30000) {
            score += 5;
        }
    }

    const int localTrack = leadingNumber(local.trackNumber());
    const int localDisc  = leadingNumber(local.discNumber());
    if(localTrack > 0 && localTrack == remote.position) {
        score += 15;
    }
    if(localDisc > 0 && localDisc == remote.mediumPosition) {
        score += 10;
    }
    if(localTrack > 0 && localDisc > 0 && (localTrack != remote.position || localDisc != remote.mediumPosition)) {
        score -= 25;
    }

    if(localCount > 1 && remoteCount > 1) {
        const double localPosition  = static_cast<double>(localIndex) / static_cast<double>(localCount - 1);
        const double remotePosition = static_cast<double>(remoteIndex) / static_cast<double>(remoteCount - 1);
        score += std::max(0, 10 - static_cast<int>(std::abs(localPosition - remotePosition) * 25.0));
    }

    return std::clamp(score, 0, 100);
}

struct Candidate
{
    size_t local{0};
    size_t remote{0};
    int score{0};
};
} // namespace

QString normaliseMatchText(const QString& text)
{
    QString result = text.normalized(QString::NormalizationForm_C).toCaseFolded();
    result.replace(QRegularExpression{uR"([^\p{L}\p{N}]+)"_s}, u" "_s);
    return result.simplified();
}

std::vector<TrackMatch> matchTracks(const TrackList& localTracks, const Release& release)
{
    const auto remoteTracks = flattenedTracks(release);

    std::vector<TrackMatch> matches(localTracks.size());
    for(size_t i{0}; i < matches.size(); ++i) {
        matches.at(i).localIndex = i;
    }

    if(localTracks.empty() || remoteTracks.empty()) {
        return matches;
    }

    std::vector<Candidate> candidates;
    candidates.reserve(localTracks.size() * remoteTracks.size());

    std::vector scores(localTracks.size(), std::vector<int>(remoteTracks.size()));
    for(size_t local{0}; local < localTracks.size(); ++local) {
        for(size_t remote{0}; remote < remoteTracks.size(); ++remote) {
            const int score             = candidateScore(localTracks.at(local), *remoteTracks.at(remote), local, remote,
                                                         localTracks.size(), remoteTracks.size());
            scores.at(local).at(remote) = score;
            candidates.emplace_back(local, remote, score);
        }
    }

    std::ranges::sort(candidates, [](const Candidate& lhs, const Candidate& rhs) {
        if(lhs.score != rhs.score) {
            return lhs.score > rhs.score;
        }
        if(lhs.local != rhs.local) {
            return lhs.local < rhs.local;
        }
        return lhs.remote < rhs.remote;
    });

    std::set<size_t> assignedLocal;
    std::set<size_t> assignedRemote;

    for(const Candidate& candidate : candidates) {
        if(candidate.score < 35 || assignedLocal.contains(candidate.local)
           || assignedRemote.contains(candidate.remote)) {
            continue;
        }

        TrackMatch& match = matches.at(candidate.local);
        match.remoteIndex = candidate.remote;
        match.confidence  = candidate.score;

        int secondBest{0};
        for(size_t other{0}; other < remoteTracks.size(); ++other) {
            if(other != candidate.remote) {
                secondBest = std::max(secondBest, scores.at(candidate.local).at(other));
            }
        }

        match.ambiguous = candidate.score < 55 || candidate.score - secondBest < 8;
        assignedLocal.emplace(candidate.local);
        assignedRemote.emplace(candidate.remote);
    }

    return matches;
}

std::vector<TrackMatch> matchTracksByPosition(const TrackList& localTracks, const Release& release)
{
    const auto remoteTracks = flattenedTracks(release);

    std::vector<TrackMatch> matches(localTracks.size());

    for(size_t localIndex{0}; localIndex < localTracks.size(); ++localIndex) {
        TrackMatch& match = matches.at(localIndex);
        match.localIndex  = localIndex;

        const int trackNumber = leadingNumber(localTracks.at(localIndex).trackNumber());
        if(trackNumber <= 0) {
            continue;
        }

        const auto remote = std::ranges::find(remoteTracks, trackNumber, &ReleaseTrack::position);
        if(remote == remoteTracks.cend()) {
            continue;
        }

        match.remoteIndex = static_cast<size_t>(std::distance(remoteTracks.cbegin(), remote));
        match.confidence  = 100;
    }

    return matches;
}
} // namespace Fooyin
