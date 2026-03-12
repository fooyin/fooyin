/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <LukeT1@proton.me>
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

#include "scriptbinder.h"

#include "scriptregistry.h"

#include <core/constants.h>

using namespace Qt::StringLiterals;

namespace {
Fooyin::BoundExpression bindExpression(const Fooyin::Expression& expr, const Fooyin::ScriptRegistry* registry)
{
    Fooyin::BoundExpression bound;
    bound.type = expr.type;

    if(const auto* value = std::get_if<QString>(&expr.value)) {
        bound.value = *value;

        if(expr.type == Fooyin::Expr::Variable || expr.type == Fooyin::Expr::VariableList) {
            bound.variableKind = registry ? registry->resolveVariable(*value) : Fooyin::VariableKind::Generic;
        }

        return bound;
    }

    if(const auto* func = std::get_if<Fooyin::FuncValue>(&expr.value)) {
        Fooyin::BoundFunctionValue boundFunc;
        boundFunc.name       = func->name;
        boundFunc.kind       = Fooyin::resolveBuiltInFunctionKind(func->name);
        boundFunc.functionId = (boundFunc.kind == Fooyin::FunctionKind::Generic && registry)
                                 ? registry->resolveFunctionId(func->name)
                                 : Fooyin::InvalidScriptFunctionId;
        boundFunc.args.reserve(func->args.size());

        for(const auto& arg : func->args) {
            boundFunc.args.emplace_back(bindExpression(arg, registry));
        }

        bound.value = std::move(boundFunc);
        return bound;
    }

    const auto& exprArgs = std::get<Fooyin::ExpressionList>(expr.value);

    Fooyin::BoundExpressionList args;
    args.reserve(exprArgs.size());

    for(const auto& arg : exprArgs) {
        args.emplace_back(bindExpression(arg, registry));
    }

    bound.value = std::move(args);
    return bound;
}
} // namespace

