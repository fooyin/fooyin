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

    if(var == QLatin1StringView{MetaData::TrackNumber} || var == "TRACKNUMBER"_L1 || var == "TRACK NUMBER"_L1) {
        return VariableKind::Track;
    }
    if(var == QLatin1StringView{MetaData::TrackTotal} || var == "TOTALTRACKS"_L1) {
        return VariableKind::TrackTotal;
    }
    if(var == QLatin1StringView{MetaData::Disc} || var == "DISCNUMBER"_L1) {
        return VariableKind::Disc;
    }
    if(var == QLatin1StringView{MetaData::DiscTotal} || var == "TOTALDISCS"_L1) {
        return VariableKind::DiscTotal;
    }
    if(var == QLatin1StringView{MetaData::Title}) {
        return VariableKind::Title;
    }
    if(var == QLatin1StringView{MetaData::Artist}) {
        return VariableKind::Artist;
    }
    if(var == QLatin1StringView{MetaData::UniqueArtist}) {
        return VariableKind::UniqueArtist;
    }
    if(var == QLatin1StringView{MetaData::Album}) {
        return VariableKind::Album;
    }
    if(var == QLatin1StringView{MetaData::AlbumArtist} || var == "ALBUM ARTIST"_L1) {
        return VariableKind::AlbumArtist;
    }
    if(var == QLatin1StringView{MetaData::Genre}) {
        return VariableKind::Genre;
    }
    if(var == "GENRES"_L1) {
        return VariableKind::Genres;
    }
    if(var == QLatin1StringView{MetaData::Composer}) {
        return VariableKind::Composer;
    }
    if(var == QLatin1StringView{MetaData::Performer}) {
        return VariableKind::Performer;
    }
    if(var == QLatin1StringView{MetaData::PlayCount} || var == "PLAY_COUNT"_L1) {
        return VariableKind::PlayCount;
    }
    if(var == QLatin1StringView{MetaData::Duration} || var == "LENGTH"_L1) {
        return VariableKind::Duration;
    }
    if(var == QLatin1StringView{MetaData::DurationSecs} || var == "LENGTH_SECONDS"_L1) {
        return VariableKind::DurationSecs;
    }
    if(var == QLatin1StringView{MetaData::DurationMSecs}) {
        return VariableKind::DurationMSecs;
    }
    if(var == QLatin1StringView{MetaData::Comment}) {
        return VariableKind::Comment;
    }
    if(var == QLatin1StringView{MetaData::Date}) {
        return VariableKind::Date;
    }
    if(var == QLatin1StringView{MetaData::Year}) {
        return VariableKind::Year;
    }
    if(var == QLatin1StringView{MetaData::FileSize}) {
        return VariableKind::FileSize;
    }
    if(var == QLatin1StringView{MetaData::FileSizeNatural}) {
        return VariableKind::FileSizeNatural;
    }
    if(var == QLatin1StringView{MetaData::Bitrate}) {
        return VariableKind::Bitrate;
    }
    if(var == QLatin1StringView{MetaData::SampleRate}) {
        return VariableKind::SampleRate;
    }
    if(var == QLatin1StringView{MetaData::BitDepth}) {
        return VariableKind::BitDepth;
    }
    if(var == QLatin1StringView{MetaData::FirstPlayed}) {
        return VariableKind::FirstPlayed;
    }
    if(var == QLatin1StringView{MetaData::LastPlayed}) {
        return VariableKind::LastPlayed;
    }
    if(var == QLatin1StringView{MetaData::Rating}) {
        return VariableKind::Rating;
    }
    if(var == QLatin1StringView{MetaData::RatingNormalized}) {
        return VariableKind::RatingNormalized;
    }
    if(var == QLatin1StringView{MetaData::Stars}) {
        return VariableKind::Stars;
    }
    if(var == QLatin1StringView{MetaData::RatingStars}) {
        return VariableKind::RatingStars;
    }
    if(var == QLatin1StringView{MetaData::RatingStarsPadded}) {
        return VariableKind::RatingStarsPadded;
    }
    if(var == QLatin1StringView{MetaData::RatingEditor}) {
        return VariableKind::RatingEditor;
    }
    if(var == QLatin1StringView{MetaData::Codec}) {
        return VariableKind::Codec;
    }
    if(var == QLatin1StringView{MetaData::CodecProfile}) {
        return VariableKind::CodecProfile;
    }
    if(var == QLatin1StringView{MetaData::Tool}) {
        return VariableKind::Tool;
    }
    if(var == QLatin1StringView{MetaData::TagType}) {
        return VariableKind::TagType;
    }
    if(var == QLatin1StringView{MetaData::Encoding}) {
        return VariableKind::Encoding;
    }
    if(var == QLatin1StringView{MetaData::Channels}) {
        return VariableKind::Channels;
    }
    if(var == QLatin1StringView{MetaData::CreatedTime}) {
        return VariableKind::CreatedTime;
    }
    if(var == QLatin1StringView{MetaData::AddedTime}) {
        return VariableKind::AddedTime;
    }
    if(var == QLatin1StringView{MetaData::LastModified}) {
        return VariableKind::LastModified;
    }
    if(var == QLatin1StringView{MetaData::FilePath}) {
        return VariableKind::FilePath;
    }
    if(var == QLatin1StringView{MetaData::FileName}) {
        return VariableKind::FileName;
    }
    if(var == QLatin1StringView{MetaData::Extension}) {
        return VariableKind::Extension;
    }
    if(var == QLatin1StringView{MetaData::FileNameWithExt}) {
        return VariableKind::FileNameWithExt;
    }
    if(var == QLatin1StringView{MetaData::Directory}) {
        return VariableKind::Directory;
    }
    if(var == QLatin1StringView{MetaData::Path}) {
        return VariableKind::Path;
    }
    if(var == QLatin1StringView{MetaData::Subsong}) {
        return VariableKind::Subsong;
    }
    if(var == QLatin1StringView{MetaData::RGTrackGain}) {
        return VariableKind::RGTrackGain;
    }
    if(var == QLatin1StringView{MetaData::RGTrackPeak}) {
        return VariableKind::RGTrackPeak;
    }
    if(var == QLatin1StringView{MetaData::RGTrackPeakDB}) {
        return VariableKind::RGTrackPeakDB;
    }
    if(var == QLatin1StringView{MetaData::RGAlbumGain}) {
        return VariableKind::RGAlbumGain;
    }
    if(var == QLatin1StringView{MetaData::RGAlbumPeak}) {
        return VariableKind::RGAlbumPeak;
    }
    if(var == QLatin1StringView{MetaData::RGAlbumPeakDB}) {
        return VariableKind::RGAlbumPeakDB;
    }
    if(var == "TRACKCOUNT"_L1) {
        return VariableKind::TrackCount;
    }
    if(var == "PLAYTIME"_L1) {
        return VariableKind::Playtime;
    }
    if(var == "PLAYLIST_SIZE"_L1) {
        return VariableKind::PlaylistSize;
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
    if(var == "PLAYBACK_TIME_S"_L1 || var == "PLAYBACK_TIME_SECONDS"_L1) {
        return VariableKind::PlaybackTimeSeconds;
    }
    if(var == "PLAYBACK_TIME_REMAINING"_L1) {
        return VariableKind::PlaybackTimeRemaining;
    }
    if(var == "PLAYBACK_TIME_REMAINING_S"_L1 || var == "PLAYBACK_TIME_REMAINING_SECONDS"_L1) {
        return VariableKind::PlaybackTimeRemainingSeconds;
    }
    if(var == "ISPLAYING"_L1) {
        return VariableKind::IsPlaying;
    }
    if(var == "ISPAUSED"_L1) {
        return VariableKind::IsPaused;
    }
    if(var == "ISSTOPPED"_L1) {
        return VariableKind::IsStopped;
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
    if(name == "select"_L1) {
        return Kind::Select;
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
