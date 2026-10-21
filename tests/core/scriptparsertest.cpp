/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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
#include <core/scripting/scriptregistry.h>
#include <core/scripting/scriptscanner.h>
#include <core/scripting/scripttrackwriter.h>
#include <core/scripting/trackqueryfilter.h>
#include <core/track.h>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

#include <array>
#include <future>

#include <gtest/gtest.h>

using namespace Qt::StringLiterals;

namespace Fooyin::Testing {
struct TestProviderContext
{
    std::array<ScriptVariableValue, 2> values;
    ScriptContext context;
    void setState(QString variableValue, QString functionPrefix)
    {
        values         = {{{u"VALUE"_s, {.value = variableValue, .cond = !variableValue.isEmpty()}},
                           {u"PREFIX"_s, {.value = functionPrefix, .cond = !functionPrefix.isEmpty()}}}};
        context.values = values;
    }
};

static const std::array ContextVariables{
    makeScriptVariableDescriptor<[](const ScriptContext& context, const QString&) -> ScriptResult {
        return context.values.empty() ? ScriptResult{} : context.values.front().result;
    }>(VariableKind::FrontCover, u"STATEFULVAR"_s)};

static constexpr auto PrefixFunction = [](const ScriptFunctionCallContext& call) -> ScriptResult {
    if(call.args.empty()) {
        return {};
    }

    const QString value = u"fn:%1"_s.arg(call.args.front().value);
    return {.value = value, .cond = true};
};

static const std::array CustomFunctions{makeScriptFunctionDescriptor<PrefixFunction>(u"prefix"_s)};

static const std::array ContextFunctions{
    makeScriptFunctionDescriptor<[](const ScriptFunctionCallContext& call) -> ScriptResult {
        if(call.args.empty()) {
            return {};
        }

        if(!call.context || call.context->values.size() < 2) {
            return {};
        }
        const QString value = call.context->values[1].result.value + call.args.front().value;
        return {.value = value, .cond = true};
    }>(u"stateful_prefix"_s)};

struct TestScriptContext
{
    ScriptPlaylistData playlistData;
    ScriptPlaybackData playback;
    ScriptContext context{.playlistData = &playlistData, .playback = &playback};
    ScriptEvaluationOptions options;
    LibraryInfoMap libraries;

    void setState(int index, int count, int depth, std::vector<int> queueIndexes, int queueTotal = 0)
    {
        auto& row        = playlistData;
        row.trackIndex   = index;
        row.trackCount   = count;
        row.depth        = depth;
        row.queueIndexes = std::move(queueIndexes);
        row.queueTotal   = queueTotal;
    }
    void setTrackList(const TrackList* tracks)
    {
        context.tracks = tracks;
    }
    void setPlaybackState(uint64_t position, uint64_t duration, int bitrate, Player::PlayState state)
    {
        playback.position  = position;
        playback.duration  = duration;
        playback.bitrate   = bitrate;
        playback.playState = state;
    }
    void setOutputInfo(Engine::PlaybackOutputInfo info)
    {
        playback.outputInfo = std::move(info);
    }
    void setLibraryState(QString name, QString path)
    {
        libraries[-1]     = {.name = std::move(name), .path = std::move(path)};
        context.libraries = &libraries;
    }
    void setEvaluationState(TrackListContextPolicy policy, QString placeholder = {}, bool escapeRichText = false,
                            bool useVariousArtists = false, QString fullStarSymbol = {}, QString halfStarSymbol = {},
                            QString emptyStarSymbol = {}, bool replacePathSeparators = false)
    {
        auto symbols = defaultRatingStarSymbols();
        if(!fullStarSymbol.isEmpty())
            symbols.fullStarSymbol = std::move(fullStarSymbol);
        if(!halfStarSymbol.isEmpty())
            symbols.halfStarSymbol = std::move(halfStarSymbol);
        if(!emptyStarSymbol.isEmpty())
            symbols.emptyStarSymbol = std::move(emptyStarSymbol);
        options = {.trackListContextPolicy = policy,
                   .trackListPlaceholder   = std::move(placeholder),
                   .escapeRichText         = escapeRichText,
                   .useVariousArtists      = useVariousArtists,
                   .ratingSymbols          = std::move(symbols),
                   .replacePathSeparators  = replacePathSeparators};
    }
};

class ScriptParserTest : public ::testing::Test
{
protected:
    ScriptParserTest()
        : m_parser{&m_registry}
        , m_filter{ScriptSearchOptions{}, &m_registry}
    {
        for(const auto& variable : ContextVariables)
            m_registry.registerVariable(variable);
        for(const auto definitions : {std::span<const ScriptFunctionDescriptor>{CustomFunctions},
                                      std::span<const ScriptFunctionDescriptor>{ContextFunctions}}) {
            for(const auto& function : definitions)
                m_registry.registerFunction(function);
        }
        m_registry.registerVariable(makeScriptVariableDescriptor<[]() {
            return uR"(\<b>provider</b>\)"_s;
        }>(VariableKind::Generic, u"ESCAPETEST"_s));
        m_registry.finalise();
    }
    ScriptRegistry m_registry;
    ScriptParser m_parser;
    TrackQueryFilter m_filter;
};

TEST_F(ScriptParserTest, BasicLiteral)
{
    EXPECT_EQ(u"I am a test.", m_parser.evaluate(u"I am a test."_s));
    EXPECT_EQ(u"", m_parser.evaluate(QString{}));
}

TEST_F(ScriptParserTest, EscapeComment)
{
    const QString escapedPercent = uR"(I am a \% test.)"_s;
    const QString escapedQuote   = uR"("I am an \"escape test.")"_s;

    EXPECT_EQ(u"I am a % test.", m_parser.evaluate(escapedPercent));
    EXPECT_EQ(u"I am an \"escape test.", m_parser.evaluate(escapedQuote));
}