namespace Fooyin {
VariableKind resolveBuiltInVariableKind(const QString& var)
{
    using namespace Fooyin::Constants;

    if(var == QLatin1String{MetaData::Track}) {
        return VariableKind::Track;
    }
    if(var == QLatin1String{MetaData::TrackTotal}) {
        return VariableKind::TrackTotal;
    }
    if(var == QLatin1String{MetaData::Disc}) {
        return VariableKind::Disc;
    }
    if(var == QLatin1String{MetaData::DiscTotal}) {
        return VariableKind::DiscTotal;
    }
    if(var == QLatin1String{MetaData::Title}) {
        return VariableKind::Title;
    }
    if(var == QLatin1String{MetaData::Artist}) {
        return VariableKind::Artist;
    }
    if(var == QLatin1String{MetaData::UniqueArtist}) {
        return VariableKind::UniqueArtist;
    }
    if(var == QLatin1String{MetaData::Album}) {
        return VariableKind::Album;
    }
    if(var == QLatin1String{MetaData::AlbumArtist}) {
        return VariableKind::AlbumArtist;
    }
    if(var == QLatin1String{MetaData::Genre}) {
        return VariableKind::Genre;
    }
    if(var == "GENRES"_L1) {
        return VariableKind::Genres;
    }
    if(var == QLatin1String{MetaData::Composer}) {
        return VariableKind::Composer;
    }
    if(var == QLatin1String{MetaData::Performer}) {
        return VariableKind::Performer;
    }
    if(var == QLatin1String{MetaData::PlayCount}) {
        return VariableKind::PlayCount;
    }
    if(var == QLatin1String{MetaData::Duration}) {
        return VariableKind::Duration;
    }
    if(var == QLatin1String{MetaData::DurationSecs}) {
        return VariableKind::DurationSecs;
    }
    if(var == QLatin1String{MetaData::DurationMSecs}) {
        return VariableKind::DurationMSecs;
    }
    if(var == QLatin1String{MetaData::Comment}) {
        return VariableKind::Comment;
    }
    if(var == QLatin1String{MetaData::Date}) {
        return VariableKind::Date;
    }
    if(var == QLatin1String{MetaData::Year}) {
        return VariableKind::Year;
    }
    if(var == QLatin1String{MetaData::FileSize}) {
        return VariableKind::FileSize;
    }
    if(var == QLatin1String{MetaData::FileSizeNatural}) {
        return VariableKind::FileSizeNatural;
    }
    if(var == QLatin1String{MetaData::Bitrate}) {
        return VariableKind::Bitrate;
    }
    if(var == QLatin1String{MetaData::SampleRate}) {
        return VariableKind::SampleRate;
    }
    if(var == QLatin1String{MetaData::BitDepth}) {
        return VariableKind::BitDepth;
    }
    if(var == QLatin1String{MetaData::FirstPlayed}) {
        return VariableKind::FirstPlayed;
    }
    if(var == QLatin1String{MetaData::LastPlayed}) {
        return VariableKind::LastPlayed;
    }
    if(var == QLatin1String{MetaData::Rating}) {
        return VariableKind::Rating;
    }
    if(var == QLatin1String{MetaData::RatingStars}) {
        return VariableKind::RatingStars;
    }
    if(var == QLatin1String{MetaData::RatingEditor}) {
        return VariableKind::RatingEditor;
    }
    if(var == QLatin1String{MetaData::Codec}) {
        return VariableKind::Codec;
    }
    if(var == QLatin1String{MetaData::CodecProfile}) {
        return VariableKind::CodecProfile;
    }
    if(var == QLatin1String{MetaData::Tool}) {
        return VariableKind::Tool;
    }
    if(var == QLatin1String{MetaData::TagType}) {
        return VariableKind::TagType;
    }
    if(var == QLatin1String{MetaData::Encoding}) {
        return VariableKind::Encoding;
    }
    if(var == QLatin1String{MetaData::Channels}) {
        return VariableKind::Channels;
    }
    if(var == QLatin1String{MetaData::AddedTime}) {
        return VariableKind::AddedTime;
    }
    if(var == QLatin1String{MetaData::LastModified}) {
        return VariableKind::LastModified;
    }
    if(var == QLatin1String{MetaData::FilePath}) {
        return VariableKind::FilePath;
    }
    if(var == QLatin1String{MetaData::FileName}) {
        return VariableKind::FileName;
    }
    if(var == QLatin1String{MetaData::Extension}) {
        return VariableKind::Extension;
    }
    if(var == QLatin1String{MetaData::FileNameWithExt}) {
        return VariableKind::FileNameWithExt;
    }
    if(var == QLatin1String{MetaData::Directory}) {
        return VariableKind::Directory;
    }
    if(var == QLatin1String{MetaData::Path}) {
        return VariableKind::Path;
    }
    if(var == QLatin1String{MetaData::Subsong}) {
        return VariableKind::Subsong;
    }
    if(var == QLatin1String{MetaData::RGTrackGain}) {
        return VariableKind::RGTrackGain;
    }
    if(var == QLatin1String{MetaData::RGTrackPeak}) {
        return VariableKind::RGTrackPeak;
    }
    if(var == QLatin1String{MetaData::RGTrackPeakDB}) {
        return VariableKind::RGTrackPeakDB;
    }
    if(var == QLatin1String{MetaData::RGAlbumGain}) {
        return VariableKind::RGAlbumGain;
    }
    if(var == QLatin1String{MetaData::RGAlbumPeak}) {
        return VariableKind::RGAlbumPeak;
    }
    if(var == QLatin1String{MetaData::RGAlbumPeakDB}) {
        return VariableKind::RGAlbumPeakDB;
    }
    if(var == "TRACKCOUNT"_L1) {
        return VariableKind::TrackCount;
    }
    if(var == "PLAYTIME"_L1) {
        return VariableKind::Playtime;
    }
    if(var == "PLAYLIST_DURATION"_L1) {
        return VariableKind::PlaylistDuration;
    }
    if(var == "PLAYLIST_ELAPSED"_L1) {
        return VariableKind::PlaylistElapsed;
    }
    if(var == "PLAYBACK_TIME"_L1) {
        return VariableKind::PlaybackTime;
    }
    if(var == "PLAYBACK_TIME_S"_L1) {
        return VariableKind::PlaybackTimeSeconds;
    }
    if(var == "PLAYBACK_TIME_REMAINING"_L1) {
        return VariableKind::PlaybackTimeRemaining;
    }
    if(var == "PLAYBACK_TIME_REMAINING_S"_L1) {
        return VariableKind::PlaybackTimeRemainingSeconds;
    }
    if(var == "ISPLAYING"_L1) {
        return VariableKind::IsPlaying;
    }
    if(var == "ISPAUSED"_L1) {
        return VariableKind::IsPaused;
    }
    if(var == "LIBRARYNAME"_L1) {
        return VariableKind::LibraryName;
    }
    if(var == "LIBRARYPATH"_L1) {
        return VariableKind::LibraryPath;
    }
    if(var == "RELATIVEPATH"_L1) {
        return VariableKind::RelativePath;
    }

    return VariableKind::Generic;
}

FunctionKind resolveBuiltInFunctionKind(const QString& name)
{
    using Kind = FunctionKind;

    if(name == "if"_L1) {
        return Kind::If;
    }
    if(name == "get"_L1) {
        return Kind::Get;
    }
    if(name == "put"_L1) {
        return Kind::Put;
    }
    if(name == "puts"_L1) {
        return Kind::Puts;
    }
    if(name == "if2"_L1) {
        return Kind::If2;
    }
    if(name == "if3"_L1) {
        return Kind::If3;
    }
    if(name == "ifequal"_L1) {
        return Kind::IfEqual;
    }
    if(name == "ifgreater"_L1) {
        return Kind::IfGreater;
    }
    if(name == "iflonger"_L1) {
        return Kind::IfLonger;
    }
    if(name == "add"_L1) {
        return Kind::Add;
    }
    if(name == "sub"_L1) {
        return Kind::Sub;
    }
    if(name == "mul"_L1) {
        return Kind::Mul;
    }
    if(name == "div"_L1) {
        return Kind::Div;
    }
    if(name == "mod"_L1) {
        return Kind::Mod;
    }
    if(name == "num"_L1) {
        return Kind::Num;
    }
    if(name == "pad"_L1) {
        return Kind::Pad;
    }
    if(name == "padright"_L1) {
        return Kind::PadRight;
    }

    return Kind::Generic;
}

BoundScript bindScript(const ParsedScript& script, const ScriptRegistry* registry)
{
    BoundScript bound;
    bound.input  = script.input;
    bound.errors = script.errors;
    bound.expressions.reserve(script.expressions.size());

    for(const auto& expr : script.expressions) {
        bound.expressions.emplace_back(bindExpression(expr, registry));
    }

    return bound;
}
} // namespace Fooyin
