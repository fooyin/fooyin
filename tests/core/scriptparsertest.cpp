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

#include <core/constants.h>
#include <core/ratingsymbols.h>
#include <core/scripting/scriptparser.h>
#include <core/scripting/scriptproviders.h>
#include <core/scripting/scripttrackwriter.h>
#include <core/track.h>

#include <gtest/gtest.h>

#include <QDateTime>
#include <QDir>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
class ScriptParserTest : public ::testing::Test
{
protected:
    ScriptParser m_parser;
};

static const StaticScriptVariableProvider ProviderVariableProvider{makeScriptVariableDescriptor<[]() -> ScriptResult {
    return {.value = QString::fromLatin1(Constants::FrontCover), .cond = true};
}>(VariableKind::FrontCover, u"FRONTCOVER"_s)};

class TestProviderEnvironment : public ScriptEnvironment
{
public:
    void setState(QString variableValue, QString functionPrefix)
    {
        m_variableValue  = std::move(variableValue);
        m_functionPrefix = std::move(functionPrefix);
    }

    [[nodiscard]] QString variableValue() const
    {
        return m_variableValue;
    }

    [[nodiscard]] QString functionPrefix() const
    {
        return m_functionPrefix;
    }

private:
    QString m_variableValue;
    QString m_functionPrefix;
};

static const StaticScriptVariableProvider ContextVariableProvider{
    makeScriptVariableDescriptor<[](const ScriptContext& context, const QString&) -> ScriptResult {
        const auto* environment = dynamic_cast<const TestProviderEnvironment*>(context.environment);
        if(environment == nullptr || environment->variableValue().isEmpty()) {
            return {};
        }
        return {.value = environment->variableValue(), .cond = true};
    }>(VariableKind::FrontCover, u"STATEFULVAR"_s)};

static constexpr auto PrefixFunction = [](const ScriptFunctionCallContext& call) -> ScriptResult {
    if(call.args.empty()) {
        return {};
    }

    const QString value = u"fn:%1"_s.arg(call.args.front().value);
    return {.value = value, .cond = true};
};

static const StaticScriptFunctionProvider ProviderFunctionProvider{
    makeScriptFunctionDescriptor<PrefixFunction>(u"prefix"_s)};

static constexpr auto ListIndexVariable = [](const ScriptContext& context, const QString&) -> ScriptResult {
    const auto* environment = context.environment ? context.environment->playlistEnvironment() : nullptr;
    if(!environment || environment->currentPlaylistTrackIndex() < 0) {
        return {};
    }

    return {.value = QString::number(environment->currentPlaylistTrackIndex() + 1), .cond = true};
};

static constexpr auto QueueIndexesVariable = [](const ScriptContext& context, const QString&) -> ScriptResult {
    const auto* environment = context.environment ? context.environment->playlistEnvironment() : nullptr;
    if(!environment) {
        return {};
    }

    QStringList values;
    for(const int index : environment->currentQueueIndexes()) {
        values.push_back(QString::number(index + 1));
    }
    const QString value = values.join(u", "_s);
    return {.value = value, .cond = !value.isEmpty()};
};

static constexpr auto QueueIndexVariable = [](const ScriptContext& context, const QString&) -> ScriptResult {
    const auto* environment = context.environment ? context.environment->playlistEnvironment() : nullptr;
    if(!environment || environment->currentQueueIndexes().empty()) {
        return {};
    }

    return {.value = QString::number(environment->currentQueueIndexes().front() + 1), .cond = true};
};

static constexpr auto QueueTotalVariable = [](const ScriptContext& context, const QString&) -> ScriptResult {
    const auto* environment = context.environment ? context.environment->playlistEnvironment() : nullptr;
    if(!environment || environment->currentQueueIndexes().empty()) {
        return {};
    }

    return {.value = QString::number(environment->currentQueueTotal()), .cond = environment->currentQueueTotal() > 0};
};

static constexpr auto DepthVariable = [](const ScriptContext& context, const QString&) -> ScriptResult {
    const auto* environment = context.environment ? context.environment->playlistEnvironment() : nullptr;
    if(!environment) {
        return {};
    }

    return {.value = QString::number(environment->trackDepth()), .cond = true};
};

static const StaticScriptFunctionProvider ContextFunctionProvider{
    makeScriptFunctionDescriptor<[](const ScriptFunctionCallContext& call) -> ScriptResult {
        if(call.args.empty()) {
            return {};
        }

        const auto* environment
            = call.context ? dynamic_cast<const TestProviderEnvironment*>(call.context->environment) : nullptr;
        if(environment == nullptr || environment->functionPrefix().isEmpty()) {
            return {};
        }

        const QString value = u"%1%2"_s.arg(environment->functionPrefix(), call.args.front().value);
        return {.value = value, .cond = true};
    }>(u"stateful_prefix"_s)};