TEST_F(ScriptParserTest, LineComments)
{
    EXPECT_EQ(u"beforeafter", m_parser.evaluate(u"before\nafter"_s));
    EXPECT_EQ(u"beforeafter", m_parser.evaluate(u"before\n// ignored FooScript: $invalid(%field%\nafter"_s));
    EXPECT_EQ(u"beforeafter", m_parser.evaluate(u"before\r\n// ignored\r\nafter"_s));
    EXPECT_EQ(u"01", m_parser.evaluate(u"$num(\n// first argument\n1,2)"_s));
    EXPECT_EQ(u"// quoted", m_parser.evaluate(uR"("// quoted")"_s));
    EXPECT_EQ(u"// escaped", m_parser.evaluate(uR"(\// escaped)"_s));
    EXPECT_EQ(u"inline // text", m_parser.evaluate(u"inline // text"_s));
    EXPECT_EQ(u"  // indented", m_parser.evaluate(u"  // indented"_s));

    const ParsedScript commentOnly = m_parser.parse(u"// ignored"_s);
    EXPECT_TRUE(commentOnly.errors.empty());
    EXPECT_EQ(u"", m_parser.evaluate(commentOnly));
}

TEST_F(ScriptParserTest, LineCommentsPreserveLineEndings)
{
    const ScriptEvaluationOptions options{.whitespaceMode = ScriptWhitespaceMode::Preserve};

    EXPECT_EQ(u"before\n\nafter", m_parser.evaluate(u"before\n// ignored\nafter"_s, ScriptContext{}, options));
}

TEST_F(ScriptParserTest, Quote)
{
    EXPECT_EQ(u"I %am% a $test$.", m_parser.evaluate(uR"("I %am% a $test$.")"_s));
    EXPECT_EQ(uR"(\%)", m_parser.evaluate(uR"("\%")"_s));
    EXPECT_EQ(uR"(\d+)", m_parser.evaluate(uR"("\d+")"_s));
    EXPECT_EQ(uR"(\d+)", m_parser.evaluate(uR"("\\d+")"_s));
    EXPECT_EQ(uR"(C:\Music)", m_parser.evaluate(uR"("C:\\Music")"_s));
}

TEST_F(ScriptParserTest, StringTest)
{
    EXPECT_EQ(u"01", m_parser.evaluate(u"$num(1,2)"_s));
    EXPECT_EQ(u"04", m_parser.evaluate(u"$num(04,2)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$num()"_s));
    EXPECT_EQ(u"01", m_parser.evaluate(u"[$num(1,2)]"_s));
    EXPECT_EQ(u"A replace cesc", m_parser.evaluate(u"$replace(A replace test,t,c)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$replace()"_s));
    EXPECT_EQ(u"abc", m_parser.evaluate(u"$replace(abc,,x)"_s));
    EXPECT_EQ(u"A replace cesc", m_parser.evaluate(u"$replace(A replace test,A,A,t,c)"_s));
    EXPECT_EQ(u"aXXcdYY", m_parser.evaluate(u"$replace(abcde,b,XX,e,YY)"_s));
    EXPECT_EQ(u"ace", m_parser.evaluate(u"$replace(abcde,b,,d,)"_s));
    EXPECT_EQ(u"ayc", m_parser.evaluate(u"$replace(abc,,x,b,y)"_s));
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
    EXPECT_EQ(u"1", m_parser.evaluate(u"$strcmp(cmp,cmp)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$strcmp(cmp,cMp)"_s));
    EXPECT_EQ(u"1", m_parser.evaluate(u"$stricmp(cmp,cMp)"_s));
    EXPECT_EQ(u"true", m_parser.evaluate(u"$if($stricmp(cmp,cMp),true,false)"_s));
    EXPECT_EQ(u"false", m_parser.evaluate(u"$if($strcmp(cmp,cMp),true,false)"_s));
    EXPECT_EQ(u"a\037b\037c", m_parser.evaluate(u"$split(a;b;c,;)"_s));
    EXPECT_EQ(u"a\037b\037c", m_parser.evaluate(u"$split( a ; b ; c ,;)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$split(a;b;c,;,0)"_s));
    EXPECT_EQ(u"a", m_parser.evaluate(u"$split(a;b;c,;,1)"_s));
    EXPECT_EQ(u"b", m_parser.evaluate(u"$split(a;b;c,;,2)"_s));
    EXPECT_EQ(u"c", m_parser.evaluate(u"$split(a;b;c,;,3)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$split(a;b;c,;,4)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$join( - )"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$join( - ,,)"_s));
    EXPECT_EQ(u"Album", m_parser.evaluate(u"$join( - ,,Album)"_s));
    EXPECT_EQ(u"Artist", m_parser.evaluate(u"$join( - ,Artist,)"_s));
    EXPECT_EQ(u"Artist - Album", m_parser.evaluate(u"$join( - ,Artist,Album)"_s));
    EXPECT_EQ(u"A / B / C", m_parser.evaluate(u"$join( / ,A,,B,C)"_s));
    EXPECT_EQ(u"a", m_parser.evaluate(u"$shortest(longer,a,bb)"_s));
    EXPECT_EQ(u"first", m_parser.evaluate(u"$shortest(first,later)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$shortest()"_s));
    EXPECT_EQ(u"1", m_parser.evaluate(u"$strchr(abca,a)"_s));
    EXPECT_EQ(u"4", m_parser.evaluate(u"$strrchr(abca,a)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$strchr(abca,z)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$strrchr(abca,ab)"_s));
    EXPECT_EQ(u"1", m_parser.evaluate(u"$strstr(abca,a)"_s));
    EXPECT_EQ(u"3", m_parser.evaluate(u"$strstr(abca,c)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$strstr(abca,A)"_s));
    EXPECT_EQ(u"4", m_parser.evaluate(u"$strstr(abca,a,1)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$strstr(abca,a,invalid)"_s));
    EXPECT_EQ(u"1", m_parser.evaluate(u"$stristr(Abca,a)"_s));
    EXPECT_EQ(u"4", m_parser.evaluate(u"$stristr(Abca,A,1)"_s));
    EXPECT_EQ(u"4", m_parser.evaluate(u"$strstrlast(abca,a)"_s));
    EXPECT_EQ(u"1", m_parser.evaluate(u"$strstrlast(abca,a,2)"_s));
    EXPECT_EQ(u"4", m_parser.evaluate(u"$stristrlast(abcA,a)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$stristrlast(abca,z)"_s));

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
    EXPECT_EQ(u"3421780262", m_parser.evaluate(u"$crc32(123456789)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$crc32(a,b)"_s));
    EXPECT_EQ(u"00FF", m_parser.evaluate(u"$hex(255,4)"_s));
    EXPECT_EQ(u"CBF43926", m_parser.evaluate(u"$hex($crc32(123456789),8)"_s));
    EXPECT_EQ(u"FFFF", m_parser.evaluate(u"$hex(65535,2)"_s));
}

TEST_F(ScriptParserTest, ApplicationLinkTest)
{
    EXPECT_EQ(uR"(<a href="fooyin://application/run?application=player">Player</a>)",
              m_parser.evaluate(u"$applink(Player,player)"_s));
    EXPECT_EQ(uR"(<a href="fooyin://application/run?application=player&arguments=--play">Player</a>)",
              m_parser.evaluate(u"$applink(Player,player,--play)"_s));
    EXPECT_EQ(
        uR"(<a href="fooyin://application/run?application=/usr/bin/player&arguments=--play&directory=/tmp/Music">Player</a>)",
        m_parser.evaluate(u"$applink(Player,/usr/bin/player,--play,/tmp/Music)"_s));
    EXPECT_TRUE(m_parser.evaluate(u"$applink()"_s).isEmpty());
    EXPECT_TRUE(m_parser.evaluate(u"$applink(Player)"_s).isEmpty());
    EXPECT_TRUE(m_parser.evaluate(u"$applink(Player,)"_s).isEmpty());
    EXPECT_TRUE(m_parser.evaluate(u"$applink(Player,player,args,directory,extra)"_s).isEmpty());
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
    EXPECT_EQ(u"0", m_parser.evaluate(u"$select(2,-10,0,10)"_s));
    EXPECT_TRUE(m_parser.evaluate(u"$select(0,-10,0,10)"_s).isEmpty());
    EXPECT_TRUE(m_parser.evaluate(u"$select(4,-10,0,10)"_s).isEmpty());
}

TEST_F(ScriptParserTest, TimeDateFunctionTest)
{
    EXPECT_EQ(u"2024", m_parser.evaluate(u"$year(\"2024-03-09 08:07:06\")"_s));
    EXPECT_EQ(u"2024", m_parser.evaluate(u"$year(2024)"_s));
    EXPECT_TRUE(m_parser.evaluate(u"$year(0000)"_s).isEmpty());
    EXPECT_EQ(u"03", m_parser.evaluate(u"$month(\"2024-03-09 08:07:06\")"_s));
    EXPECT_EQ(u"01", m_parser.evaluate(u"$month(2024)"_s));
    EXPECT_EQ(u"09", m_parser.evaluate(u"$day_of_month(\"2024-03-09 08:07:06\")"_s));
    EXPECT_EQ(u"01", m_parser.evaluate(u"$day_of_month(\"2024-03\")"_s));
    EXPECT_EQ(u"2024-03-09", m_parser.evaluate(u"$date(\"2024-03-09 08:07:06\")"_s));
    EXPECT_EQ(u"2024-01-01", m_parser.evaluate(u"$date(2024)"_s));
    EXPECT_EQ(u"2024-02-29", m_parser.evaluate(u"$date(\"2024-02-29\")"_s));
    EXPECT_EQ(u"08:07:06", m_parser.evaluate(u"$time(\"2024-03-09 08:07:06\")"_s));
    EXPECT_EQ(u"08:07", m_parser.evaluate(u"$time(\"2024-03-09 08:07\")"_s));
    EXPECT_EQ(u"08:00", m_parser.evaluate(u"$time(\"2024-03-09 08\")"_s));
    EXPECT_EQ(u"23:59:59", m_parser.evaluate(u"$time(\"2024-03-09 23:59:59\")"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$time(\"2024-03-09\")"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$year(not-a-date)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$date(\"2023-02-29\")"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$date(\"2024-13-01\")"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$date(\"2024/03/09\")"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$date(\"2024-3-09\")"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$time(\"2024-03-09 24:00:00\")"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$time(\"2024-03-09 08:60:00\")"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$time(\"2024-03-09 08:07:60\")"_s));
}

TEST_F(ScriptParserTest, ConditionalTest)
{
    EXPECT_EQ(u"", m_parser.evaluate(u"[literal]"_s));
    EXPECT_EQ(u"1shown", m_parser.evaluate(u"[$strcmp(a,a)shown]"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"[$strcmp(a,b)shown]"_s));
    EXPECT_EQ(u"yes", m_parser.evaluate(u"$if([$strcmp(a,a)],yes,no)"_s));
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
    EXPECT_EQ(u"false", m_parser.evaluate(u"[$iflonger(aaa,3,true,false)]"_s));
    EXPECT_EQ(u"one", m_parser.evaluate(u"$select(1,one,two,three)"_s));
    EXPECT_EQ(u"three", m_parser.evaluate(u"$select(3,one,two,three)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$select(0,one,two,three)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(u"$select(4,one,two,three)"_s));
    EXPECT_EQ(u"oneone", m_parser.evaluate(u"$select(1,$put(value,one),$put(value,two))$get(value)"_s));
    EXPECT_EQ(u"then", m_parser.evaluate(u"$if(1,$puts(value,then),$puts(value,else))$get(value)"_s));
    EXPECT_EQ(u"yes", m_parser.evaluate(u"$if2(yes,$puts(value,fallback))$get(value)"_s));
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
    EXPECT_EQ(u"yes", m_parser.evaluate(u"$if(%disc%%track%,yes,no)"_s, track));
    EXPECT_EQ(u"no", m_parser.evaluate(u"$if(%disc%literal,yes,no)"_s, track));
    EXPECT_EQ(u"yes", m_parser.evaluate(u"$if($strcmp(a,a)literal,yes,no)"_s, track));
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

    EXPECT_EQ(u" - 7", m_parser.evaluate(u"[%disc% - %track%]"_s, track));
}

TEST_F(ScriptParserTest, RegexTest)
{
    EXPECT_EQ(u"true", m_parser.evaluate(uR"REGEX($if($regex_test(Track 42,"^Track \d+$"),true,false))REGEX"_s));
    EXPECT_EQ(u"true", m_parser.evaluate(uR"REGEX($if($regex_test(FOOYIN,"^fooyin$",i),true,false))REGEX"_s));
    EXPECT_EQ(u"false", m_parser.evaluate(uR"REGEX($if($regex_test(FOOYIN,"^fooyin$"),true,false))REGEX"_s));
    EXPECT_EQ(u"false", m_parser.evaluate(uR"REGEX($if($regex_test(text,"["),true,false))REGEX"_s));
    EXPECT_EQ(u"false", m_parser.evaluate(uR"REGEX($if($regex_test(text,"text",g),true,false))REGEX"_s));
    EXPECT_EQ(u"Track 42", m_parser.evaluate(uR"REGEX($regex_match(Track 42,"Track (\d+)"))REGEX"_s));
    EXPECT_EQ(u"42", m_parser.evaluate(uR"REGEX($regex_match(Track 42,"Track (\d+)",1))REGEX"_s));
    EXPECT_EQ(u"FOOYIN", m_parser.evaluate(uR"REGEX($regex_match(FOOYIN,"(fooyin)",1,i))REGEX"_s));
    EXPECT_EQ(u"42", m_parser.evaluate(uR"REGEX($regex_match(Track 42,"Track (?<number>\d+)",number))REGEX"_s));
    EXPECT_EQ(u"", m_parser.evaluate(uR"REGEX($regex_match(Track 42,"Track (\d+)",2))REGEX"_s));
    EXPECT_EQ(u"", m_parser.evaluate(uR"REGEX($regex_match(Track 42,"Track (\d+)",missing))REGEX"_s));
    EXPECT_EQ(u"A1 / B22 / C333", m_parser.evaluate(uR"REGEX($regex_matches(A1 B22 C333,"[A-Z]\d+"," / "))REGEX"_s));
    EXPECT_EQ(u"1 / 22 / 333", m_parser.evaluate(uR"REGEX($regex_matches(A1 B22 C333,"([A-Z])(\d+)"," / ",2))REGEX"_s));
    EXPECT_EQ(u"1 / 22 / 333",
              m_parser.evaluate(uR"REGEX($regex_matches(a1 B22 c333,"([a-z])(\d+)"," / ",2,i))REGEX"_s));
    EXPECT_EQ(u"Track #42", m_parser.evaluate(uR"REGEX($regex_replace(Track 42,"(\d+)","#\1"))REGEX"_s));
    EXPECT_EQ(u"one cat two cat", m_parser.evaluate(u"$regex_replace(one DOG two dog,dog,cat,i)"_s));
    EXPECT_EQ(u"", m_parser.evaluate(uR"REGEX($regex_replace(text,"[",replacement))REGEX"_s));
}

TEST_F(ScriptParserTest, ReplaceCanSanitiseUserFilenameExpression)
{
    Track track;
    track.setDiscNumber(u"2"_s);
    track.setTrackNumber(u"13"_s);
    track.setTitle(u"What? A \"Title\": Part | Two"_s);

    static auto nestedReplaceScript
        = uR"($trim($replace($replace($replace($replace($iflonger(%disc%,0,%disc%.,)$iflonger(%track%,0,%track%.,) %title%,?, ),\",),:, ),|,)))"_s;
    static auto variadicReplaceScript
        = uR"($trim($replace($iflonger(%disc%,0,%disc%.,)$iflonger(%track%,0,%track%.,) %title%,?, ,\",,:, ,|,)))"_s;

    EXPECT_EQ(u"2.13. What  A Title  Part  Two", m_parser.evaluate(nestedReplaceScript, track));
    EXPECT_EQ(u"2.13. What  A Title  Part  Two", m_parser.evaluate(variadicReplaceScript, track));
}

TEST_F(ScriptParserTest, RatingStarsFormattingTest)
{
    ScriptParser parser{&m_registry};

    TestScriptContext environment;
    environment.setEvaluationState(TrackListContextPolicy::Unresolved, {}, false, false, u"*"_s, u"+"_s, u"."_s);

    auto& context = environment.context;

    Track track;

    EXPECT_EQ(u"", parser.evaluate(u"%rating_stars%"_s, track, context, environment.options));
    EXPECT_EQ(u".....", parser.evaluate(u"%rating_stars_padded%"_s, track, context, environment.options));

    track.setRatingStars(7);
    EXPECT_EQ(u"***+", parser.evaluate(u"%rating_stars%"_s, track, context, environment.options));
    EXPECT_EQ(u"***+.", parser.evaluate(u"%rating_stars_padded%"_s, track, context, environment.options));
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

TEST_F(ScriptParserTest, MultiValueScriptWritesAcceptSingleStrings)
{
    Track track;

    setTrackScriptValue(u"artist"_s, u"Artist"_s, track);
    setTrackScriptValue(u"albumartist"_s, u"Album Artist"_s, track);
    setTrackScriptValue(u"genre"_s, u"Genre"_s, track);
    setTrackScriptValue(u"composer"_s, u"Composer"_s, track);
    setTrackScriptValue(u"performer"_s, u"Performer"_s, track);

    EXPECT_EQ(track.artists(), QStringList{u"Artist"_s});
    EXPECT_EQ(track.albumArtists(), QStringList{u"Album Artist"_s});
    EXPECT_EQ(track.genres(), QStringList{u"Genre"_s});
    EXPECT_EQ(track.composers(), QStringList{u"Composer"_s});
    EXPECT_EQ(track.performers(), QStringList{u"Performer"_s});

    setTrackScriptValue(u"genre"_s, QString{}, track);
    EXPECT_FALSE(track.hasGenres());
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
    ScriptParser parser{&m_registry};

    TestScriptContext environment;
    environment.setEvaluationState(TrackListContextPolicy::Fallback);

    auto& context = environment.context;

    const QString script = u"[%trackcount% $ifequal(%trackcount%,1,Track,Tracks) | %playtime%]$crlf()Test"_s;
    EXPECT_EQ(u"0 Tracks | 00:00\nTest", parser.evaluate(script, TrackList{}, context, environment.options));
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

TEST_F(ScriptParserTest, TrackArtistTest)
{
    Track track;
    track.setArtists({u"Artist"_s});

    EXPECT_EQ(u"", m_parser.evaluate(u"%trackartist%"_s, track));
    EXPECT_EQ(u"", m_parser.evaluate(u"%uniqueartist%"_s, track));

    track.setAlbumArtists({u"Album Artist"_s});
    EXPECT_EQ(u"Artist", m_parser.evaluate(u"%trackartist%"_s, track));
    EXPECT_EQ(u"Artist", m_parser.evaluate(u"%uniqueartist%"_s, track));

    track.setArtists({u"Album Artist"_s});
    EXPECT_EQ(u"", m_parser.evaluate(u"%trackartist%"_s, track));

    track.setArtists({});
    track.setAlbumArtists({});
    track.setComposers({u"Composer"_s});
    EXPECT_EQ(u"", m_parser.evaluate(u"%trackartist%"_s, track));
}

TEST_F(ScriptParserTest, InfoTest)
{
    Track track;
    track.setChannels(2);

    EXPECT_EQ(u"Stereo", m_parser.evaluate(u"%channels%"_s, track));
    EXPECT_EQ(u"2", m_parser.evaluate(u"$info(channels)"_s, track));
}

TEST_F(ScriptParserTest, PlaylistRowContextVariables)
{
    ScriptParser parser{&m_registry};

    TestScriptContext environment;
    environment.setState(4, 12, 2, {});

    auto& context = environment.context;
    const Track track;

    EXPECT_EQ(u"05", parser.evaluate(u"%list_index%"_s, track, context, environment.options));
    EXPECT_EQ(u"2", parser.evaluate(u"%depth%"_s, track, context, environment.options));
}

TEST_F(ScriptParserTest, VariableDefinitionsInstallIntoRegistry)
{
    ScriptParser parser{&m_registry};
    const Track track;

    EXPECT_EQ(QString::fromLatin1(Constants::FrontCover), parser.evaluate(u"%frontcover%"_s, track));
}

TEST_F(ScriptParserTest, StatefulVariableDefinitionsInstallIntoRegistry)
{
    ScriptParser parser{&m_registry};
    TestProviderContext environment;
    environment.setState(u"stateful"_s, {});
    auto& context = environment.context;
    const Track track;

    EXPECT_EQ(u"stateful", parser.evaluate(u"%statefulvar%"_s, track, context));
}

TEST_F(ScriptParserTest, FunctionDefinitionsInstallIntoRegistry)
{
    ScriptParser parser{&m_registry};
    const ParsedScript parsed = parser.parse(u"$prefix(test)"_s);

    EXPECT_EQ(u"fn:test", parser.evaluate(parsed));
    EXPECT_EQ(u"fn:test", parser.evaluate(parsed));
}

TEST_F(ScriptParserTest, StatefulFunctionDefinitionsInstallIntoRegistry)
{
    ScriptParser parser{&m_registry};
    TestProviderContext environment;
    environment.setState({}, u"ctx:"_s);
    auto& context             = environment.context;
    const ParsedScript parsed = parser.parse(u"$stateful_prefix(test)"_s);

    EXPECT_EQ(u"ctx:test", parser.evaluate(parsed, context));
    EXPECT_EQ(u"ctx:test", parser.evaluate(parsed, context));
}

TEST_F(ScriptParserTest, ContextVariables)
{
    ScriptParser parser{&m_registry};

    TestScriptContext environment;
    environment.setState(4, 12, 2, {0, 2}, 7);

    auto& context = environment.context;
    const Track track;

    EXPECT_EQ(u"05", parser.evaluate(u"%list_index%"_s, track, context, environment.options));
    EXPECT_EQ(u"2", parser.evaluate(u"%depth%"_s, track, context, environment.options));
    EXPECT_EQ(u"1", parser.evaluate(u"%queueindex%"_s, track, context, environment.options));
    EXPECT_EQ(u"1", parser.evaluate(u"%queue_index%"_s, track, context, environment.options));
    EXPECT_EQ(u"1, 3", parser.evaluate(u"%queueindexes%"_s, track, context, environment.options));
    EXPECT_EQ(u"1, 3", parser.evaluate(u"%queue_indexes%"_s, track, context, environment.options));
    EXPECT_EQ(u"[1, 3]", parser.evaluate(uR"([\[%queueindexes%\]])"_s, track, context, environment.options));
    EXPECT_EQ(u"7", parser.evaluate(u"%queuetotal%"_s, track, context, environment.options));
    EXPECT_EQ(u"7", parser.evaluate(u"%queue_total%"_s, track, context, environment.options));
}

TEST_F(ScriptParserTest, QueueTotalRequiresQueuedTrack)
{
    ScriptParser parser{&m_registry};

    TestScriptContext environment;
    environment.setState(4, 12, 2, {}, 7);

    auto& context = environment.context;
    const Track track;

    EXPECT_EQ(u"", parser.evaluate(u"%queuetotal%"_s, track, context, environment.options));
    EXPECT_EQ(u"", parser.evaluate(u"%queue_total%"_s, track, context, environment.options));
}

TEST_F(ScriptParserTest, EvaluationOptionsControlPolicyAndEscaping)
{
    ScriptParser parser{&m_registry};

    TestScriptContext environment;
    environment.setEvaluationState(TrackListContextPolicy::Placeholder, u"|Loading|"_s, true);

    auto& context = environment.context;
    Track track;
    track.setTitle(u"A < B"_s);

    EXPECT_EQ(u"|Loading|", parser.evaluate(u"%trackcount%"_s, track, context, environment.options));
    EXPECT_EQ(u"A \\< B", parser.evaluate(u"%title%"_s, track, context, environment.options));
}

TEST_F(ScriptParserTest, EvaluationOptionsPreservePathVariableSeparators)
{
    ScriptParser parser{&m_registry};

    const QString libraryPath = QFileInfo{u"/tmp/music"_s}.absoluteFilePath();
    const QString filePath    = QFileInfo{u"/tmp/music/foo/bar.mp3"_s}.absoluteFilePath();

    TestScriptContext environment;
    environment.setEvaluationState(TrackListContextPolicy::Unresolved, {}, false, false, {}, {}, {}, true);
    environment.setLibraryState({}, libraryPath);

    auto& context = environment.context;
    Track track;
    track.setFilePath(filePath);
    track.setTitle(u"foo/bar"_s);

    EXPECT_EQ(QFileInfo{filePath}.path(), parser.evaluate(u"%path%"_s, track, context, environment.options));
    EXPECT_EQ(filePath, parser.evaluate(u"%filepath%"_s, track, context, environment.options));
    EXPECT_EQ(u"foo/bar.mp3", parser.evaluate(u"%relativepath%"_s, track, context, environment.options));
    EXPECT_EQ(u"foo-bar", parser.evaluate(u"%title%"_s, track, context, environment.options));
}

TEST_F(ScriptParserTest, RichTextEscapingPreservesMetadataAndPathBackslashes)
{
    TestScriptContext environment;
    environment.setEvaluationState(TrackListContextPolicy::Unresolved, {}, true);

    auto& context = environment.context;
    Track track;
    track.setAlbum(uR"(/// ABCD \\)"_s);
    track.setFilePath(uR"(/tmp/music/\<album>\track.flac)"_s);

    EXPECT_EQ(uR"(/// ABCD \\\\)"_s, m_parser.evaluate(u"%album%"_s, track, context, environment.options));
    EXPECT_EQ(uR"(/tmp/music/\\\<album>\\track.flac)"_s,
              m_parser.evaluate(u"%filepath%"_s, track, context, environment.options));
    EXPECT_EQ(track.album(), m_parser.evaluate(u"%album%"_s, track));
    EXPECT_EQ(track.filepath(), m_parser.evaluate(u"%filepath%"_s, track));
}

TEST_F(ScriptParserTest, ProviderVariablesRespectRichTextEscaping)
{
    TestScriptContext environment;
    environment.setEvaluationState(TrackListContextPolicy::Unresolved, {}, true);
    const auto& context = environment.context;
    const Track track;
    const TrackList tracks{track};
    const QString escaped = uR"(\\\<b>provider\</b>\\)"_s;

    EXPECT_EQ(escaped, m_parser.evaluate(u"%escapetest%"_s, track, context, environment.options));
    EXPECT_EQ(escaped, m_parser.evaluate(u"%escapetest%"_s, tracks, context, environment.options));
    EXPECT_EQ(escaped, m_parser.evaluate(u"%escapetest%"_s, context, environment.options));
    EXPECT_EQ(uR"(\<b>provider</b>\)"_s, m_parser.evaluate(u"%escapetest%"_s, track));
}

TEST_F(ScriptParserTest, ContextTrackListProvidesFallbackData)
{
    ScriptParser parser{&m_registry};

    TrackList tracks;

    Track track1;
    track1.setGenres({u"Pop"_s});
    track1.setDuration(2000);
    tracks.push_back(track1);

    Track track2;
    track2.setGenres({u"Rock"_s});
    track2.setDuration(3000);
    tracks.push_back(track2);

    TestScriptContext environment;
    environment.setTrackList(&tracks);
    environment.setEvaluationState(TrackListContextPolicy::Fallback);

    auto& context = environment.context;
    const Track track;

    EXPECT_EQ(u"2", parser.evaluate(u"%trackcount%"_s, track, context, environment.options));
    EXPECT_EQ(u"00:05", parser.evaluate(u"%playtime%"_s, track, context, environment.options));
}

TEST_F(ScriptParserTest, PlaybackContextProvidesPlaybackVariables)
{
    ScriptParser parser{&m_registry};

    TestScriptContext environment;
    environment.setPlaybackState(45000, 120000, 320, Player::PlayState::Playing);

    auto& context = environment.context;
    Track track;
    track.setBitrate(192);

    EXPECT_EQ(u"00:45", parser.evaluate(u"%playback_time%"_s, track, context, environment.options));
    EXPECT_EQ(u"45", parser.evaluate(u"%playback_time_seconds%"_s, track, context, environment.options));
    EXPECT_EQ(u"75", parser.evaluate(u"%playback_time_remaining_s%"_s, track, context, environment.options));
    EXPECT_EQ(u"75", parser.evaluate(u"%playback_time_remaining_seconds%"_s, track, context, environment.options));
    EXPECT_EQ(u"1", parser.evaluate(u"%isplaying%"_s, track, context, environment.options));
    EXPECT_EQ(u"320", parser.evaluate(u"%bitrate%"_s, track, context, environment.options));
}

TEST_F(ScriptParserTest, PlaybackTimeRemainingFollowsElapsedSecondBoundaries)
{
    ScriptParser parser{&m_registry};

    TestScriptContext environment;
    auto& context = environment.context;

    const Track track;

    environment.setPlaybackState(1005, 254013, 320, Player::PlayState::Playing);
    EXPECT_EQ(u"04:13", parser.evaluate(u"%playback_time_remaining%"_s, track, context, environment.options));
    EXPECT_EQ(u"253", parser.evaluate(u"%playback_time_remaining_s%"_s, track, context, environment.options));

    environment.setPlaybackState(2015, 254013, 320, Player::PlayState::Playing);
    EXPECT_EQ(u"04:12", parser.evaluate(u"%playback_time_remaining%"_s, track, context, environment.options));
    EXPECT_EQ(u"252", parser.evaluate(u"%playback_time_remaining_s%"_s, track, context, environment.options));
}

TEST_F(ScriptParserTest, PlaybackContextProvidesOutputVariables)
{
    ScriptParser parser{&m_registry};

    TestScriptContext environment;
    environment.setPlaybackState(1000, 120000, 320, Player::PlayState::Playing);
    Engine::PlaybackOutputInfo info{
        .format         = AudioFormat{SampleFormat::S24In32, 96000, 2},
        .device         = u"DAC"_s,
        .dsps           = {u"Resampler"_s, u"Equalizer"_s},
        .dspPreset      = u"Headphones"_s,
        .volume         = 0.5,
        .bufferLengthMs = 200,
        .replayGain     = {.source     = Engine::ReplayGainSource::Track,
                           .processing = Engine::ApplyGain | Engine::PreventClipping,
                           .gainDb     = -3.5,
                           .peak       = 0.9,
                           .hasPeak    = true},
    };
    info.format.setChannelLayout({AudioFormat::ChannelPosition::FrontLeft, AudioFormat::ChannelPosition::FrontRight});
    environment.setOutputInfo(std::move(info));

    auto& context = environment.context;
    const Track track;

    EXPECT_EQ(u"96000", parser.evaluate(u"%output_samplerate%"_s, track, context, environment.options));
    EXPECT_EQ(u"2", parser.evaluate(u"%output_channels%"_s, track, context, environment.options));
    EXPECT_EQ(u"FL,FR", parser.evaluate(u"%output_channel_mask%"_s, track, context, environment.options));
    EXPECT_EQ(u"24", parser.evaluate(u"%output_bitdepth%"_s, track, context, environment.options));
    EXPECT_EQ(u"DAC", parser.evaluate(u"%output_device%"_s, track, context, environment.options));
    EXPECT_EQ(u"Resampler, Equalizer", parser.evaluate(u"%output_dsps%"_s, track, context, environment.options));
    EXPECT_EQ(u"Headphones", parser.evaluate(u"%output_dsp_preset%"_s, track, context, environment.options));
    EXPECT_EQ(u"-6.02 dB", parser.evaluate(u"%output_volume%"_s, track, context, environment.options));
    EXPECT_EQ(u"Track", parser.evaluate(u"%output_rg_source%"_s, track, context, environment.options));
    EXPECT_EQ(u"Gain and clip prevention", parser.evaluate(u"%output_rg_mode%"_s, track, context, environment.options));
    EXPECT_EQ(u"-3.50 dB", parser.evaluate(u"%output_rg_gain%"_s, track, context, environment.options));
    EXPECT_EQ(u"0.9", parser.evaluate(u"%output_rg_peak%"_s, track, context, environment.options));
    EXPECT_EQ(u"-0.92 dBFS", parser.evaluate(u"%output_rg_peak_db%"_s, track, context, environment.options));
    EXPECT_EQ(u"200", parser.evaluate(u"%output_buffer_length%"_s, track, context, environment.options));
}

TEST_F(ScriptParserTest, LibraryContextProvidesLibraryVariables)
{
    ScriptParser parser{&m_registry};

    TestScriptContext environment;
    environment.setLibraryState(u"Main"_s, u"/music"_s);

    auto& context = environment.context;
    Track track;
    track.setFilePath(u"/music/Artist/Album/Track.flac"_s);

    EXPECT_EQ(u"Main", parser.evaluate(u"%libraryname%"_s, track, context, environment.options));
    EXPECT_EQ(u"/music", parser.evaluate(u"%librarypath%"_s, track, context, environment.options));
    EXPECT_EQ(u"Artist/Album/Track.flac", parser.evaluate(u"%relativepath%"_s, track, context, environment.options));
}

TEST_F(ScriptParserTest, TrackEvaluationDoesNotUseTrackListContextByDefault)
{
    ScriptParser parser{&m_registry};

    TrackList tracks;

    Track track1;
    track1.setDuration(2000);
    tracks.push_back(track1);

    Track track2;
    track2.setDuration(3000);
    tracks.push_back(track2);

    TestScriptContext environment;
    environment.setTrackList(&tracks);

    auto& context = environment.context;

    EXPECT_EQ(u"%TRACKCOUNT%", parser.evaluate(u"%trackcount%"_s, track1, context, environment.options));
    EXPECT_EQ(u"%PLAYLIST_DURATION%", parser.evaluate(u"%playlist_duration%"_s, track1, context, environment.options));
}

TEST_F(ScriptParserTest, TrackEvaluationCanUseTrackListContextWhenEnabled)
{
    ScriptParser parser{&m_registry};

    TrackList tracks;

    Track track1;
    track1.setDuration(2000);
    tracks.push_back(track1);

    Track track2;
    track2.setDuration(3000);
    tracks.push_back(track2);

    TestScriptContext environment;
    environment.setTrackList(&tracks);
    environment.setEvaluationState(TrackListContextPolicy::Fallback);

    auto& context = environment.context;

    EXPECT_EQ(u"2", parser.evaluate(u"%trackcount%"_s, track1, context, environment.options));
    EXPECT_EQ(u"00:05", parser.evaluate(u"%playlist_duration%"_s, track1, context, environment.options));
}

TEST_F(ScriptParserTest, TrackEvaluationCanUseTrackListPlaceholderPolicy)
{
    ScriptParser parser{&m_registry};

    TrackList tracks;

    Track track1;
    track1.setDuration(2000);
    tracks.push_back(track1);

    TestScriptContext environment;
    environment.setTrackList(&tracks);
    environment.setEvaluationState(TrackListContextPolicy::Placeholder, u"|Loading|"_s);

    auto& context = environment.context;

    EXPECT_EQ(u"|Loading|", parser.evaluate(u"%trackcount%"_s, track1, context, environment.options));
    EXPECT_EQ(u"|Loading|", parser.evaluate(u"%playlist_duration%"_s, track1, context, environment.options));
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
    EXPECT_EQ(1, m_filter.filter(u"$info(duration)=210000"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"playcount>1"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"playcount GREATER 1"_s, tracks).size());
    EXPECT_EQ(0, m_filter.filter(u"playcount LESS 1"_s, tracks).size());
    EXPECT_EQ(2, m_filter.filter(u"playcount>=1"_s, tracks).size());
    EXPECT_EQ(0, m_filter.filter(u"playcount>=A"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"stars>=3"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"rating>=3"_s, tracks).size());
    EXPECT_EQ(2, m_filter.filter(u"rating>1"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"rating_normalized>=0.6"_s, tracks).size());
    EXPECT_EQ(0, m_filter.filter(u"$unknown(duration)=210000"_s, tracks).size());

    // Logical operator tests
    EXPECT_EQ(1, m_filter.filter(u"title=Wandering Horizon AND genre MISSING"_s, tracks).size());
    EXPECT_EQ(2, m_filter.filter(u"playcount=1 OR playcount=8"_s, tracks).size());
    EXPECT_EQ(2, m_filter.filter(u"playcount=1 XOR playcount=8"_s, tracks).size());
    EXPECT_EQ(0, m_filter.filter(u"playcount=1 XOR playcount=1"_s, tracks).size());

    // Negation test
    EXPECT_EQ(1, m_filter.filter(u"!playcount=1"_s, tracks).size());
    EXPECT_EQ(0, m_filter.filter(u"NOT playcount>=1"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"NOT (playcount=1 OR bitrate=1000)"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"NOT (playcount=8 AND bitrate=950)"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"playcount NOT = 1"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"playcount NOT GREATER 1"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"playcount NOT LESS 8"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"playcount NOT >= 8"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"bitrate NOT < 1000"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"title NOT : Celestial"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"title NOT = Wandering Horizon"_s, tracks).size());

    // PRESENT/MISSING keyword tests
    EXPECT_EQ(1, m_filter.filter(u"performer PRESENT"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"performer MISSING"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"performer NOT PRESENT"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"performer NOT MISSING"_s, tracks).size());
    EXPECT_EQ(0, m_filter.filter(u"rating_stars PRESENT"_s, tracks).size());
    EXPECT_EQ(2, m_filter.filter(u"rating_stars MISSING"_s, tracks).size());

    // String matching tests
    EXPECT_EQ(1, m_filter.filter(u"title:wandering hor"_s, tracks).size());
    EXPECT_EQ(0, m_filter.filter(u"title=wandering hor"_s, tracks).size());
    EXPECT_EQ(2, m_filter.filter(u"title:Wa"_s, tracks).size());
    track1.setArtists({u"Ke$ha"_s});
    track1.setComment(u"Costs $5 [sale] 100% %artist%"_s);
    tracks[0] = track1;
    EXPECT_EQ(1, m_filter.filter(u"Ke$ha"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"$5"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"$"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"[sale]"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"100%"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"%artist%"_s, tracks).size());

    // Date comparisons
    EXPECT_EQ(1, m_filter.filter(u"date BEFORE 2000"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"date AFTER 2000"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"firstplayed SINCE 2022"_s, tracks).size());
    EXPECT_EQ(2, m_filter.filter(u"lastplayed DURING LAST WEEK"_s, tracks).size());
    EXPECT_EQ(2, m_filter.filter(u"lastplayed DURING LAST MONTH"_s, tracks).size());
    EXPECT_EQ(2, m_filter.filter(u"lastplayed DURING LAST YEAR"_s, tracks).size());
    EXPECT_EQ(0, m_filter.filter(u"lastplayed DURING 2"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"date NOT BEFORE 2000"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"date NOT AFTER 2000"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"firstplayed NOT SINCE 2022"_s, tracks).size());
    EXPECT_EQ(0, m_filter.filter(u"lastplayed NOT DURING LAST WEEK"_s, tracks).size());
    EXPECT_EQ(2, m_filter.filter(u"%lastplayed% NOT DURING LAST 1 DAYS"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"%firstplayed% NOT DURING LAST 3 MONTHS"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"%firstplayed% NOT DURING LAST 2 YEARS"_s, tracks).size());

    // Grouping and complex queries
    EXPECT_EQ(2, m_filter.filter(u"(playcount>=1 AND bitrate>500) OR title:Celestial"_s, tracks).size());
    EXPECT_EQ(2, m_filter.filter(u"(playcount>=1 AND (bitrate>500 OR bitrate=950))"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"(playcount=8 OR (bitrate>950 AND duration>200000))"_s, tracks).size());
    EXPECT_EQ(0, m_filter.filter(u"(playcount=8 AND (bitrate<900 OR duration<180000))"_s, tracks).size());

    QString query = u"(title:Wand AND album:Elec) OR (title:Celest AND playcount=8)"_s;
    EXPECT_EQ(2, m_filter.filter(query, tracks).size());
    query = u"(title:Wandering OR (playcount=1 AND bitrate>900))"_s;
    EXPECT_EQ(1, m_filter.filter(query, tracks).size());
    query = u"((playcount>=1 AND bitrate>500) OR title:Celest) AND (duration_ms>180000)"_s;
    EXPECT_EQ(2, m_filter.filter(query, tracks).size());
}

TEST(ScriptScannerTest, RecognisesTextBoundaryKeywords)
{
    const std::array cases{
        std::pair{u"STARTS"_s, ScriptScanner::TokStarts}, std::pair{u"ENDS"_s, ScriptScanner::TokEnds},
        std::pair{u"WITH"_s, ScriptScanner::TokWith},     std::pair{u"IN"_s, ScriptScanner::TokIn},
        std::pair{u"ANY"_s, ScriptScanner::TokAny},       std::pair{u"EQUAL"_s, ScriptScanner::TokEquals}};

    for(const auto& [input, expected] : cases) {
        ScriptScanner scanner;
        scanner.setup(input);

        const auto token = scanner.next();
        EXPECT_EQ(expected, token.type);
        EXPECT_EQ(input, token.value);
        EXPECT_EQ(ScriptScanner::TokEos, scanner.next().type);
    }
}

TEST(ScriptScannerTest, KeywordsRequireExactLengths)
{
    for(const auto& keyword : {u"STARTS"_s, u"ENDS"_s, u"WITH"_s, u"IN"_s, u"ANY"_s, u"EQUAL"_s, u"AND"_s}) {
        for(qsizetype length{1}; length < keyword.size(); ++length) {
            const QString input = keyword.left(length);
            ScriptScanner scanner;
            scanner.setup(input);

            const auto token = scanner.next();
            EXPECT_EQ(ScriptScanner::TokLiteral, token.type);
            EXPECT_EQ(input, token.value);
        }
    }

    for(const auto& input : {u"STARTSWITH"_s, u"ENDSWITH"_s, u"STARTSWITHOUT"_s, u"ENDSWITHOUT"_s, u"WITHOUT"_s,
                             u"INDEX"_s, u"EQUALS"_s, u"ANDROID"_s, u"ANYTHING"_s, u"HASANY"_s}) {
        ScriptScanner scanner;
        scanner.setup(input);

        const auto token = scanner.next();
        EXPECT_EQ(ScriptScanner::TokLiteral, token.type);
        EXPECT_EQ(input, token.value);
    }
}

TEST_F(ScriptParserTest, QueryTextBoundaryOperators)
{
    Track first{u"/music/first.flac"_s};
    first.setTitle(u"Café Moonlight"_s);

    Track second{u"/music/second.flac"_s};
    second.setTitle(u"Moonlight Cafe\u0301"_s);

    Track middle{u"/music/middle.flac"_s};
    middle.setTitle(u"A café at dusk"_s);

    const TrackList tracks{first, second, middle};

    EXPECT_EQ(TrackList{first}, m_filter.filter(uR"(title STARTS WITH "CAFE")"_s, tracks));
    EXPECT_EQ(TrackList{second}, m_filter.filter(uR"(title ENDS WITH "CAFE")"_s, tracks));
    EXPECT_EQ(TrackList{second}, m_filter.filter(uR"(title STARTS WITH "moon")"_s, tracks));
    EXPECT_EQ(TrackList{first}, m_filter.filter(uR"(title ENDS WITH "LIGHT")"_s, tracks));
    EXPECT_EQ(tracks, m_filter.filter(uR"(title HAS "cafe")"_s, tracks));
    EXPECT_EQ((TrackList{second, middle}), m_filter.filter(uR"(title NOT STARTS WITH "cafe")"_s, tracks));
    EXPECT_EQ((TrackList{first, middle}), m_filter.filter(uR"(title NOT ENDS WITH "cafe")"_s, tracks));
    EXPECT_EQ((TrackList{second, middle}), m_filter.filter(uR"(NOT (title STARTS WITH "cafe"))"_s, tracks));
    EXPECT_EQ(TrackList{first}, m_filter.filter(uR"(title STARTS WITH "cafe" AND title ENDS WITH "light")"_s, tracks));
    EXPECT_EQ((TrackList{first, second}),
              m_filter.filter(uR"(title STARTS WITH "cafe" OR title ENDS WITH "cafe")"_s, tracks));
    EXPECT_EQ(TrackList{first}, m_filter.filter(uR"($lower(%title%) STARTS WITH "cafe")"_s, tracks));
    EXPECT_EQ(TrackList{first}, m_filter.filter(u"title STARTS\tWITH \"cafe\""_s, tracks));
    EXPECT_EQ(TrackList{first}, m_filter.filter(u"title ENDS\n  WITH \"light\""_s, tracks));
    EXPECT_EQ(TrackList{first}, m_filter.filter(u"title STARTS\n// Prefix match\nWITH \"cafe\""_s, tracks));
}

TEST_F(ScriptParserTest, QueryTextBoundaryOperatorsRequireWith)
{
    for(const auto& query :
        {u"title STARTS"_s, u"title ENDS"_s, uR"(title STARTS "moon")"_s, uR"(title ENDS "light")"_s,
         uR"(title STARTS WITHOUT "moon")"_s, uR"(title ENDS WI "light")"_s}) {
        SCOPED_TRACE(query.toStdString());
        const auto parsed = m_parser.parseQuery(query);
        EXPECT_FALSE(parsed.isValid());
        ASSERT_FALSE(parsed.errors.empty());
    }
}

TEST_F(ScriptParserTest, TextBoundaryKeywordsRemainLiteralInValuesAndScripts)
{
    Track track{u"/music/first.flac"_s};
    track.setTitle(u"STARTS WITH me and ENDS WITH you"_s);
    const TrackList tracks{track};

    EXPECT_EQ(tracks, m_filter.filter(uR"(title STARTS WITH "STARTS WITH")"_s, tracks));
    EXPECT_EQ(tracks, m_filter.filter(uR"(title ENDS WITH "ENDS WITH you")"_s, tracks));
    EXPECT_EQ(tracks, m_filter.filter(uR"(title HAS "STARTS WITH")"_s, tracks));
    EXPECT_EQ(u"STARTS WITH / ENDS WITH", m_parser.evaluate(u"STARTS WITH / ENDS WITH"_s, track));
}

TEST_F(ScriptParserTest, QueryInOperator)
{
    Track first{u"/music/first.flac"_s};
    first.setGenres({u"Rock"_s});
    first.setPlayCount(1);

    Track second{u"/music/second.flac"_s};
    second.setGenres({u"Jazz"_s});
    second.setPlayCount(2);

    Track third{u"/music/third.flac"_s};
    third.setGenres({u"Blues"_s});
    third.setPlayCount(3);

    const TrackList tracks{first, second, third};

    EXPECT_EQ((TrackList{first, second}), m_filter.filter(uR"(genre IN ("ROCK", "jazz"))"_s, tracks));
    EXPECT_EQ(TrackList{second}, m_filter.filter(u"genre IN (Jazz)"_s, tracks));
    EXPECT_EQ(tracks, m_filter.filter(u"genre IN (Rock, Jazz, Blues)"_s, tracks));
    EXPECT_TRUE(m_filter.filter(uR"(genre IN ("Ro", "Classical"))"_s, tracks).empty());
    EXPECT_EQ(TrackList{first}, m_filter.filter(uR"(genre IN ("Rock", "Rock"))"_s, tracks));
    EXPECT_EQ((TrackList{first, third}), m_filter.filter(u"playcount IN (1, 3)"_s, tracks));
    EXPECT_EQ(TrackList{third}, m_filter.filter(uR"(genre NOT IN ("Rock", "Jazz"))"_s, tracks));
    EXPECT_EQ(TrackList{third}, m_filter.filter(uR"(NOT genre IN ("Rock", "Jazz"))"_s, tracks));
    EXPECT_EQ(TrackList{third}, m_filter.filter(uR"(NOT (genre IN ("Rock", "Jazz")))"_s, tracks));
    EXPECT_EQ(TrackList{second}, m_filter.filter(uR"(genre IN ("Rock", "Jazz") AND playcount > 1)"_s, tracks));
    EXPECT_EQ(tracks, m_filter.filter(uR"(genre IN ("Rock", "Jazz") OR playcount IS 3)"_s, tracks));
    EXPECT_EQ(TrackList{first}, m_filter.filter(uR"(playcount IS 1 AND genre IN ("Rock", "Jazz"))"_s, tracks));
    EXPECT_EQ(TrackList{second}, m_filter.filter(uR"((genre IN ("Rock", "Jazz")) AND playcount IS 2)"_s, tracks));
    EXPECT_EQ(TrackList{second},
              m_filter.filter(uR"(genre IN ("Rock", "Jazz") SORT DESCENDING BY playcount LIMIT 1)"_s, tracks));
}

TEST_F(ScriptParserTest, QueryInExpressionsAndQuotedValues)
{
    Track track{u"/music/first.flac"_s};
    track.setGenres({u"Jazz"_s});
    track.setTitle(u" Rock, Jazz (Live) "_s);
    track.setComment(uR"(say "hello")"_s);
    const TrackList tracks{track};

    EXPECT_EQ(tracks, m_filter.filter(uR"(%genre% IN ($lower("JAZZ"), "Rock"))"_s, tracks));
    EXPECT_EQ(tracks, m_filter.filter(uR"($lower(%genre%) IN ("Rock", "JAZZ"))"_s, tracks));
    EXPECT_EQ(tracks, m_filter.filter(uR"(genre IN (%title%, %genre%))"_s, tracks));
    EXPECT_EQ(tracks, m_filter.filter(uR"(genre IN ($replace("Blues", "Blues", "Jazz")))"_s, tracks));
    EXPECT_EQ(tracks, m_filter.filter(uR"(genre IN (%missing%, "Jazz"))"_s, tracks));
    EXPECT_TRUE(m_filter.filter(uR"(missing IN ("", "Jazz"))"_s, tracks).empty());
    EXPECT_EQ(tracks, m_filter.filter(uR"(missing NOT IN ("", "Jazz"))"_s, tracks));
    EXPECT_EQ(tracks, m_filter.filter(uR"(title IN ("other", " Rock, Jazz (Live) "))"_s, tracks));
    EXPECT_TRUE(m_filter.filter(uR"QUERY(title IN ("Rock, Jazz (Live)"))QUERY"_s, tracks).empty());
    EXPECT_EQ(tracks, m_filter.filter(uR"(comment IN ("say \"hello\""))"_s, tracks));

    track.setTitle(u""_s);
    EXPECT_TRUE(m_parser.parseQuery(uR"(title IN (""))"_s).isValid());
    EXPECT_TRUE(m_filter.filter(uR"(title IN (""))"_s, TrackList{track}).empty());
}

TEST_F(ScriptParserTest, QueryInUsesEqualitySemantics)
{
    Track track{u"/music/first.flac"_s};
    track.setTitle(u"Café"_s);
    track.setGenres({u"Rock"_s, u"Jazz"_s});
    const TrackList tracks{track};

    for(const auto& [field, value] : {std::pair{u"title"_s, u"CAFÉ"_s}, std::pair{u"title"_s, u"Cafe"_s},
                                      std::pair{u"genre"_s, u"Rock"_s}, std::pair{u"genre"_s, u"Rock, Jazz"_s}}) {
        SCOPED_TRACE(value.toStdString());
        EXPECT_EQ(m_filter.filter(uR"(%1 IS "%2")"_s.arg(field, value), tracks),
                  m_filter.filter(uR"(%1 IN ("%2"))"_s.arg(field, value), tracks));
    }
}

TEST_F(ScriptParserTest, QueryInRejectsMalformedLists)
{
    for(const auto& query : {u"genre IN"_s, u"genre IN Rock"_s, u"genre IN ()"_s, u"genre IN (,Rock)"_s,
                             u"genre IN (Rock,)"_s, u"genre IN (Rock,,Jazz)"_s, u"genre IN (Rock"_s,
                             uR"(genre IN ("Rock" "Jazz"))"_s, uR"(genre IN ("Rock))"_s, u"genre NOT IN ()"_s}) {
        SCOPED_TRACE(query.toStdString());
        EXPECT_FALSE(m_parser.parseQuery(query).isValid());
    }
}

TEST_F(ScriptParserTest, QueryHasWordOperator)
{
    Track first{u"/music/first.flac"_s};
    first.setTitle(u"Café (Live)"_s);
    Track second{u"/music/second.flac"_s};
    second.setTitle(u"Alive"_s);
    Track third{u"/music/third.flac"_s};
    third.setTitle(u"Lively"_s);
    const TrackList tracks{first, second, third};

    EXPECT_EQ(TrackList{first}, m_filter.filter(uR"(title HAS WORD "live")"_s, tracks));
    EXPECT_EQ(TrackList{first}, m_filter.filter(uR"(title : WORD "live")"_s, tracks));
    EXPECT_EQ((TrackList{second, third}), m_filter.filter(uR"(title NOT HAS WORD "live")"_s, tracks));
    EXPECT_EQ(TrackList{first}, m_filter.filter(uR"(title HAS WORD "cafe")"_s, tracks));
    EXPECT_EQ(TrackList{first}, m_filter.filter(uR"(ALL HAS WORD "live")"_s, tracks));
    EXPECT_TRUE(m_filter.filter(uR"(ALL HAS WORD "")"_s, tracks).empty());
    EXPECT_EQ(tracks, m_filter.filter(uR"(title HAS "live")"_s, tracks));
}

TEST_F(ScriptParserTest, QueryHasAnyOperator)
{
    Track first{u"/music/first.flac"_s};
    first.setTitle(u"Café Moonlight (Live)"_s);
    first.setPlayCount(1);
    Track second{u"/music/second.flac"_s};
    second.setTitle(u"De\u0301mo sessions"_s);
    second.setPlayCount(2);
    Track third{u"/music/third.flac"_s};
    third.setTitle(u"Studio edit"_s);
    third.setPlayCount(3);
    const TrackList tracks{first, second, third};

    EXPECT_EQ((TrackList{first, second}), m_filter.filter(uR"(title HAS ANY ("cafe", "DEMO"))"_s, tracks));
    EXPECT_EQ((TrackList{first, second}), m_filter.filter(uR"(title : ANY ("cafe", "DEMO"))"_s, tracks));
    EXPECT_EQ(TrackList{first}, m_filter.filter(uR"(title HAS ANY ("light"))"_s, tracks));
    EXPECT_TRUE(m_filter.filter(uR"(title IN ("light"))"_s, tracks).empty());
    EXPECT_TRUE(m_filter.filter(uR"(title HAS ANY ("acoustic", "remix"))"_s, tracks).empty());
    EXPECT_EQ(TrackList{third}, m_filter.filter(uR"(title NOT HAS ANY ("cafe", "demo"))"_s, tracks));
    EXPECT_EQ(TrackList{third}, m_filter.filter(uR"(title NOT : ANY ("cafe", "demo"))"_s, tracks));
    EXPECT_EQ(TrackList{third}, m_filter.filter(uR"(NOT title HAS ANY ("cafe", "demo"))"_s, tracks));
    EXPECT_EQ(TrackList{third}, m_filter.filter(uR"(NOT (title HAS ANY ("cafe", "demo")))"_s, tracks));
    EXPECT_EQ(TrackList{second}, m_filter.filter(uR"(title HAS ANY ("cafe", "demo") AND playcount IS 2)"_s, tracks));
    EXPECT_EQ(tracks, m_filter.filter(uR"(playcount IS 3 OR title HAS ANY ("cafe", "demo"))"_s, tracks));
    EXPECT_EQ(TrackList{first}, m_filter.filter(uR"((title HAS ANY (live, demo)) AND playcount IS 1)"_s, tracks));
    EXPECT_EQ(TrackList{second},
              m_filter.filter(uR"(title HAS ANY ("cafe", "demo") SORT DESCENDING BY playcount LIMIT 1)"_s, tracks));
    EXPECT_EQ(TrackList{first}, m_filter.filter(u"title HAS\tANY\n(\"live\", \"remix\")"_s, tracks));
    EXPECT_EQ(TrackList{first}, m_filter.filter(u"title HAS\n// Alternatives\nANY\n// Values\n(\"live\")"_s, tracks));
}

TEST_F(ScriptParserTest, QueryHasAnyExpressionsAndQuotedValues)
{
    Track track{u"/music/first.flac"_s};
    track.setTitle(u"Rock, Jazz (Live)"_s);
    track.setComment(uR"(say "hello")"_s);
    const TrackList tracks{track};

    EXPECT_EQ(tracks, m_filter.filter(uR"(%title% HAS ANY ("demo", $lower("LIVE")))"_s, tracks));
    EXPECT_EQ(tracks, m_filter.filter(uR"($lower(%title%) HAS ANY ("demo", "LIVE"))"_s, tracks));
    EXPECT_EQ(tracks, m_filter.filter(uR"(title HAS ANY ($replace("demo", "demo", "live")))"_s, tracks));
    EXPECT_EQ(tracks, m_filter.filter(uR"(title HAS ANY (%missing%, "live"))"_s, tracks));
    EXPECT_EQ(tracks, m_filter.filter(uR"(title HAS ANY ("Rock, Jazz", "demo"))"_s, tracks));
    EXPECT_EQ(tracks, m_filter.filter(uR"(comment HAS ANY ("say \"hello\"", "demo"))"_s, tracks));
    EXPECT_EQ(tracks, m_filter.filter(uR"(title HAS ANY (""))"_s, tracks));
    EXPECT_TRUE(m_filter.filter(uR"(missing HAS ANY ("", "live"))"_s, tracks).empty());
    EXPECT_EQ(tracks, m_filter.filter(uR"(missing NOT HAS ANY ("", "live"))"_s, tracks));
}

TEST_F(ScriptParserTest, HasAnyPreservesLiteralAnySearches)
{
    Track first{u"/music/first.flac"_s};
    first.setTitle(u"ANYTHING (Live)"_s);
    Track second{u"/music/second.flac"_s};
    second.setTitle(u"Many words"_s);
    Track third{u"/music/third.flac"_s};
    third.setTitle(u"Studio demo"_s);
    const TrackList tracks{first, second, third};

    for(const auto& query : {u"title HAS ANY"_s, u"title : ANY"_s, uR"(title HAS "ANY")"_s}) {
        SCOPED_TRACE(query.toStdString());
        EXPECT_EQ((TrackList{first, second}), m_filter.filter(query, tracks));
        const auto parsed = m_parser.parseQuery(query);
        ASSERT_TRUE(parsed.isValid());
        ASSERT_EQ(1, parsed.expressions.size());
        EXPECT_EQ(Expr::Contains, parsed.expressions.front().type);
    }
    EXPECT_EQ(TrackList{first}, m_filter.filter(u"title HAS ANYTHING"_s, tracks));
    EXPECT_EQ(TrackList{second}, m_filter.filter(uR"(title HAS ANY AND title HAS "words")"_s, tracks));
    EXPECT_EQ(u"HAS ANY", m_parser.evaluate(u"HAS ANY"_s, first));
}

TEST_F(ScriptParserTest, QueryHasAnyRejectsMalformedLists)
{
    for(const auto& query :
        {u"title HAS ANY ()"_s, u"title HAS ANY (,live)"_s, u"title HAS ANY (live,)"_s, u"title HAS ANY (live,,demo)"_s,
         u"title HAS ANY (live"_s, uR"(title HAS ANY ("live" "demo"))"_s, u"title NOT HAS ANY ()"_s}) {
        SCOPED_TRACE(query.toStdString());
        const auto parsed = m_parser.parseQuery(query);
        EXPECT_FALSE(parsed.isValid());
        ASSERT_FALSE(parsed.errors.empty());
    }
}

TEST_F(ScriptParserTest, QueryHasAnySupportsAllFields)
{
    Track first{u"/music/first.flac"_s};
    first.setTitle(u"Moonlight"_s);
    first.setAlbum(u"Live at home"_s);
    Track second{u"/music/second.flac"_s};
    second.setTitle(u"Café"_s);
    const TrackList tracks{first, second};

    EXPECT_EQ(tracks, m_filter.filter(uR"(ALL HAS ANY ("cafe", "live"))"_s, tracks));
    EXPECT_EQ(m_filter.filter(uR"(ALL HAS "cafe" OR ALL HAS "live")"_s, tracks),
              m_filter.filter(uR"(ALL HAS ANY ("cafe", "live"))"_s, tracks));
    EXPECT_EQ(TrackList{first}, m_filter.filter(uR"(ALL HAS ANY (moon live, "remix"))"_s, tracks));
}

TEST_F(ScriptParserTest, PathQueriesNormaliseSeparators)
{
    const Track canonicalPathTrack{u"C:/Music/First Album/song.flac"_s};
    const Track nativePathTrack{QDir::toNativeSeparators(u"C:/Music/Second Album/song.flac"_s)};
    const TrackList tracks{canonicalPathTrack, nativePathTrack};

    const QString nativeFolder = QDir::toNativeSeparators(u"C:/Music/First Album"_s);
    EXPECT_EQ(1, m_filter.filter(uR"(path HAS "%1")"_s.arg(nativeFolder), tracks).size());
    EXPECT_EQ(1, m_filter.filter(uR"(filepath IS "C:/Music/Second Album/song.flac")"_s, tracks).size());

    QString escapedFolder{nativeFolder};
    escapedFolder.replace(u'\\', uR"(\\)"_s);
    EXPECT_EQ(TrackList{canonicalPathTrack},
              m_filter.filter(uR"(filepath STARTS WITH "%1")"_s.arg(escapedFolder), tracks));
    EXPECT_EQ(tracks, m_filter.filter(uR"(filepath ENDS WITH ".FLAC")"_s, tracks));
    EXPECT_EQ(tracks, m_filter.filter(uR"(filepath HAS ANY ("/Music/", ".FLAC"))"_s, tracks));
}

TEST_F(ScriptParserTest, QueryQuotedLiteralsPreserveWhitespace)
{
    Track track{u"C:/Music/2025 - blossom/song.flac"_s};
    track.setComment(uR"(say " hello + goodbye)"_s);
    const TrackList tracks{track};

    EXPECT_EQ(1, m_filter.filter(uR"(path HAS "C:/Music/2025 - blossom")"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"comment HAS \"say \\\" hello + goodbye\""_s, tracks).size());
}

TEST_F(ScriptParserTest, QueryLongLogicalChains)
{
    const QStringList paths{u"(Variants)"_s, u"(Ban)"_s, u"[Mixed]"_s, u"_broken"_s, u"__DEFAULT"_s,
                            u"Six"_s,        u"Seven"_s, u"Eight"_s,   u"Allowed"_s};
    TrackList tracks;
    for(const auto& path : paths) {
        Track track;
        track.setId(static_cast<int>(tracks.size()));
        track.setFilePath(u"/music/%1/song.flac"_s.arg(path));
        track.setRating(0.8F);
        track.setLastPlayed(QDateTime::currentDateTime().addMonths(-2).toMSecsSinceEpoch());
        tracks.push_back(track);
    }

    QStringList alternatives;
    QStringList exclusions;
    for(int count{1}; count < paths.size(); ++count) {
        SCOPED_TRACE(count);
        alternatives.push_back(u"path : \"%1\""_s.arg(paths.at(count - 1)));
        exclusions.push_back(u"NOT path : \"%1\""_s.arg(paths.at(count - 1)));

        const QString disjunction = alternatives.join(u" OR "_s);
        const QString conjunction = exclusions.join(u" AND "_s);
        const TrackList included{tracks.begin(), tracks.begin() + count};
        const TrackList excluded{tracks.begin() + count, tracks.end()};

        EXPECT_EQ(included, m_filter.filter(disjunction, tracks));
        EXPECT_EQ(included, m_filter.filter(u"(%1)"_s.arg(disjunction), tracks));
        EXPECT_EQ(excluded, m_filter.filter(u"NOT (%1)"_s.arg(disjunction), tracks));
        EXPECT_EQ(excluded, m_filter.filter(conjunction, tracks));
        EXPECT_EQ(excluded, m_filter.filter(u"(%1)"_s.arg(conjunction), tracks));
        EXPECT_EQ(excluded,
                  m_filter.filter(
                      u"rating >= 4 AND NOT lastplayed DURING LAST 1 MONTHS AND NOT (%1)"_s.arg(disjunction), tracks));
    }
}

TEST_F(ScriptParserTest, QueryDateRangesRespectInputPrecision)
{
    Track first;
    first.setFirstPlayed(QDateTime{QDate{2024, 2, 29}, QTime{8, 7, 6}}.toMSecsSinceEpoch());

    Track second;
    second.setFirstPlayed(QDateTime{QDate{2024, 2, 29}, QTime{8, 7, 59}}.toMSecsSinceEpoch());

    Track third;
    third.setFirstPlayed(QDateTime{QDate{2024, 2, 29}, QTime{8, 8}}.toMSecsSinceEpoch());

    const TrackList tracks{first, second, third};

    EXPECT_EQ(3, m_filter.filter(u"firstplayed DURING 2024"_s, tracks).size());
    EXPECT_EQ(3, m_filter.filter(u"firstplayed DURING \"2024-02\""_s, tracks).size());
    EXPECT_EQ(3, m_filter.filter(u"firstplayed DURING \"2024-02-29\""_s, tracks).size());
    EXPECT_EQ(3, m_filter.filter(u"firstplayed DURING \"2024-02-29 08\""_s, tracks).size());
    EXPECT_EQ(2, m_filter.filter(u"firstplayed DURING \"2024-02-29 08:07\""_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"firstplayed DURING \"2024-02-29 08:07:06\""_s, tracks).size());
    EXPECT_EQ(0, m_filter.filter(u"firstplayed DURING \"2023-02-29\""_s, tracks).size());
}

TEST_F(ScriptParserTest, QueryBetweenDateOperator)
{
    const QDateTime start{QDate{2024, 1, 1}, QTime{0, 0}};
    const QDateTime end{QDate{2024, 4, 1}, QTime{0, 0}};

    Track before{u"/music/before.flac"_s};
    before.setFirstPlayed(start.addMSecs(-1).toMSecsSinceEpoch());

    Track first{u"/music/first.flac"_s};
    first.setFirstPlayed(start.toMSecsSinceEpoch());
    first.setPlayCount(2);

    Track last{u"/music/last.flac"_s};
    last.setFirstPlayed(end.addMSecs(-1).toMSecsSinceEpoch());

    Track after{u"/music/after.flac"_s};
    after.setFirstPlayed(end.toMSecsSinceEpoch());

    const TrackList tracks{before, first, last, after};
    const TrackList included{first, last};
    const TrackList excluded{before, after};
    const TrackList duringYear{first, last, after};
    const QString query = uR"(firstplayed BETWEEN "2024-01-01" AND "2024-03-31")"_s;

    EXPECT_EQ(included, m_filter.filter(query, tracks));
    EXPECT_EQ(excluded, m_filter.filter(uR"(firstplayed NOT BETWEEN "2024-01-01" AND "2024-03-31")"_s, tracks));
    EXPECT_EQ(duringYear, m_filter.filter(u"firstplayed BETWEEN 2024 AND 2024"_s, tracks));
    EXPECT_EQ(TrackList{first}, m_filter.filter(query + u" AND playcount > 1"_s, tracks));
    EXPECT_FALSE(m_parser.parseQuery(u"firstplayed BETWEEN 2025 AND 2024"_s).isValid());
    EXPECT_FALSE(m_parser.parseQuery(uR"(firstplayed BETWEEN "2024-02-30" AND "2024-03-31")"_s).isValid());
}

TEST_F(ScriptParserTest, QueryBetweenRejectsMalformedRanges)
{
    for(const auto& query :
        {u"firstplayed BETWEEN 2024"_s, u"firstplayed BETWEEN 2024 OR 2025"_s,
         uR"(firstplayed BETWEEN "2024-01-01" "2024-03-31")"_s, u"firstplayed BETWEEN 2024 AND"_s,
         u"firstplayed BETWEEN 2024 AND invalid"_s, uR"(firstplayed BETWEEN 2024 AND "not a date")"_s,
         uR"(firstplayed BETWEEN 2024 AND "2024-02-30")"_s}) {
        SCOPED_TRACE(query.toStdString());
        const auto parsed = m_parser.parseQuery(query);
        EXPECT_FALSE(parsed.isValid());
        EXPECT_FALSE(parsed.errors.empty());
    }
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

    EXPECT_EQ(1, m_filter.filter(u"gabor"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"\"gabor szabo\""_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"artist:gabor"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"album:mas"_s, tracks).size());
    EXPECT_EQ(1, m_filter.filter(u"title:cafe"_s, tracks).size());
}

TEST_F(ScriptParserTest, QueryLiteralSearchOptions)
{
    Track track;
    track.setId(0);
    track.setTitle(u"Moonlight Sketches"_s);
    track.setAlbum(u"Hidden Album"_s);

    const TrackList tracks{track};

    ScriptSearchOptions titleOnly;
    titleOnly.script = u"%title%"_s;
    titleOnly.mode   = ScriptSearchMode::MatchWordBeginnings;

    TrackQueryFilter filter{titleOnly, &m_registry};
    EXPECT_EQ(1, filter.filter(u"Moon"_s, tracks).size());
    EXPECT_EQ(0, filter.filter(u"light"_s, tracks).size());
    EXPECT_EQ(0, filter.filter(u"Hidden"_s, tracks).size());
    EXPECT_EQ(1, filter.filter(u"album:hidden"_s, tracks).size());
    EXPECT_EQ(1, filter.filter(u"\"Moonlight Sketches\""_s, tracks).size());
    EXPECT_EQ(0, filter.filter(u"\"Moon Sketches\""_s, tracks).size());

    titleOnly.mode = ScriptSearchMode::MatchAnywhere;
    TrackQueryFilter anywhereFilter{titleOnly, &m_registry};
    EXPECT_EQ(1, anywhereFilter.filter(u"light"_s, tracks).size());
}

TEST_F(ScriptParserTest, QueryWordBeginningSearchIgnoresAccents)
{
    Track composedTrack;
    composedTrack.setId(0);
    composedTrack.setArtists({u"Ásgeir"_s});

    Track decomposedTrack;
    decomposedTrack.setId(1);
    decomposedTrack.setArtists({u"A\u0301sgeir"_s});

    ScriptSearchOptions artistOnly;
    artistOnly.script = u"%artist%"_s;
    artistOnly.mode   = ScriptSearchMode::MatchWordBeginnings;

    TrackQueryFilter filter{artistOnly, &m_registry};
    const TrackList tracks{composedTrack, decomposedTrack};

    EXPECT_EQ(2, filter.filter(u"Asgeir"_s, tracks).size());
    EXPECT_EQ(2, filter.filter(u"Ásgeir"_s, tracks).size());
    EXPECT_EQ(0, filter.filter(u"sgeir"_s, tracks).size());
}

TEST_F(ScriptParserTest, QueryClassification)
{
    ScriptParser parser{&m_registry};

    const ParsedScript literal = parser.parseQuery(u"radiohead moon"_s);
    EXPECT_TRUE(ScriptParser::canEvaluateAsQuery(literal));
    EXPECT_FALSE(ScriptParser::containsQueryExpression(literal));

    const ParsedScript fieldQuery = parser.parseQuery(u"artist:radiohead"_s);
    EXPECT_TRUE(ScriptParser::canEvaluateAsQuery(fieldQuery));
    EXPECT_TRUE(ScriptParser::containsQueryExpression(fieldQuery));

    for(const auto& query : {uR"(title STARTS WITH "moon")"_s, uR"(title ENDS WITH "light")"_s,
                             uR"(genre IN ("Rock", "Jazz"))"_s, uR"(genre NOT IN ("Rock", "Jazz"))"_s,
                             uR"(title HAS ANY ("live", "demo"))"_s, uR"(title NOT HAS ANY ("live", "demo"))"_s}) {
        const ParsedScript boundaryQuery = parser.parseQuery(query);
        EXPECT_TRUE(boundaryQuery.isValid());
        EXPECT_TRUE(ScriptParser::canEvaluateAsQuery(boundaryQuery));
        EXPECT_TRUE(ScriptParser::containsQueryExpression(boundaryQuery));
    }

    const ParsedScript invalidQuery = parser.parseQuery(u"$unknown(duration)=210000"_s);
    EXPECT_FALSE(ScriptParser::canEvaluateAsQuery(invalidQuery));
    EXPECT_FALSE(ScriptParser::containsQueryExpression(invalidQuery));
}

TEST_F(ScriptParserTest, QueryAndLiteralCachesStaySeparate)
{
    TrackList tracks;

    Track track;
    track.setPlayCount(2);
    tracks.push_back(track);

    EXPECT_EQ(1, m_filter.filter(u"playcount>1"_s, tracks).size());
    EXPECT_EQ(u"playcount>1", m_parser.evaluate(u"playcount>1"_s, track));
}

TEST(ScriptRegistryTest, AliasCollisionsRejectTheWholeRegistration)
{
    ScriptRegistry registry;
    const auto variable = makeScriptVariableDescriptor<[]() {
        return u"plugin"_s;
    }>(VariableKind::Generic, u"plugin_title"_s, {}, {}, {u"plugin_alias"_s, u"TRACKNUMBER"_s});

    EXPECT_FALSE(registry.registerVariable(variable));
    EXPECT_FALSE(registry.isVariable(u"plugin_title"_s));
    EXPECT_FALSE(registry.isVariable(u"plugin_alias"_s));
    EXPECT_FALSE(registry.registerVariable(makeScriptVariableDescriptor<[]() {
        return u"plugin"_s;
    }>(VariableKind::Generic, u"plugin_title"_s, {}, {}, {u"same"_s, u"SAME"_s})));
    EXPECT_FALSE(registry.registerVariable(makeScriptVariableDescriptor<[]() {
        return u"plugin"_s;
    }>(VariableKind::Generic, u"plugin_title"_s, {}, {}, {QString{}})));

    EXPECT_FALSE(registry.registerFunction(
        makeScriptFunctionDescriptor<PrefixFunction>(u"plugin_prefix"_s, {}, {}, {}, {u"plugin_alias"_s, u"GET"_s})));
    EXPECT_EQ(InvalidScriptFunctionId, registry.resolveFunctionId(u"plugin_prefix"_s));
    EXPECT_EQ(InvalidScriptFunctionId, registry.resolveFunctionId(u"plugin_alias"_s));
    EXPECT_FALSE(registry.registerFunction(
        makeScriptFunctionDescriptor<PrefixFunction>(u"plugin_prefix"_s, {}, {}, {}, {u"same"_s, u"SAME"_s})));
    EXPECT_FALSE(registry.registerFunction(
        makeScriptFunctionDescriptor<PrefixFunction>(u"plugin_prefix"_s, {}, {}, {}, {QString{}})));
}

TEST(ScriptRegistryTest, BindingWaitsForFinalisation)
{
    ScriptRegistry registry;
    ScriptParser parser{&registry};

    const auto parsed = parser.parse(u"%title%"_s);
    Track track;
    track.setTitle(u"Ready"_s);

    EXPECT_TRUE(parser.evaluate(parsed, track).isEmpty());
    registry.finalise();
    EXPECT_EQ(u"Ready", parser.evaluate(parsed, track));
}

TEST(ScriptRegistryTest, RejectsDuplicateReservedAndLateDefinitions)
{
    ScriptRegistry registry;
    const auto resolver = makeScriptVariableInvoker<[]() {
        return u"plugin"_s;
    }>();

    ASSERT_TRUE(registry.registerVariable({.kind = VariableKind::Generic, .name = u"custom"_s, .invoker = resolver}));
    EXPECT_FALSE(registry.registerVariable({.kind = VariableKind::Generic, .name = u"CUSTOM"_s, .invoker = resolver}));
    EXPECT_FALSE(registry.registerVariable({.kind = VariableKind::Generic, .name = u"artist"_s, .invoker = resolver}));
    EXPECT_FALSE(
        registry.registerVariable({.kind = VariableKind::Generic, .name = u"tracknumber"_s, .invoker = resolver}));
    EXPECT_FALSE(registry.registerFunction(makeScriptFunctionDescriptor<PrefixFunction>(u"GET"_s)));
    ASSERT_TRUE(registry.registerFunction(makeScriptFunctionDescriptor<PrefixFunction>(u"custom"_s)));
    EXPECT_FALSE(registry.registerFunction(makeScriptFunctionDescriptor<PrefixFunction>(u"CUSTOM"_s)));
    registry.finalise();
    EXPECT_FALSE(registry.registerVariable({.kind = VariableKind::Generic, .name = u"late"_s, .invoker = resolver}));
    EXPECT_FALSE(registry.registerFunction(makeScriptFunctionDescriptor<PrefixFunction>(u"late"_s)));
    ScriptParser parser{&registry};
    EXPECT_EQ(u"pluginfn:value", parser.evaluate(u"%custom%$custom(value)"_s));
}

TEST(ScriptRegistryTest, ContextValuesPreserveConditionAndListFallback)
{
    ScriptRegistry registry;
    registry.finalise();
    ScriptParser parser{&registry};

    const std::array values{ScriptVariableValue{u"TRACKCOUNT"_s, {.value = u"deferred"_s, .cond = true}},
                            ScriptVariableValue{u"NODE"_s, {.value = QString{}, .cond = true}}};
    const ScriptContext context{.values = values};

    EXPECT_EQ(u"deferred", parser.evaluate(u"%trackcount%"_s, context));
    EXPECT_EQ(u"visible", parser.evaluate(u"[%node%visible]"_s, context));
    EXPECT_EQ(u"%TRACKCOUNT%", parser.evaluate(u"%trackcount%"_s));
    EXPECT_EQ(u"", parser.evaluate(u"%node%"_s));
    EXPECT_EQ(u"0", parser.evaluate(u"%trackcount%"_s, TrackList{}));
}

TEST(ScriptRegistryTest, PlaybackValuesMatchTheEvaluatedRow)
{
    ScriptRegistry registry;
    registry.finalise();
    ScriptParser parser{&registry};
    Track track;

    const ScriptPlaybackData playback{.position = 45000, .playState = Player::PlayState::Playing};
    ScriptPlaylistData row{.trackIndex = 1, .playingTrackIndex = 0};
    const ScriptContext context{.playlistData = &row, .playback = &playback};

    EXPECT_EQ(u"", parser.evaluate(u"%playback_time%"_s, track, context));
    row.trackIndex = 0;
    EXPECT_EQ(u"00:45", parser.evaluate(u"%playback_time%"_s, track, context));
    row.trackIndex = -1;
    EXPECT_EQ(u"00:45", parser.evaluate(u"%playback_time%"_s, track, context));
}
} // namespace Fooyin::Testing
