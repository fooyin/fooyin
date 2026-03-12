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
#include <core/scripting/scriptparser.h>
#include <core/scripting/scriptproviders.h>
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
    void setState(int playlistTrackIndex, int playlistTrackCount, int trackDepth, std::vector<int> queueIndexes)
    {
        m_playlistTrackIndex = playlistTrackIndex;
        m_playlistTrackCount = playlistTrackCount;
        m_trackDepth         = trackDepth;
        m_queueIndexes       = std::move(queueIndexes);
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
                            bool useVariousArtists = false)
    {
        m_trackListContextPolicy = policy;
        m_trackListPlaceholder   = std::move(placeholder);
        m_escapeRichText         = escapeRichText;
        m_useVariousArtists      = useVariousArtists;
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

private:
    int m_playlistTrackIndex{-1};
    int m_playlistTrackCount{0};
    int m_trackDepth{0};
    std::vector<int> m_queueIndexes;
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
};

static const StaticScriptVariableProvider EnvironmentVariableProvider{
    makeScriptVariableDescriptor<ListIndexVariable>(VariableKind::ListIndex, u"LIST_INDEX"_s),
    makeScriptVariableDescriptor<QueueIndexesVariable>(VariableKind::QueueIndexes, u"QUEUEINDEXES"_s),
    makeScriptVariableDescriptor<DepthVariable>(VariableKind::Depth, u"DEPTH"_s)};

TEST_F(ScriptParserTest, BasicLiteral)
{
    EXPECT_EQ(u"I am a test.", m_parser.evaluate(QStringLiteral("I am a test.")));
    EXPECT_EQ(u"", m_parser.evaluate(QString{}));
}