class TestPlaylistEnvironment : public ScriptEnvironment,
                                public ScriptPlaylistEnvironment,
                                public ScriptTrackListEnvironment,
                                public ScriptPlaybackEnvironment,
                                public ScriptLibraryEnvironment,
                                public ScriptEvaluationEnvironment
{
public:
    void setState(int playlistTrackIndex, int playlistTrackCount, int trackDepth, std::vector<int> queueIndexes,
                  int queueTotal = 0)
    {
        m_playlistTrackIndex = playlistTrackIndex;
        m_playlistTrackCount = playlistTrackCount;
        m_trackDepth         = trackDepth;
        m_queueIndexes       = std::move(queueIndexes);
        m_queueTotal         = queueTotal;
    }

    void setTrackList(const TrackList* tracks)
    {
        m_tracks = tracks;
    }

    void setPlaybackState(uint64_t currentPosition, uint64_t currentTrackDuration, int bitrate,
                          Player::PlayState playState)
    {
        m_currentPosition      = currentPosition;
        m_currentTrackDuration = currentTrackDuration;
        m_bitrate              = bitrate;
        m_playState            = playState;
    }

    void setLibraryState(QString libraryName, QString libraryPath)
    {
        m_libraryName = std::move(libraryName);
        m_libraryPath = std::move(libraryPath);
    }

    void setEvaluationState(TrackListContextPolicy policy, QString placeholder = {}, bool escapeRichText = false,
                            bool useVariousArtists = false, QString fullStarSymbol = {}, QString halfStarSymbol = {},
                            QString emptyStarSymbol = {}, bool replacePathSeparators = false)
    {
        m_trackListContextPolicy = policy;
        m_trackListPlaceholder   = std::move(placeholder);
        m_escapeRichText         = escapeRichText;
        m_useVariousArtists      = useVariousArtists;
        m_ratingSymbols          = {std::move(fullStarSymbol), std::move(halfStarSymbol), std::move(emptyStarSymbol)};
        m_replacePathSeparators  = replacePathSeparators;
    }

    [[nodiscard]] const ScriptPlaylistEnvironment* playlistEnvironment() const override
    {
        return this;
    }

    [[nodiscard]] const ScriptTrackListEnvironment* trackListEnvironment() const override
    {
        return this;
    }

    [[nodiscard]] const ScriptPlaybackEnvironment* playbackEnvironment() const override
    {
        return this;
    }

    [[nodiscard]] const ScriptLibraryEnvironment* libraryEnvironment() const override
    {
        return this;
    }

    [[nodiscard]] const ScriptEvaluationEnvironment* evaluationEnvironment() const override
    {
        return this;
    }

    [[nodiscard]] int currentPlaylistTrackIndex() const override
    {
        return m_playlistTrackIndex;
    }

    [[nodiscard]] int playlistTrackCount() const override
    {
        return m_playlistTrackCount;
    }

    [[nodiscard]] int trackDepth() const override
    {
        return m_trackDepth;
    }

    [[nodiscard]] std::span<const int> currentQueueIndexes() const override
    {
        return m_queueIndexes;
    }

    [[nodiscard]] int currentQueueTotal() const override
    {
        return m_queueTotal;
    }

    [[nodiscard]] const TrackList* trackList() const override
    {
        return m_tracks;
    }

    [[nodiscard]] uint64_t currentPosition() const override
    {
        return m_currentPosition;
    }

    [[nodiscard]] uint64_t currentTrackDuration() const override
    {
        return m_currentTrackDuration;
    }

    [[nodiscard]] int bitrate() const override
    {
        return m_bitrate;
    }

    [[nodiscard]] Player::PlayState playState() const override
    {
        return m_playState;
    }

    [[nodiscard]] QString libraryName(const Track&) const override
    {
        return m_libraryName;
    }

    [[nodiscard]] QString libraryPath(const Track&) const override
    {
        return m_libraryPath;
    }

    [[nodiscard]] QString relativePath(const Track& track) const override
    {
        return m_libraryPath.isEmpty() ? QString{} : QDir{m_libraryPath}.relativeFilePath(track.prettyFilepath());
    }

    [[nodiscard]] TrackListContextPolicy trackListContextPolicy() const override
    {
        return m_trackListContextPolicy;
    }

    [[nodiscard]] QString trackListPlaceholder() const override
    {
        return m_trackListPlaceholder;
    }

    [[nodiscard]] bool escapeRichText() const override
    {
        return m_escapeRichText;
    }

    [[nodiscard]] bool useVariousArtists() const override
    {
        return m_useVariousArtists;
    }

    [[nodiscard]] bool replacePathSeparators() const override
    {
        return m_replacePathSeparators;
    }

    [[nodiscard]] RatingStarSymbols ratingStarSymbols() const override
    {
        RatingStarSymbols symbols{m_ratingSymbols};
        const RatingStarSymbols defaultSymbols = ScriptEvaluationEnvironment::ratingStarSymbols();

        if(symbols.fullStarSymbol.isEmpty()) {
            symbols.fullStarSymbol = defaultSymbols.fullStarSymbol;
        }
        if(symbols.halfStarSymbol.isEmpty()) {
            symbols.halfStarSymbol = defaultSymbols.halfStarSymbol;
        }
        if(symbols.emptyStarSymbol.isEmpty()) {
            symbols.emptyStarSymbol = defaultSymbols.emptyStarSymbol;
        }

        return symbols;
    }

private:
    int m_playlistTrackIndex{-1};
    int m_playlistTrackCount{0};
    int m_trackDepth{0};
    std::vector<int> m_queueIndexes;
    int m_queueTotal{0};
    const TrackList* m_tracks{nullptr};
    uint64_t m_currentPosition{0};
    uint64_t m_currentTrackDuration{0};
    int m_bitrate{0};
    Player::PlayState m_playState{Player::PlayState::Stopped};
    QString m_libraryName;
    QString m_libraryPath;
    TrackListContextPolicy m_trackListContextPolicy{TrackListContextPolicy::Unresolved};
    QString m_trackListPlaceholder;
    bool m_escapeRichText{false};
    bool m_useVariousArtists{false};
    bool m_replacePathSeparators{false};
    RatingStarSymbols m_ratingSymbols;
};

static const StaticScriptVariableProvider EnvironmentVariableProvider{
    makeScriptVariableDescriptor<ListIndexVariable>(VariableKind::ListIndex, u"LIST_INDEX"_s),
    makeScriptVariableDescriptor<QueueIndexVariable>(VariableKind::QueueIndex, u"QUEUEINDEX"_s),
    makeScriptVariableDescriptor<QueueIndexVariable>(VariableKind::QueueIndex, u"QUEUE_INDEX"_s),
    makeScriptVariableDescriptor<QueueIndexesVariable>(VariableKind::QueueIndexes, u"QUEUEINDEXES"_s),
    makeScriptVariableDescriptor<QueueIndexesVariable>(VariableKind::QueueIndexes, u"QUEUE_INDEXES"_s),
    makeScriptVariableDescriptor<QueueTotalVariable>(VariableKind::QueueTotal, u"QUEUETOTAL"_s),
    makeScriptVariableDescriptor<QueueTotalVariable>(VariableKind::QueueTotal, u"QUEUE_TOTAL"_s),
    makeScriptVariableDescriptor<DepthVariable>(VariableKind::Depth, u"DEPTH"_s)};

TEST_F(ScriptParserTest, BasicLiteral)
{
    EXPECT_EQ(u"I am a test.", m_parser.evaluate(u"I am a test."_s));
    EXPECT_EQ(u"", m_parser.evaluate(QString{}));
}

