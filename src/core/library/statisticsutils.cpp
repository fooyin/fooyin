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

#include "statisticsutils.h"

#include <core/scripting/scriptparser.h>
#include <core/scripting/scriptregistry.h>
#include <utils/crypto.h>

#include <limits>

using namespace Qt::StringLiterals;

namespace Fooyin {
namespace {
bool statisticsField(const QString& name)
{
    const auto kind = ScriptRegistry::instance().resolveVariable(name.toUpper());
    return kind == VariableKind::PlayCount || kind == VariableKind::FirstPlayed || kind == VariableKind::LastPlayed
        || kind == VariableKind::AddedTime || kind == VariableKind::Rating || kind == VariableKind::RatingNormalized
        || kind == VariableKind::Stars || kind == VariableKind::RatingStars || kind == VariableKind::RatingStarsPadded
        || kind == VariableKind::RatingEditor || kind == VariableKind::Loved || kind == VariableKind::LoveEditor;
}

std::optional<QString> literalText(const Expression& expression)
{
    if(expression.type == Expr::Literal || expression.type == Expr::QuotedLiteral) {
        if(const auto* text = std::get_if<QString>(&expression.value)) {
            return *text;
        }
    }

    if(const auto* children = std::get_if<ExpressionList>(&expression.value)) {
        QString result;
        for(const auto& child : *children) {
            const auto text = literalText(child);
            if(!text) {
                return {};
            }
            result += *text;
        }
        return result;
    }
    return {};
}

bool safeExpressions(const ExpressionList& expressions)
{
    for(const auto& expression : expressions) {
        if((expression.type == Expr::Variable || expression.type == Expr::VariableList
            || expression.type == Expr::VariableRaw)
           && statisticsField(std::get<QString>(expression.value))) {
            return false;
        }

        if(const auto* children = std::get_if<ExpressionList>(&expression.value);
           children && !safeExpressions(*children)) {
            return false;
        }

        if(const auto* function = std::get_if<FuncValue>(&expression.value)) {
            const QString name = function->name.toLower();
            if(name == u"rand" || name == u"random" || !safeExpressions(function->args)) {
                return false;
            }

            if(name == u"meta" || name == u"meta_sep" || name == u"meta_num" || name == u"meta_test"
               || name == u"info") {
                const size_t fieldCount
                    = name == u"meta_test" ? function->args.size() : std::min(size_t{1}, function->args.size());
                if(fieldCount == 0) {
                    return false;
                }
                for(size_t i{0}; i < fieldCount; ++i) {
                    const auto field = literalText(function->args.at(i));
                    if(!field || statisticsField(*field)) {
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

uint64_t earliest(uint64_t first, uint64_t second)
{
    return first == 0 ? second : second == 0 ? first : std::min(first, second);
}
} // namespace

StatisticsProgress::StatisticsProgress(QPromise<PlaybackStatisticsResult>* promise)
    : m_promise{promise}
    , m_maximum{0}
{ }

void StatisticsProgress::start(const QString& text, int maximum)
{
    if(!m_promise) {
        return;
    }

    m_text    = text;
    m_maximum = maximum;
    m_timer.start();

    // QPromise only updates the text when the value increases
    m_promise->setProgressRange(-1, maximum);
    m_promise->setProgressValueAndText(0, text);
    m_promise->setProgressRange(0, maximum);
}

void StatisticsProgress::update(int value)
{
    if(m_promise && (value == m_maximum || m_timer.hasExpired(100))) {
        m_promise->setProgressValueAndText(value, m_text);
        m_timer.restart();
    }
}

QString defaultPlaybackStatisticsScript()
{
    return uR"($meta_sep(artist,",")%album%%disc%%track%$if2($meta(title),%directory%%filename%)%subsong%)"_s;
}

QString PlaybackStatistics::validateScript(const QString& script)
{
    if(script.isEmpty()) {
        return {};
    }

    ScriptParser parser;
    const auto parsed = parser.parse(script);
    if(!parsed.isValid() || ScriptParser::containsQueryExpression(parsed)) {
        return tr("Enter a valid track metadata script.");
    }

    const auto dependencies = parser.dependencies(parsed);
    if(dependencies.unclassified
       || std::ranges::any_of(dependencies.sources, [](auto source) { return source != ScriptSources::TrackMetadata; })
       || !safeExpressions(parsed.expressions)) {
        return tr("The statistics script must use only track metadata and deterministic functions. "
                  "Playback statistics and changing playback or clock values are unavailable.");
    }

    return {};
}

QString PlaybackStatistics::evaluateKey(const PlaybackStatisticsConfig& config, const Track& track, bool* fallback)
{
    if(fallback) {
        *fallback = false;
    }
    if(config.script.isEmpty()) {
        return track.hash();
    }

    thread_local ScriptParser parser;

    const QString value = parser.evaluate(config.script, track);
    if(value.isEmpty()) {
        if(fallback) {
            *fallback = true;
        }
        return track.hash();
    }

    return Utils::generateHash(value);
}

Track PlaybackStatistics::combineStatistics(const TrackList& tracks, const PlaybackStatisticsConfig& config)
{
    Track combined;

    int playcount{0};
    float rating{-1.0F};
    bool loved = (config.loved == LovedMergePolicy::All);
    uint64_t added{0};
    uint64_t first{0};
    uint64_t last{0};

    for(const auto& track : tracks) {
        added = earliest(added, track.addedTime());
        first = earliest(first, track.firstPlayed());
        last  = std::max(last, track.lastPlayed());

        if(config.playcount == PlaycountMergePolicy::Sum) {
            playcount += std::max(track.playCount(), 0);
        }
        else if(config.playcount == PlaycountMergePolicy::Maximum) {
            playcount = std::max(playcount, track.playCount());
        }

        if(config.rating != RatingMergePolicy::Clear && track.rating() > 0) {
            rating = rating < 0                                  ? track.rating()
                   : config.rating == RatingMergePolicy::Highest ? std::max(rating, track.rating())
                                                                 : std::min(rating, track.rating());
        }

        if(config.loved == LovedMergePolicy::Any) {
            loved |= track.isLoved();
        }
        else if(config.loved == LovedMergePolicy::All) {
            loved &= track.isLoved();
        }
    }

    combined.setAddedTime(added);
    combined.setFirstPlayed(config.clearFirstPlayed ? 0 : first);
    combined.setLastPlayed(config.clearLastPlayed ? 0 : last);
    combined.setPlayCount(playcount);
    combined.setRating(rating);
    combined.setLoved(loved);

    return combined;
}
} // namespace Fooyin
