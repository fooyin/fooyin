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

#pragma once

#include <QString>

namespace Fooyin {
namespace Expr {
enum Type : uint8_t
{
    Null = 0,
    Literal,
    Variable,
    VariableList,
    VariableRaw,
    Date,
    Function,
    FunctionArg,
    Conditional,
    Not,
    Group,
    And,
    Or,
    XOr,
    Equals,
    Contains,
    Greater,
    GreaterEqual,
    Less,
    LessEqual,
    SortAscending,
    SortDescending,
    All,
    QuotedLiteral,
    Missing,
    Present,
    Before,
    After,
    Since,
    During,
    Limit,
};
}

enum class FunctionKind : uint8_t
{
    Generic = 0,
    Get,
    Put,
    Puts,
    If,
    If2,
    If3,
    IfEqual,
    IfGreater,
    IfLonger,
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Num,
    Pad,
    PadRight,
};

enum class VariableKind : uint8_t
{
    Generic = 0,
    Track,
    TrackTotal,
    Disc,
    DiscTotal,
    Title,
    Artist,
    UniqueArtist,
    PlayCount,
    Duration,
    DurationSecs,
    DurationMSecs,
    AlbumArtist,
    Album,
    Genre,
    Genres,
    Composer,
    Performer,
    Comment,
    Date,
    Year,
    FileSize,
    FileSizeNatural,
    Bitrate,
    SampleRate,
    BitDepth,
    FirstPlayed,
    LastPlayed,
    Rating,
    RatingStars,
    RatingEditor,
    Codec,
    CodecProfile,
    Tool,
    TagType,
    Encoding,
    Channels,
    AddedTime,
    LastModified,
    FilePath,
    FileName,
    Extension,
    FileNameWithExt,
    Directory,
    Path,
    Subsong,
    RGTrackGain,
    RGTrackPeak,
    RGTrackPeakDB,
    RGAlbumGain,
    RGAlbumPeak,
    RGAlbumPeakDB,
    TrackCount,
    Playtime,
    PlaylistDuration,
    PlaylistElapsed,
    PlaybackTime,
    PlaybackTimeSeconds,
    PlaybackTimeRemaining,
    PlaybackTimeRemainingSeconds,
    IsPlaying,
    IsPaused,
    LibraryName,
    LibraryPath,
    RelativePath,
    Depth,
    ListIndex,
    QueueIndex,
    QueueIndexes,
    PlayingIcon,
    FrontCover,
    BackCover,
    ArtistPicture,
};

struct Expression;
using ExpressionList = std::vector<Expression>;

struct FuncValue
{
    QString name;
    ExpressionList args;
};

using ExpressionValue = std::variant<QString, FuncValue, ExpressionList>;

struct Expression
{
    Expression() = default;

    Expression(Expr::Type exprType)
        : type{exprType}
    { }

    Expression(Expr::Type exprType, ExpressionValue exprValue)
        : type{exprType}
        , value{std::move(exprValue)}
    { }

    Expr::Type type{Expr::Null};
    ExpressionValue value{QString{}};
};
} // namespace Fooyin