TEST_F(ScriptParserTest, EscapeComment)
{
    EXPECT_EQ(u"I am a % test.", m_parser.evaluate(uR"("I am a \% test.")"_s));
    EXPECT_EQ(u"I am an \"escape test.", m_parser.evaluate(uR"("I am an \"escape test.")"_s));
}

TEST_F(ScriptParserTest, Quote)
{
    EXPECT_EQ(u"I %am% a $test$.", m_parser.evaluate(uR"("I %am% a $test$.")"_s));
}

TEST_F(ScriptParserTest, StringTest)
{
    EXPECT_EQ(u"01", m_parser.evaluate(u"$num(1,2)"_s));
    EXPECT_EQ(u"04", m_parser.evaluate(u"$num(04,2)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$num()"_s));
    EXPECT_EQ(u"01", m_parser.evaluate(u"[$num(1,2)]"_s));
    EXPECT_EQ(u"A replace cesc", m_parser.evaluate(u"$replace(A replace test,t,c)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$replace()"_s));
    EXPECT_EQ(u"test", m_parser.evaluate(u"$slice(A slice test,8)"_s));
    EXPECT_EQ(u"slice", m_parser.evaluate(u"$slice(A slice test,2,5)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$slice()"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$slice(1,2,3,4,5)"_s));
    EXPECT_EQ(u"A chop", m_parser.evaluate(u"$chop(A chop test,5)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$chop()"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$chop(1,2,3,4,5)"_s));
    EXPECT_EQ(u"L", m_parser.evaluate(u"$left(Left test,1)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$left(1,2,3,4,5)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$left()"_s));
    EXPECT_EQ(u"est", m_parser.evaluate(u"$right(Right test,3)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$right()"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$right(1,2,3,4,5)"_s));
    EXPECT_EQ(u"slice", m_parser.evaluate(u"$substr(A slice test,2,6)"_s));
    EXPECT_EQ(u"s", m_parser.evaluate(u"$substr(A slice test,2,2)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$substr()"_s));
    EXPECT_EQ(u"a  ", m_parser.evaluate(u"[$pad(a,3)]"_s));
    EXPECT_EQ(u"  a", m_parser.evaluate(u"[$padright(a,3)]"_s));
    EXPECT_EQ(u"x", m_parser.evaluate(u"[$if2(,x)]"_s));
    EXPECT_EQ(u"fallback", m_parser.evaluate(u"$if3(,,fallback)"_s));
    EXPECT_EQ(u"second", m_parser.evaluate(u"$if3(,second,third,fallback)"_s));
    EXPECT_EQ(u"winnerwinner",
              m_parser.evaluate(u"$if3(,$put(choice,winner),$put(choice,wrong),fallback)$get(choice)"_s));
    EXPECT_EQ(u"          X", m_parser.evaluate(u"$padright(,$mul($sub(3,1),5))X"_s));
    EXPECT_EQ(u"true", m_parser.evaluate(u"$if($stricmp(cmp,cMp),true,false)"_s));
    EXPECT_EQ(u"false", m_parser.evaluate(u"$if($strcmp(cmp,cMp),true,false)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$split(a;b;c,;)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$split(a;b;c,;,0)"_s));
    EXPECT_EQ(u"a", m_parser.evaluate(u"$split(a;b;c,;,1)"_s));
    EXPECT_EQ(u"b", m_parser.evaluate(u"$split(a;b;c,;,2)"_s));
    EXPECT_EQ(u"c", m_parser.evaluate(u"$split(a;b;c,;,3)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$split(a;b;c,;,4)"_s));

    EXPECT_EQ(u"true", m_parser.evaluate(u"$if($isalpha(abcXYZ),true,false)"_s));
    EXPECT_EQ(u"false", m_parser.evaluate(u"$if($isalpha(abc123),true,false)"_s));
    EXPECT_EQ(u"true", m_parser.evaluate(u"$if($isalnum(abc123),true,false)"_s));
    EXPECT_EQ(u"false", m_parser.evaluate(u"$if($isalnum(abc_123),true,false)"_s));
    EXPECT_EQ(u"true", m_parser.evaluate(u"$if($isnum(12345),true,false)"_s));
    EXPECT_EQ(u"false", m_parser.evaluate(u"$if($isnum(12a45),true,false)"_s));
    EXPECT_EQ(u"o", m_parser.evaluate(u"$ascii(ô)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$ascii(µ)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$ascii(μ)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$ascii(¥)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$ascii(©)"_s));
    EXPECT_EQ(u"Hello", m_parser.evaluate(u"$ascii(Hello)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$ascii()"_s));
}

TEST_F(ScriptParserTest, MathTest)
{
    EXPECT_EQ(3, m_parser.evaluate(u"$add(1,2)"_s).toInt());
    EXPECT_EQ(2, m_parser.evaluate(u"$sub(10,8)"_s).toInt());
    EXPECT_EQ(99, m_parser.evaluate(u"$mul(3,33)"_s).toInt());
    EXPECT_EQ(11, m_parser.evaluate(u"$div(33,3)"_s).toInt());
    EXPECT_EQ(1, m_parser.evaluate(u"$mod(10,3)"_s).toInt());
    EXPECT_EQ(u"3", m_parser.evaluate(u"[$add(1,2)]"_s));
    EXPECT_EQ(u"2", m_parser.evaluate(u"[$sub(10,8)]"_s));
    EXPECT_EQ(u"99", m_parser.evaluate(u"[$mul(3,33)]"_s));
    EXPECT_EQ(u"11", m_parser.evaluate(u"[$div(33,3)]"_s));
    EXPECT_EQ(u"1", m_parser.evaluate(u"[$mod(10,3)]"_s));
    EXPECT_EQ(2, m_parser.evaluate(u"$min(3,2,3,9,23,100,4)"_s).toInt());
    EXPECT_EQ(100, m_parser.evaluate(u"$max(3,2,3,9,23,100,4)"_s).toInt());
}

TEST_F(ScriptParserTest, TimeDateFunctionTest)
{
    EXPECT_EQ(u"2024", m_parser.evaluate(u"$year(\"2024-03-09 08:07:06\")"_s));
    EXPECT_EQ(u"03", m_parser.evaluate(u"$month(\"2024-03-09 08:07:06\")"_s));
    EXPECT_EQ(u"09", m_parser.evaluate(u"$day_of_month(\"2024-03-09 08:07:06\")"_s));
    EXPECT_EQ(u"2024-03-09", m_parser.evaluate(u"$date(\"2024-03-09 08:07:06\")"_s));
    EXPECT_EQ(u"08:07:06", m_parser.evaluate(u"$time(\"2024-03-09 08:07:06\")"_s));
    EXPECT_EQ(u"08:07", m_parser.evaluate(u"$time(\"2024-03-09 08:07\")"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$time(\"2024-03-09\")"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$year(not-a-date)"_s));
}

TEST_F(ScriptParserTest, ConditionalTest)
{
    EXPECT_EQ(u"", m_parser.evaluate(u"$and(1,1)"_s));
    EXPECT_EQ(u"true", m_parser.evaluate(u"$if($and(1,$strcmp(a,a),$stricmp(B,b)),true,false)"_s));
    EXPECT_EQ(u"false", m_parser.evaluate(u"$if($and(1,,2),true,false)"_s));
    EXPECT_EQ(u"true", m_parser.evaluate(u"$if($or(,,$strcmp(a,a)),true,false)"_s));
    EXPECT_EQ(u"false", m_parser.evaluate(u"$if($not($stricmp(a,a)),true,false)"_s));
    EXPECT_EQ(u"true", m_parser.evaluate(u"$if($not($strcmp(a,b)),true,false)"_s));
    EXPECT_EQ(u"true", m_parser.evaluate(u"$if($xor($strcmp(a,a),,),true,false)"_s));
    EXPECT_EQ(u"false", m_parser.evaluate(u"$if($xor($strcmp(a,a),$stricmp(b,b)),true,false)"_s));
    EXPECT_EQ(u"true", m_parser.evaluate(u"$if($xor($strcmp(a,a),,$stricmp(b,b),$strcmp(c,c)),true,false)"_s));
    EXPECT_EQ(u"true", m_parser.evaluate(u"$ifequal(1,1,true,false)"_s));
    EXPECT_EQ(u"false", m_parser.evaluate(u"$ifgreater(23,32,true,false)"_s));
    EXPECT_EQ(u"true", m_parser.evaluate(u"$iflonger(aaa,2,true,false)"_s));
    EXPECT_EQ(u"true", m_parser.evaluate(u"[$if(1,true,false)]"_s));
    EXPECT_EQ(u"first", m_parser.evaluate(u"[$if2(first,second)]"_s));
    EXPECT_EQ(u"true", m_parser.evaluate(u"[$ifequal(1,1,true,false)]"_s));
    EXPECT_EQ(u"true", m_parser.evaluate(u"[$ifgreater(5,3,true,false)]"_s));
    EXPECT_EQ(u"true", m_parser.evaluate(u"[$iflonger(aaa,2,true,false)]"_s));
    EXPECT_EQ(u"<A", m_parser.evaluate(u"$if(1,<A,X)"_s));
    EXPECT_EQ(u"<rgb=255,0,0>A", m_parser.evaluate(u"$if(1,<rgb=255,0,0>A,X)"_s));
    EXPECT_EQ(u"\\<A", m_parser.evaluate(u"$if(1,\\<A,X)"_s));
    EXPECT_EQ(u"\\<rgb=255,0,0>", m_parser.evaluate(u"$if(1,\\<rgb=255,0,0>,X)"_s));
}

TEST_F(ScriptParserTest, MetadataTest)
{
    Track track;
    track.setTitle(u"A Test"_s);

    EXPECT_EQ(u"A Test", m_parser.evaluate(u"%title%"_s, track));
    EXPECT_EQ(u"A Test", m_parser.evaluate(u"[%title%]"_s, track));
    EXPECT_EQ(u"A Test", m_parser.evaluate(u"%title%[ - %album%]"_s, track));

    track.setAlbum(u"A Test Album"_s);

    EXPECT_EQ(u"A Test Album", m_parser.evaluate(u"[%album%]"_s, track));
    EXPECT_EQ(u"A Test - A Test Album", m_parser.evaluate(u"%title%[ - %album%]"_s, track));

    track.setGenres({u"Pop"_s, u"Rock"_s});

    EXPECT_EQ(u"Pop, Rock", m_parser.evaluate(u"%genre%"_s, track));
    EXPECT_EQ(u"Pop\037Rock", m_parser.evaluate(u"%<genre>%"_s, track));

    track.setArtists({u"Me"_s, u"You"_s});

    EXPECT_EQ(u"Pop, Rock - Me, You", m_parser.evaluate(u"%genre% - %artist%"_s, track));
    EXPECT_EQ(u"Pop - Me\037Rock - Me\037Pop - You\037Rock - You",
              m_parser.evaluate(u"%<genre>% - %<artist>%"_s, track));

    track.setTrackNumber(u"7"_s);
    EXPECT_EQ(u"7", m_parser.evaluate(u"[%track%]"_s, track));
    EXPECT_EQ(u"07", m_parser.evaluate(u"$num(%track%,2)"_s, track));
    EXPECT_EQ(u"07", m_parser.evaluate(u"[$num(%track%,2)]"_s, track));
    EXPECT_EQ(u"07.  ", m_parser.evaluate(u"[$num(%track%,2).  ]"_s, track));

    const auto& defaultSymbols       = defaultRatingStarSymbols();
    const QString unratedPaddedStars = defaultSymbols.emptyStarSymbol.repeated(5);
    EXPECT_EQ(u"", m_parser.evaluate(u"%rating_stars%"_s, track));
    EXPECT_EQ(unratedPaddedStars, m_parser.evaluate(u"%rating_stars_padded%"_s, track));

    track.setRatingStars(7);
    const QString compactStars = defaultSymbols.fullStarSymbol.repeated(3) + defaultSymbols.halfStarSymbol;
    const QString paddedStars  = compactStars + defaultSymbols.emptyStarSymbol;
    EXPECT_EQ(compactStars, m_parser.evaluate(u"%rating_stars%"_s, track));
    EXPECT_EQ(paddedStars, m_parser.evaluate(u"%rating_stars_padded%"_s, track));
    EXPECT_EQ(u"3.5", m_parser.evaluate(u"%stars%"_s, track));
    EXPECT_EQ(u"7", m_parser.evaluate(u"%rating_editor%"_s, track));

    EXPECT_EQ(u"", m_parser.evaluate(u"%replaygain_track_gain%"_s, track));
    EXPECT_EQ(u"", m_parser.evaluate(u"%replaygain_album_gain%"_s, track));
    EXPECT_EQ(u"false", m_parser.evaluate(u"$if(%replaygain_track_gain%,true,false)"_s, track));
    EXPECT_EQ(u"false", m_parser.evaluate(u"$if(%replaygain_album_gain%,true,false)"_s, track));

    track.setRGTrackGain(-7.25F);
    track.setRGAlbumGain(-5.0F);

    EXPECT_EQ(u"-7.25 dB", m_parser.evaluate(u"%replaygain_track_gain%"_s, track));
    EXPECT_EQ(u"-5 dB", m_parser.evaluate(u"%replaygain_album_gain%"_s, track));
    EXPECT_EQ(u"true", m_parser.evaluate(u"$if(%replaygain_track_gain%,true,false)"_s, track));
    EXPECT_EQ(u"true", m_parser.evaluate(u"$if(%replaygain_album_gain%,true,false)"_s, track));

    EXPECT_EQ(u"", m_parser.evaluate(u"[%disc% - %track%]"_s, track));
}

TEST_F(ScriptParserTest, RatingStarsFormattingTest)
{
    ScriptParser parser;
    parser.addProvider(EnvironmentVariableProvider);

    TestPlaylistEnvironment environment;
    environment.setEvaluationState(TrackListContextPolicy::Unresolved, {}, false, false, u"*"_s, u"+"_s, u"."_s);

    ScriptContext context;
    context.environment = &environment;

    Track track;

    EXPECT_EQ(u"", parser.evaluate(u"%rating_stars%"_s, track, context));
    EXPECT_EQ(u".....", parser.evaluate(u"%rating_stars_padded%"_s, track, context));

    track.setRatingStars(7);
    EXPECT_EQ(u"***+", parser.evaluate(u"%rating_stars%"_s, track, context));
    EXPECT_EQ(u"***+.", parser.evaluate(u"%rating_stars_padded%"_s, track, context));
}

TEST_F(ScriptParserTest, RatingScriptWritesUseExpectedScales)
{
    Track track;

    setTrackScriptValue(u"rating"_s, 3.5F, track);
    EXPECT_FLOAT_EQ(track.rating(), 0.7F);

    setTrackScriptValue(u"stars"_s, u"4"_s, track);
    EXPECT_FLOAT_EQ(track.rating(), 0.8F);

    setTrackScriptValue(u"rating_normalized"_s, 0.6F, track);
    EXPECT_FLOAT_EQ(track.rating(), 0.6F);

    setTrackScriptValue(u"rating_normalized"_s, u"0.4"_s, track);
    EXPECT_FLOAT_EQ(track.rating(), 0.4F);

    setTrackScriptValue(u"rating_editor"_s, 0.5F, track);
    EXPECT_FLOAT_EQ(track.rating(), 0.5F);

    setTrackScriptValue(u"rating_stars"_s, u"7"_s, track);
    EXPECT_FLOAT_EQ(track.rating(), 0.7F);
}

TEST_F(ScriptParserTest, TrackListTest)
{
    TrackList tracks;

    Track track1;
    track1.setGenres({u"Pop"_s});
    track1.setDuration(2000);
    tracks.push_back(track1);

    Track track2;
    track2.setGenres({u"Rock"_s});
    track2.setDuration(3000);
    tracks.push_back(track2);

    EXPECT_EQ(u"2", m_parser.evaluate(u"%trackcount%"_s, tracks));
    EXPECT_EQ(u"00:05", m_parser.evaluate(u"%playtime%"_s, tracks));
    EXPECT_EQ(u"Pop / Rock", m_parser.evaluate(u"%genres%"_s, tracks));
}

TEST_F(ScriptParserTest, EmptyTrackListStillEvaluatesGenericFunctions)
{
    ScriptParser parser;

    TestPlaylistEnvironment environment;
    environment.setEvaluationState(TrackListContextPolicy::Fallback);

    ScriptContext context;
    context.environment = &environment;

    const QString script = u"[%trackcount% $ifequal(%trackcount%,1,Track,Tracks) | %playtime%]$crlf()Test"_s;
    EXPECT_EQ(u"0 Tracks | 00:00\nTest", parser.evaluate(script, TrackList{}, context));
}

TEST_F(ScriptParserTest, MetaTest)
{
    Track track;
    track.setTitle(u"Song"_s);
    track.setArtists({u"The Verve"_s});
    track.setRating(0.7F);
    track.setRawRatingTag(u"RATING"_s, u"7"_s);

    EXPECT_EQ(u"The Verve", m_parser.evaluate(u"%albumartist%"_s, track));
    EXPECT_EQ(u"", m_parser.evaluate(u"$meta(albumartist)"_s, track));
    EXPECT_EQ(u"The Verve", m_parser.evaluate(u"$meta(artist)"_s, track));
    EXPECT_EQ(u"3.5", m_parser.evaluate(u"%rating%"_s, track));
    EXPECT_EQ(u"0.7", m_parser.evaluate(u"%rating_normalized%"_s, track));
    EXPECT_EQ(u"7", m_parser.evaluate(u"$meta(RATING)"_s, track));

    track.setArtists({u"He"_s, u"She"_s, u"Me"_s});
    track.addExtraTag(u"MOOD"_s, QStringList{u"Calm"_s, u"Bright"_s});

    EXPECT_EQ(u"He, She, Me", m_parser.evaluate(u"$meta(artist)"_s, track));
    EXPECT_EQ(u"She", m_parser.evaluate(u"$meta(artist,1)"_s, track));
    EXPECT_EQ(u"", m_parser.evaluate(u"$meta(artist,3)"_s, track));
    EXPECT_EQ(u"He + She + Me", m_parser.evaluate(u"$meta_sep(artist,\" + \")"_s, track));
    EXPECT_EQ(u"He, She, and Me", m_parser.evaluate(u"$meta_sep(artist,\", \",\", and \")"_s, track));
    EXPECT_EQ(u"3", m_parser.evaluate(u"$meta_num(artist)"_s, track));
    EXPECT_EQ(u"1", m_parser.evaluate(u"$meta_test(artist,title)"_s, track));
    EXPECT_EQ(u"", m_parser.evaluate(u"$meta_test(artist,missing)"_s, track));
    EXPECT_EQ(u"Calm, Bright", m_parser.evaluate(u"$meta(mood)"_s, track));
    EXPECT_EQ(u"Bright", m_parser.evaluate(u"$meta(mood,1)"_s, track));
    EXPECT_EQ(u"Calm / Bright", m_parser.evaluate(u"$meta_sep(mood,\" / \")"_s, track));
    EXPECT_EQ(u"2", m_parser.evaluate(u"$meta_num(mood)"_s, track));
    EXPECT_EQ(u"0", m_parser.evaluate(u"$meta_num(missing)"_s, track));
}

TEST_F(ScriptParserTest, InfoTest)
{
    Track track;
    track.setChannels(2);

    EXPECT_EQ(u"Stereo", m_parser.evaluate(u"%channels%"_s, track));
    EXPECT_EQ(u"2", m_parser.evaluate(u"$info(channels)"_s, track));
}

TEST_F(ScriptParserTest, RegistryContextVariables)
{
    ScriptParser parser;
    parser.addProvider(EnvironmentVariableProvider);

    TestPlaylistEnvironment environment;
    environment.setState(4, 12, 2, {});

    ScriptContext context;
    context.environment = &environment;
    const Track track;

    EXPECT_EQ(u"5", parser.evaluate(u"%list_index%"_s, track, context));
    EXPECT_EQ(u"2", parser.evaluate(u"%depth%"_s, track, context));
}

TEST_F(ScriptParserTest, VariableProvidersInstallIntoRegistry)
{
    ScriptParser parser;
    parser.addProvider(ProviderVariableProvider);
    const Track track;

    EXPECT_EQ(QString::fromLatin1(Constants::FrontCover), parser.evaluate(u"%frontcover%"_s, track));
}

TEST_F(ScriptParserTest, StatefulVariableProvidersInstallIntoRegistry)
{
    ScriptParser parser;
    parser.addProvider(ContextVariableProvider);
    TestProviderEnvironment environment;
    environment.setState(u"stateful"_s, {});
    ScriptContext context;
    context.environment = &environment;
    const Track track;

    EXPECT_EQ(u"stateful", parser.evaluate(u"%statefulvar%"_s, track, context));
}

TEST_F(ScriptParserTest, FunctionProvidersInstallIntoRegistry)
{
    ScriptParser parser;
    parser.addProvider(ProviderFunctionProvider);
    const ParsedScript parsed = parser.parse(u"$prefix(test)"_s);

    EXPECT_EQ(u"fn:test", parser.evaluate(parsed));
    EXPECT_EQ(u"fn:test", parser.evaluate(parsed));
}

TEST_F(ScriptParserTest, StatefulFunctionProvidersInstallIntoRegistry)
{
    ScriptParser parser;
    parser.addProvider(ContextFunctionProvider);
    TestProviderEnvironment environment;
    environment.setState({}, u"ctx:"_s);
    ScriptContext context;
    context.environment       = &environment;
    const ParsedScript parsed = parser.parse(u"$stateful_prefix(test)"_s);

    EXPECT_EQ(u"ctx:test", parser.evaluate(parsed, context));
    EXPECT_EQ(u"ctx:test", parser.evaluate(parsed, context));
}

TEST_F(ScriptParserTest, ContextEnvironmentVariables)
{
    ScriptParser parser;
    parser.addProvider(EnvironmentVariableProvider);

    TestPlaylistEnvironment environment;
    environment.setState(4, 12, 2, {0, 2}, 7);

    ScriptContext context;
    context.environment = &environment;
    const Track track;

    EXPECT_EQ(u"5", parser.evaluate(u"%list_index%"_s, track, context));
    EXPECT_EQ(u"2", parser.evaluate(u"%depth%"_s, track, context));
    EXPECT_EQ(u"1", parser.evaluate(u"%queueindex%"_s, track, context));
    EXPECT_EQ(u"1", parser.evaluate(u"%queue_index%"_s, track, context));
    EXPECT_EQ(u"1, 3", parser.evaluate(u"%queueindexes%"_s, track, context));
    EXPECT_EQ(u"1, 3", parser.evaluate(u"%queue_indexes%"_s, track, context));
    EXPECT_EQ(u"7", parser.evaluate(u"%queuetotal%"_s, track, context));
    EXPECT_EQ(u"7", parser.evaluate(u"%queue_total%"_s, track, context));
}

TEST_F(ScriptParserTest, QueueTotalRequiresQueuedTrack)
{
    ScriptParser parser;
    parser.addProvider(EnvironmentVariableProvider);

    TestPlaylistEnvironment environment;
    environment.setState(4, 12, 2, {}, 7);

    ScriptContext context;
    context.environment = &environment;
    const Track track;

    EXPECT_EQ(u"", parser.evaluate(u"%queuetotal%"_s, track, context));
    EXPECT_EQ(u"", parser.evaluate(u"%queue_total%"_s, track, context));
}

TEST_F(ScriptParserTest, ContextEvaluationEnvironmentControlsPolicyAndEscaping)
{
    ScriptParser parser;

    TestPlaylistEnvironment environment;
    environment.setEvaluationState(TrackListContextPolicy::Placeholder, u"|Loading|"_s, true);

    ScriptContext context;
    context.environment = &environment;
    Track track;
    track.setTitle(u"A < B"_s);

    EXPECT_EQ(u"|Loading|", parser.evaluate(u"%trackcount%"_s, track, context));
    EXPECT_EQ(u"A \\< B", parser.evaluate(u"%title%"_s, track, context));
}

TEST_F(ScriptParserTest, ContextEvaluationEnvironmentPreservesPathVariableSeparators)
{
    ScriptParser parser;

    TestPlaylistEnvironment environment;
    environment.setEvaluationState(TrackListContextPolicy::Unresolved, {}, false, false, {}, {}, {}, true);
    environment.setLibraryState({}, u"/tmp/music"_s);

    ScriptContext context;
    context.environment = &environment;
    Track track;
    track.setFilePath(u"/tmp/music/foo/bar.mp3"_s);
    track.setTitle(u"foo/bar"_s);

    EXPECT_EQ(u"/tmp/music/foo", parser.evaluate(u"%path%"_s, track, context));
    EXPECT_EQ(u"/tmp/music/foo/bar.mp3", parser.evaluate(u"%filepath%"_s, track, context));
    EXPECT_EQ(u"foo/bar.mp3", parser.evaluate(u"%relativepath%"_s, track, context));
    EXPECT_EQ(u"foo-bar", parser.evaluate(u"%title%"_s, track, context));
}

TEST_F(ScriptParserTest, ContextTrackListEnvironmentProvidesFallbackData)
{
    ScriptParser parser;

    TrackList tracks;

    Track track1;
    track1.setGenres({u"Pop"_s});
    track1.setDuration(2000);
    tracks.push_back(track1);

    Track track2;
    track2.setGenres({u"Rock"_s});
    track2.setDuration(3000);
    tracks.push_back(track2);

    TestPlaylistEnvironment environment;
    environment.setTrackList(&tracks);
    environment.setEvaluationState(TrackListContextPolicy::Fallback);

    ScriptContext context;
    context.environment = &environment;
    const Track track;

    EXPECT_EQ(u"2", parser.evaluate(u"%trackcount%"_s, track, context));
    EXPECT_EQ(u"00:05", parser.evaluate(u"%playtime%"_s, track, context));
}

TEST_F(ScriptParserTest, ContextPlaybackEnvironmentProvidesPlaybackVariables)
{
    ScriptParser parser;

    TestPlaylistEnvironment environment;
    environment.setPlaybackState(45000, 120000, 320, Player::PlayState::Playing);

    ScriptContext context;
    context.environment = &environment;
    Track track;
    track.setBitrate(192);

    EXPECT_EQ(u"00:45", parser.evaluate(u"%playback_time%"_s, track, context));
    EXPECT_EQ(u"75", parser.evaluate(u"%playback_time_remaining_s%"_s, track, context));
    EXPECT_EQ(u"1", parser.evaluate(u"%isplaying%"_s, track, context));
    EXPECT_EQ(u"320", parser.evaluate(u"%bitrate%"_s, track, context));
}

TEST_F(ScriptParserTest, ContextLibraryEnvironmentProvidesLibraryVariables)
{
    ScriptParser parser;

    TestPlaylistEnvironment environment;
    environment.setLibraryState(u"Main"_s, u"/music"_s);

    ScriptContext context;
    context.environment = &environment;
    Track track;
    track.setFilePath(u"/music/Artist/Album/Track.flac"_s);

    EXPECT_EQ(u"Main", parser.evaluate(u"%libraryname%"_s, track, context));
    EXPECT_EQ(u"/music", parser.evaluate(u"%librarypath%"_s, track, context));
    EXPECT_EQ(u"Artist/Album/Track.flac", parser.evaluate(u"%relativepath%"_s, track, context));
}

TEST_F(ScriptParserTest, TrackEvaluationDoesNotUseTrackListContextByDefault)
{
    ScriptParser parser;

    TrackList tracks;

    Track track1;
    track1.setDuration(2000);
    tracks.push_back(track1);

    Track track2;
    track2.setDuration(3000);
    tracks.push_back(track2);

    TestPlaylistEnvironment environment;
    environment.setTrackList(&tracks);

    ScriptContext context;
    context.environment = &environment;

    EXPECT_EQ(u"%TRACKCOUNT%", parser.evaluate(u"%trackcount%"_s, track1, context));
    EXPECT_EQ(u"%PLAYLIST_DURATION%", parser.evaluate(u"%playlist_duration%"_s, track1, context));
}

TEST_F(ScriptParserTest, TrackEvaluationCanUseTrackListContextWhenEnabled)
{
    ScriptParser parser;

    TrackList tracks;

    Track track1;
    track1.setDuration(2000);
    tracks.push_back(track1);

    Track track2;
    track2.setDuration(3000);
    tracks.push_back(track2);

    TestPlaylistEnvironment environment;
    environment.setTrackList(&tracks);
    environment.setEvaluationState(TrackListContextPolicy::Fallback);

    ScriptContext context;
    context.environment = &environment;

    EXPECT_EQ(u"2", parser.evaluate(u"%trackcount%"_s, track1, context));
    EXPECT_EQ(u"00:05", parser.evaluate(u"%playlist_duration%"_s, track1, context));
}

TEST_F(ScriptParserTest, TrackEvaluationCanUseTrackListPlaceholderPolicy)
{
    ScriptParser parser;

    TrackList tracks;

    Track track1;
    track1.setDuration(2000);
    tracks.push_back(track1);

    TestPlaylistEnvironment environment;
    environment.setTrackList(&tracks);
    environment.setEvaluationState(TrackListContextPolicy::Placeholder, u"|Loading|"_s);

    ScriptContext context;
    context.environment = &environment;

    EXPECT_EQ(u"|Loading|", parser.evaluate(u"%trackcount%"_s, track1, context));
    EXPECT_EQ(u"|Loading|", parser.evaluate(u"%playlist_duration%"_s, track1, context));
}

TEST_F(ScriptParserTest, QueryTest)
{
    TrackList tracks;

    Track track1;
    track1.setId(0);
    track1.setTitle(u"Wandering Horizon"_s);
    track1.setAlbum(u"Electric Dreams"_s);
    track1.setAlbumArtists({u"Solar Artist 1"_s, u"Stellar Artist 2"_s});
    track1.setArtists({u"Lunar Sound 1"_s, u"Galactic Echo 2"_s});
    track1.setDate(u"2021-05-17"_s);
    track1.setTrackNumber(u"7"_s);
    track1.setDiscNumber(u"2"_s);
    track1.setBitDepth(24);
    track1.setSampleRate(44100);
    track1.setBitrate(1000);
    track1.setComment(u"Awesome track with deep beats"_s);
    track1.setComposers({u"Sound Designer 1"_s, u"Master Composer 2"_s});
    track1.setPerformers({u"Vocalist 1"_s, u"Instrumentalist 2"_s});
    track1.setDuration(210000);
    track1.setFileSize(45600000);
    track1.setDate(u"1991-03-29"_s);
    track1.setFirstPlayed(QDateTime::currentDateTime().addMonths(-2).toMSecsSinceEpoch());
    track1.setLastPlayed(QDateTime::currentDateTime().addDays(-6).toMSecsSinceEpoch());
    track1.setPlayCount(1);
    track1.setRating(0.6F);
    tracks.push_back(track1);

    Track track2;
    track2.setId(1);
    track2.setTitle(u"Celestial Waves"_s);
    track2.setAlbum(u"Chasing Light"_s);
    track2.setAlbumArtists({u"Horizon Band"_s, u"Sunset Group"_s});
    track2.setArtists({u"Ocean Vibes"_s, u"Sky Whisper"_s});
    track2.setDate(u"2023-11-12"_s);
    track2.setTrackNumber(u"4"_s);
    track2.setDiscNumber(u"1"_s);
    track2.setBitDepth(24);
    track2.setSampleRate(48000);
    track2.setBitrate(950);
    track2.setComment(u"A serene journey through sound"_s);
    track2.setComposers({u"Melody Creator 1"_s, u"Harmony Producer 2"_s});
    track2.setDuration(195000);
    track2.setFileSize(32000000);
    track2.setDate(u"2010-09-15"_s);
    track2.setFirstPlayed(QDateTime(QDate(2021, 1, 1), QTime(0, 0, 0)).toMSecsSinceEpoch());
    track2.setLastPlayed(QDateTime::currentDateTime().addDays(-3).toMSecsSinceEpoch());
    track2.setPlayCount(8);
    track2.setRating(0.4F);
    tracks.push_back(track2);

    // Basic operator tests
    EXPECT_EQ(1, m_parser.filter(u"$info(duration)=210000"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"playcount>1"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"playcount GREATER 1"_s, tracks).size());
    EXPECT_EQ(0, m_parser.filter(u"playcount LESS 1"_s, tracks).size());
    EXPECT_EQ(2, m_parser.filter(u"playcount>=1"_s, tracks).size());
    EXPECT_EQ(0, m_parser.filter(u"playcount>=A"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"stars>=3"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"rating>=3"_s, tracks).size());
    EXPECT_EQ(2, m_parser.filter(u"rating>1"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"rating_normalized>=0.6"_s, tracks).size());
    EXPECT_EQ(0, m_parser.filter(u"$unknown(duration)=210000"_s, tracks).size());

    // Logical operator tests
    EXPECT_EQ(1, m_parser.filter(u"title=Wandering Horizon AND genre MISSING"_s, tracks).size());
    EXPECT_EQ(2, m_parser.filter(u"playcount=1 OR playcount=8"_s, tracks).size());
    EXPECT_EQ(2, m_parser.filter(u"playcount=1 XOR playcount=8"_s, tracks).size());
    EXPECT_EQ(0, m_parser.filter(u"playcount=1 XOR playcount=1"_s, tracks).size());

    // Negation test
    EXPECT_EQ(1, m_parser.filter(u"!playcount=1"_s, tracks).size());
    EXPECT_EQ(0, m_parser.filter(u"NOT playcount>=1"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"NOT (playcount=1 OR bitrate=1000)"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"NOT (playcount=8 AND bitrate=950)"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"playcount NOT = 1"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"playcount NOT GREATER 1"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"playcount NOT LESS 8"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"playcount NOT >= 8"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"bitrate NOT < 1000"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"title NOT : Celestial"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"title NOT = Wandering Horizon"_s, tracks).size());

    // PRESENT/MISSING keyword tests
    EXPECT_EQ(1, m_parser.filter(u"performer PRESENT"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"performer MISSING"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"performer NOT PRESENT"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"performer NOT MISSING"_s, tracks).size());
    EXPECT_EQ(0, m_parser.filter(u"rating_stars PRESENT"_s, tracks).size());
    EXPECT_EQ(2, m_parser.filter(u"rating_stars MISSING"_s, tracks).size());

    // String matching tests
    EXPECT_EQ(1, m_parser.filter(u"title:wandering hor"_s, tracks).size());
    EXPECT_EQ(0, m_parser.filter(u"title=wandering hor"_s, tracks).size());
    EXPECT_EQ(2, m_parser.filter(u"title:Wa"_s, tracks).size());
    track1.setArtists({u"Ke$ha"_s});
    track1.setComment(u"Costs $5 [sale] 100% %artist%"_s);
    tracks[0] = track1;
    EXPECT_EQ(1, m_parser.filter(u"Ke$ha"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"$5"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"$"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"[sale]"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"100%"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"%artist%"_s, tracks).size());

    // Date comparisons
    EXPECT_EQ(1, m_parser.filter(u"date BEFORE 2000"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"date AFTER 2000"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"firstplayed SINCE 2022"_s, tracks).size());
    EXPECT_EQ(2, m_parser.filter(u"lastplayed DURING LAST WEEK"_s, tracks).size());
    EXPECT_EQ(0, m_parser.filter(u"lastplayed DURING 2"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"date NOT BEFORE 2000"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"date NOT AFTER 2000"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"firstplayed NOT SINCE 2022"_s, tracks).size());
    EXPECT_EQ(0, m_parser.filter(u"lastplayed NOT DURING LAST WEEK"_s, tracks).size());
    EXPECT_EQ(2, m_parser.filter(u"%lastplayed% NOT DURING LAST 1 DAYS"_s, tracks).size());

    // Grouping and complex queries
    EXPECT_EQ(2, m_parser.filter(u"(playcount>=1 AND bitrate>500) OR title:Celestial"_s, tracks).size());
    EXPECT_EQ(2, m_parser.filter(u"(playcount>=1 AND (bitrate>500 OR bitrate=950))"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"(playcount=8 OR (bitrate>950 AND duration>200000))"_s, tracks).size());
    EXPECT_EQ(0, m_parser.filter(u"(playcount=8 AND (bitrate<900 OR duration<180000))"_s, tracks).size());

    QString query = u"(title:Wand AND album:Elec) OR (title:Celest AND playcount=8)"_s;
    EXPECT_EQ(2, m_parser.filter(query, tracks).size());
    query = u"(title:Wandering OR (playcount=1 AND bitrate>900))"_s;
    EXPECT_EQ(1, m_parser.filter(query, tracks).size());
    query = u"((playcount>=1 AND bitrate>500) OR title:Celest) AND (duration_ms>180000)"_s;
    EXPECT_EQ(2, m_parser.filter(query, tracks).size());
}

TEST_F(ScriptParserTest, QueryAccentInsensitiveSearch)
{
    Track track;
    track.setId(0);
    track.setTitle(u"Café Sketches"_s);
    track.setAlbum(u"Más Allá"_s);
    track.setAlbumArtists({u"Gábor Szabó"_s});
    track.setArtists({u"Gábor Szabó"_s});

    const TrackList tracks{track};

    EXPECT_EQ(1, m_parser.filter(u"gabor"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"\"gabor szabo\""_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"artist:gabor"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"album:mas"_s, tracks).size());
    EXPECT_EQ(1, m_parser.filter(u"title:cafe"_s, tracks).size());
}

TEST_F(ScriptParserTest, QueryAndLiteralCachesStaySeparate)
{
    TrackList tracks;

    Track track;
    track.setPlayCount(2);
    tracks.push_back(track);

    EXPECT_EQ(1, m_parser.filter(u"playcount>1"_s, tracks).size());
    EXPECT_EQ(u"playcount>1", m_parser.evaluate(u"playcount>1"_s, track));
}
} // namespace Fooyin::Testing