TEST_F(ScriptParserTest, EscapeComment)
{
    EXPECT_EQ(u"I am a % test.", m_parser.evaluate(QStringLiteral(R"("I am a \% test.")")));
    EXPECT_EQ(u"I am an \"escape test.", m_parser.evaluate(QStringLiteral(R"("I am an \"escape test.")")));
}

TEST_F(ScriptParserTest, Quote)
{
    EXPECT_EQ(u"I %am% a $test$.", m_parser.evaluate(QStringLiteral(R"("I %am% a $test$.")")));
}

TEST_F(ScriptParserTest, StringTest)
{
    EXPECT_EQ(u"01", m_parser.evaluate(QStringLiteral("$num(1,2)")));
    EXPECT_EQ(u"04", m_parser.evaluate(QStringLiteral("$num(04,2)")));
    EXPECT_EQ(u"", m_parser.evaluate(QStringLiteral("$num()")));
    EXPECT_EQ(u"01", m_parser.evaluate(QStringLiteral("[$num(1,2)]")));
    EXPECT_EQ(u"A replace cesc", m_parser.evaluate(QStringLiteral("$replace(A replace test,t,c)")));
    EXPECT_EQ(u"", m_parser.evaluate(QStringLiteral("$replace()")));
    EXPECT_EQ(u"test", m_parser.evaluate(QStringLiteral("$slice(A slice test,8)")));
    EXPECT_EQ(u"slice", m_parser.evaluate(QStringLiteral("$slice(A slice test,2,5)")));
    EXPECT_EQ(u"", m_parser.evaluate(QStringLiteral("$slice()")));
    EXPECT_EQ(u"", m_parser.evaluate(QStringLiteral("$slice(1,2,3,4,5)")));
    EXPECT_EQ(u"A chop", m_parser.evaluate(QStringLiteral("$chop(A chop test,5)")));
    EXPECT_EQ(u"", m_parser.evaluate(QStringLiteral("$chop()")));
    EXPECT_EQ(u"", m_parser.evaluate(QStringLiteral("$chop(1,2,3,4,5)")));
    EXPECT_EQ(u"L", m_parser.evaluate(QStringLiteral("$left(Left test,1)")));
    EXPECT_EQ(u"", m_parser.evaluate(QStringLiteral("$left(1,2,3,4,5)")));
    EXPECT_EQ(u"", m_parser.evaluate(QStringLiteral("$left()")));
    EXPECT_EQ(u"est", m_parser.evaluate(QStringLiteral("$right(Right test,3)")));
    EXPECT_EQ(u"", m_parser.evaluate(QStringLiteral("$right()")));
    EXPECT_EQ(u"", m_parser.evaluate(QStringLiteral("$right(1,2,3,4,5)")));
    EXPECT_EQ(u"a  ", m_parser.evaluate(QStringLiteral("[$pad(a,3)]")));
    EXPECT_EQ(u"  a", m_parser.evaluate(QStringLiteral("[$padright(a,3)]")));
    EXPECT_EQ(u"x", m_parser.evaluate(QStringLiteral("[$if2(,x)]")));
    EXPECT_EQ(u"fallback", m_parser.evaluate(QStringLiteral("$if3(,,fallback)")));
    EXPECT_EQ(u"second", m_parser.evaluate(QStringLiteral("$if3(,second,third,fallback)")));
    EXPECT_EQ(u"winnerwinner",
              m_parser.evaluate(QStringLiteral("$if3(,$put(choice,winner),$put(choice,wrong),fallback)$get(choice)")));
    EXPECT_EQ(u"          X", m_parser.evaluate(QStringLiteral("$padright(,$mul($sub(3,1),5))X")));
    EXPECT_EQ(u"true", m_parser.evaluate(QStringLiteral("$if($stricmp(cmp,cMp),true,false)")));
    EXPECT_EQ(u"false", m_parser.evaluate(QStringLiteral("$if($strcmp(cmp,cMp),true,false)")));
}

TEST_F(ScriptParserTest, MathTest)
{
    EXPECT_EQ(3, m_parser.evaluate(QStringLiteral("$add(1,2)")).toInt());
    EXPECT_EQ(2, m_parser.evaluate(QStringLiteral("$sub(10,8)")).toInt());
    EXPECT_EQ(99, m_parser.evaluate(QStringLiteral("$mul(3,33)")).toInt());
    EXPECT_EQ(11, m_parser.evaluate(QStringLiteral("$div(33,3)")).toInt());
    EXPECT_EQ(1, m_parser.evaluate(QStringLiteral("$mod(10,3)")).toInt());
    EXPECT_EQ(u"3", m_parser.evaluate(QStringLiteral("[$add(1,2)]")));
    EXPECT_EQ(u"2", m_parser.evaluate(QStringLiteral("[$sub(10,8)]")));
    EXPECT_EQ(u"99", m_parser.evaluate(QStringLiteral("[$mul(3,33)]")));
    EXPECT_EQ(u"11", m_parser.evaluate(QStringLiteral("[$div(33,3)]")));
    EXPECT_EQ(u"1", m_parser.evaluate(QStringLiteral("[$mod(10,3)]")));
    EXPECT_EQ(2, m_parser.evaluate(QStringLiteral("$min(3,2,3,9,23,100,4)")).toInt());
    EXPECT_EQ(100, m_parser.evaluate(QStringLiteral("$max(3,2,3,9,23,100,4)")).toInt());
}

TEST_F(ScriptParserTest, ConditionalTest)
{
    EXPECT_EQ(u"true", m_parser.evaluate(QStringLiteral("$ifequal(1,1,true,false)")));
    EXPECT_EQ(u"false", m_parser.evaluate(QStringLiteral("$ifgreater(23,32,true,false)")));
    EXPECT_EQ(u"true", m_parser.evaluate(QStringLiteral("$iflonger(aaa,2,true,false)")));
    EXPECT_EQ(u"true", m_parser.evaluate(QStringLiteral("[$if(1,true,false)]")));
    EXPECT_EQ(u"first", m_parser.evaluate(QStringLiteral("[$if2(first,second)]")));
    EXPECT_EQ(u"true", m_parser.evaluate(QStringLiteral("[$ifequal(1,1,true,false)]")));
    EXPECT_EQ(u"true", m_parser.evaluate(QStringLiteral("[$ifgreater(5,3,true,false)]")));
    EXPECT_EQ(u"true", m_parser.evaluate(QStringLiteral("[$iflonger(aaa,2,true,false)]")));
}

TEST_F(ScriptParserTest, MetadataTest)
{
    Track track;
    track.setTitle(QStringLiteral("A Test"));

    EXPECT_EQ(u"A Test", m_parser.evaluate(QStringLiteral("%title%"), track));
    EXPECT_EQ(u"A Test", m_parser.evaluate(QStringLiteral("[%title%]"), track));
    EXPECT_EQ(u"A Test", m_parser.evaluate(QStringLiteral("%title%[ - %album%]"), track));

    track.setAlbum(QStringLiteral("A Test Album"));

    EXPECT_EQ(u"A Test Album", m_parser.evaluate(QStringLiteral("[%album%]"), track));
    EXPECT_EQ(u"A Test - A Test Album", m_parser.evaluate(QStringLiteral("%title%[ - %album%]"), track));

    track.setGenres({QStringLiteral("Pop"), QStringLiteral("Rock")});

    EXPECT_EQ(u"Pop, Rock", m_parser.evaluate(QStringLiteral("%genre%"), track));
    EXPECT_EQ(u"Pop\037Rock", m_parser.evaluate(QStringLiteral("%<genre>%"), track));

    track.setArtists({QStringLiteral("Me"), QStringLiteral("You")});

    EXPECT_EQ(u"Pop, Rock - Me, You", m_parser.evaluate(QStringLiteral("%genre% - %artist%"), track));
    EXPECT_EQ(u"Pop - Me\037Rock - Me\037Pop - You\037Rock - You",
              m_parser.evaluate(QStringLiteral("%<genre>% - %<artist>%"), track));

    track.setTrackNumber(u"7"_s);
    EXPECT_EQ(u"7", m_parser.evaluate(QStringLiteral("[%track%]"), track));
    EXPECT_EQ(u"07", m_parser.evaluate(QStringLiteral("$num(%track%,2)"), track));
    EXPECT_EQ(u"07", m_parser.evaluate(QStringLiteral("[$num(%track%,2)]"), track));
    EXPECT_EQ(u"07.  ", m_parser.evaluate(QStringLiteral("[$num(%track%,2).  ]"), track));

    EXPECT_EQ(u"", m_parser.evaluate(QStringLiteral("[%disc% - %track%]"), track));
}

TEST_F(ScriptParserTest, TrackListTest)
{
    TrackList tracks;

    Track track1;
    track1.setGenres({QStringLiteral("Pop")});
    track1.setDuration(2000);
    tracks.push_back(track1);

    Track track2;
    track2.setGenres({QStringLiteral("Rock")});
    track2.setDuration(3000);
    tracks.push_back(track2);

    EXPECT_EQ(u"2", m_parser.evaluate(QStringLiteral("%trackcount%"), tracks));
    EXPECT_EQ(u"00:05", m_parser.evaluate(QStringLiteral("%playtime%"), tracks));
    EXPECT_EQ(u"Pop / Rock", m_parser.evaluate(QStringLiteral("%genres%"), tracks));
}

TEST_F(ScriptParserTest, MetaTest)
{
    Track track;
    track.setArtists({QStringLiteral("The Verve")});

    EXPECT_EQ(u"The Verve", m_parser.evaluate(QStringLiteral("%albumartist%"), track));
    EXPECT_EQ(u"", m_parser.evaluate(QStringLiteral("$meta(albumartist)"), track));
    EXPECT_EQ(u"The Verve", m_parser.evaluate(QStringLiteral("$meta(artist)"), track));
}

TEST_F(ScriptParserTest, InfoTest)
{
    Track track;
    track.setChannels(2);

    EXPECT_EQ(u"Stereo", m_parser.evaluate(QStringLiteral("%channels%"), track));
    EXPECT_EQ(u"2", m_parser.evaluate(QStringLiteral("$info(channels)"), track));
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

    EXPECT_EQ(u"5", parser.evaluate(QStringLiteral("%list_index%"), track, context));
    EXPECT_EQ(u"2", parser.evaluate(QStringLiteral("%depth%"), track, context));
}

TEST_F(ScriptParserTest, VariableProvidersInstallIntoRegistry)
{
    ScriptParser parser;
    parser.addProvider(ProviderVariableProvider);
    const Track track;

    EXPECT_EQ(QString::fromLatin1(Constants::FrontCover), parser.evaluate(QStringLiteral("%frontcover%"), track));
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

    EXPECT_EQ(u"stateful", parser.evaluate(QStringLiteral("%statefulvar%"), track, context));
}

TEST_F(ScriptParserTest, FunctionProvidersInstallIntoRegistry)
{
    ScriptParser parser;
    parser.addProvider(ProviderFunctionProvider);
    const ParsedScript parsed = parser.parse(QStringLiteral("$prefix(test)"));

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
    const ParsedScript parsed = parser.parse(QStringLiteral("$stateful_prefix(test)"));

    EXPECT_EQ(u"ctx:test", parser.evaluate(parsed, context));
    EXPECT_EQ(u"ctx:test", parser.evaluate(parsed, context));
}

TEST_F(ScriptParserTest, ContextEnvironmentVariables)
{
    ScriptParser parser;
    parser.addProvider(EnvironmentVariableProvider);

    TestPlaylistEnvironment environment;
    environment.setState(4, 12, 2, {0, 2});

    ScriptContext context;
    context.environment = &environment;
    const Track track;

    EXPECT_EQ(u"5", parser.evaluate(QStringLiteral("%list_index%"), track, context));
    EXPECT_EQ(u"2", parser.evaluate(QStringLiteral("%depth%"), track, context));
    EXPECT_EQ(u"1, 3", parser.evaluate(QStringLiteral("%queueindexes%"), track, context));
}

TEST_F(ScriptParserTest, ContextEvaluationEnvironmentControlsPolicyAndEscaping)
{
    ScriptParser parser;

    TestPlaylistEnvironment environment;
    environment.setEvaluationState(TrackListContextPolicy::Placeholder, u"|Loading|"_s, true);

    ScriptContext context;
    context.environment = &environment;
    Track track;
    track.setTitle(QStringLiteral("A < B"));

    EXPECT_EQ(u"|Loading|", parser.evaluate(QStringLiteral("%trackcount%"), track, context));
    EXPECT_EQ(u"A \\< B", parser.evaluate(QStringLiteral("%title%"), track, context));
}

TEST_F(ScriptParserTest, ContextTrackListEnvironmentProvidesFallbackData)
{
    ScriptParser parser;

    TrackList tracks;

    Track track1;
    track1.setGenres({QStringLiteral("Pop")});
    track1.setDuration(2000);
    tracks.push_back(track1);

    Track track2;
    track2.setGenres({QStringLiteral("Rock")});
    track2.setDuration(3000);
    tracks.push_back(track2);

    TestPlaylistEnvironment environment;
    environment.setTrackList(&tracks);
    environment.setEvaluationState(TrackListContextPolicy::Fallback);

    ScriptContext context;
    context.environment = &environment;
    const Track track;

    EXPECT_EQ(u"2", parser.evaluate(QStringLiteral("%trackcount%"), track, context));
    EXPECT_EQ(u"00:05", parser.evaluate(QStringLiteral("%playtime%"), track, context));
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

    EXPECT_EQ(u"00:45", parser.evaluate(QStringLiteral("%playback_time%"), track, context));
    EXPECT_EQ(u"75", parser.evaluate(QStringLiteral("%playback_time_remaining_s%"), track, context));
    EXPECT_EQ(u"1", parser.evaluate(QStringLiteral("%isplaying%"), track, context));
    EXPECT_EQ(u"320", parser.evaluate(QStringLiteral("%bitrate%"), track, context));
}

TEST_F(ScriptParserTest, ContextLibraryEnvironmentProvidesLibraryVariables)
{
    ScriptParser parser;

    TestPlaylistEnvironment environment;
    environment.setLibraryState(QStringLiteral("Main"), QStringLiteral("/music"));

    ScriptContext context;
    context.environment = &environment;
    Track track;
    track.setFilePath(QStringLiteral("/music/Artist/Album/Track.flac"));

    EXPECT_EQ(u"Main", parser.evaluate(QStringLiteral("%libraryname%"), track, context));
    EXPECT_EQ(u"/music", parser.evaluate(QStringLiteral("%librarypath%"), track, context));
    EXPECT_EQ(u"Artist/Album/Track.flac", parser.evaluate(QStringLiteral("%relativepath%"), track, context));
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

    EXPECT_EQ(u"%TRACKCOUNT%", parser.evaluate(QStringLiteral("%trackcount%"), track1, context));
    EXPECT_EQ(u"%PLAYLIST_DURATION%", parser.evaluate(QStringLiteral("%playlist_duration%"), track1, context));
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

    EXPECT_EQ(u"2", parser.evaluate(QStringLiteral("%trackcount%"), track1, context));
    EXPECT_EQ(u"00:05", parser.evaluate(QStringLiteral("%playlist_duration%"), track1, context));
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

    EXPECT_EQ(u"|Loading|", parser.evaluate(QStringLiteral("%trackcount%"), track1, context));
    EXPECT_EQ(u"|Loading|", parser.evaluate(QStringLiteral("%playlist_duration%"), track1, context));
}

TEST_F(ScriptParserTest, QueryTest)
{
    TrackList tracks;

    Track track1;
    track1.setId(0);
    track1.setTitle(QStringLiteral("Wandering Horizon"));
    track1.setAlbum(QStringLiteral("Electric Dreams"));
    track1.setAlbumArtists({QStringLiteral("Solar Artist 1"), QStringLiteral("Stellar Artist 2")});
    track1.setArtists({QStringLiteral("Lunar Sound 1"), QStringLiteral("Galactic Echo 2")});
    track1.setDate(QStringLiteral("2021-05-17"));
    track1.setTrackNumber(QStringLiteral("7"));
    track1.setDiscNumber(QStringLiteral("2"));
    track1.setBitDepth(24);
    track1.setSampleRate(44100);
    track1.setBitrate(1000);
    track1.setComment(QStringLiteral("Awesome track with deep beats"));
    track1.setComposers({QStringLiteral("Sound Designer 1"), QStringLiteral("Master Composer 2")});
    track1.setPerformers({QStringLiteral("Vocalist 1"), QStringLiteral("Instrumentalist 2")});
    track1.setDuration(210000);
    track1.setFileSize(45600000);
    track1.setDate(QStringLiteral("1991-03-29"));
    track1.setFirstPlayed(QDateTime::currentDateTime().addMonths(-2).toMSecsSinceEpoch());
    track1.setLastPlayed(QDateTime::currentDateTime().addDays(-6).toMSecsSinceEpoch());
    track1.setPlayCount(1);
    tracks.push_back(track1);

    Track track2;
    track2.setId(1);
    track2.setTitle(QStringLiteral("Celestial Waves"));
    track2.setAlbum(QStringLiteral("Chasing Light"));
    track2.setAlbumArtists({QStringLiteral("Horizon Band"), QStringLiteral("Sunset Group")});
    track2.setArtists({QStringLiteral("Ocean Vibes"), QStringLiteral("Sky Whisper")});
    track2.setDate(QStringLiteral("2023-11-12"));
    track2.setTrackNumber(QStringLiteral("4"));
    track2.setDiscNumber(QStringLiteral("1"));
    track2.setBitDepth(24);
    track2.setSampleRate(48000);
    track2.setBitrate(950);
    track2.setComment(QStringLiteral("A serene journey through sound"));
    track2.setComposers({QStringLiteral("Melody Creator 1"), QStringLiteral("Harmony Producer 2")});
    track2.setDuration(195000);
    track2.setFileSize(32000000);
    track2.setDate(QStringLiteral("2010-09-15"));
    track2.setFirstPlayed(QDateTime(QDate(2021, 1, 1), QTime(0, 0, 0)).toMSecsSinceEpoch());
    track2.setLastPlayed(QDateTime::currentDateTime().addDays(-3).toMSecsSinceEpoch());
    track2.setPlayCount(8);
    tracks.push_back(track2);

    // Basic operator tests
    EXPECT_EQ(1, m_parser.filter(QStringLiteral("$info(duration)=210000"), tracks).size());
    EXPECT_EQ(1, m_parser.filter(QStringLiteral("playcount>1"), tracks).size());
    EXPECT_EQ(1, m_parser.filter(QStringLiteral("playcount GREATER 1"), tracks).size());
    EXPECT_EQ(0, m_parser.filter(QStringLiteral("playcount LESS 1"), tracks).size());
    EXPECT_EQ(2, m_parser.filter(QStringLiteral("playcount>=1"), tracks).size());
    EXPECT_EQ(0, m_parser.filter(QStringLiteral("playcount>=A"), tracks).size());

    // Logical operator tests
    EXPECT_EQ(1, m_parser.filter(QStringLiteral("title=Wandering Horizon AND genre MISSING"), tracks).size());
    EXPECT_EQ(2, m_parser.filter(QStringLiteral("playcount=1 OR playcount=8"), tracks).size());
    EXPECT_EQ(2, m_parser.filter(QStringLiteral("playcount=1 XOR playcount=8"), tracks).size());
    EXPECT_EQ(0, m_parser.filter(QStringLiteral("playcount=1 XOR playcount=1"), tracks).size());

    // Negation test
    EXPECT_EQ(1, m_parser.filter(QStringLiteral("!playcount=1"), tracks).size());
    EXPECT_EQ(0, m_parser.filter(QStringLiteral("NOT playcount>=1"), tracks).size());

    // PRESENT/MISSING keyword tests
    EXPECT_EQ(1, m_parser.filter(QStringLiteral("performer PRESENT"), tracks).size());
    EXPECT_EQ(1, m_parser.filter(QStringLiteral("performer MISSING"), tracks).size());

    // String matching tests
    EXPECT_EQ(1, m_parser.filter(QStringLiteral("title:wandering hor"), tracks).size());
    EXPECT_EQ(0, m_parser.filter(QStringLiteral("title=wandering hor"), tracks).size());
    EXPECT_EQ(2, m_parser.filter(QStringLiteral("title:Wa"), tracks).size());

    // Date comparisons
    EXPECT_EQ(1, m_parser.filter(QStringLiteral("date BEFORE 2000"), tracks).size());
    EXPECT_EQ(1, m_parser.filter(QStringLiteral("date AFTER 2000"), tracks).size());
    EXPECT_EQ(1, m_parser.filter(QStringLiteral("firstplayed SINCE 2022"), tracks).size());
    EXPECT_EQ(2, m_parser.filter(QStringLiteral("lastplayed DURING LAST WEEK"), tracks).size());
    EXPECT_EQ(0, m_parser.filter(QStringLiteral("lastplayed DURING 2"), tracks).size());

    // Grouping and complex queries
    EXPECT_EQ(2, m_parser.filter(QStringLiteral("(playcount>=1 AND bitrate>500) OR title:Celestial"), tracks).size());
    EXPECT_EQ(2, m_parser.filter(QStringLiteral("(playcount>=1 AND (bitrate>500 OR bitrate=950))"), tracks).size());
    EXPECT_EQ(1, m_parser.filter(QStringLiteral("(playcount=8 OR (bitrate>950 AND duration>200000))"), tracks).size());
    EXPECT_EQ(0, m_parser.filter(QStringLiteral("(playcount=8 AND (bitrate<900 OR duration<180000))"), tracks).size());

    QString query = QStringLiteral("(title:Wand AND album:Elec) OR (title:Celest AND playcount=8)");
    EXPECT_EQ(2, m_parser.filter(query, tracks).size());
    query = QStringLiteral("(title:Wandering OR (playcount=1 AND bitrate>900))");
    EXPECT_EQ(1, m_parser.filter(query, tracks).size());
    query = QStringLiteral("((playcount>=1 AND bitrate>500) OR title:Celest) AND (duration_ms>180000)");
    EXPECT_EQ(2, m_parser.filter(query, tracks).size());
}

TEST_F(ScriptParserTest, QueryAndLiteralCachesStaySeparate)
{
    TrackList tracks;

    Track track;
    track.setPlayCount(2);
    tracks.push_back(track);

    EXPECT_EQ(1, m_parser.filter(QStringLiteral("playcount>1"), tracks).size());
    EXPECT_EQ(u"playcount>1", m_parser.evaluate(QStringLiteral("playcount>1"), track));
}
} // namespace Fooyin::Testing
