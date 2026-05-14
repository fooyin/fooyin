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

#include "scriptreferenceentries.h"

#include <core/constants.h>
#include <gui/scripting/scriptcommandhandler.h>

#include <QCoreApplication>

using namespace Qt::StringLiterals;

namespace {
QString translate(const char* sourceText)
{
    return QCoreApplication::translate("Fooyin", sourceText);
}

QString variableLabel(const QString& name)
{
    return u"%%1%"_s.arg(name.toLower());
}

Fooyin::ScriptReferenceEntry variableEntry(const QString& name, const char* category, const char* description)
{
    const QString label = variableLabel(name);

    return {.kind         = Fooyin::ScriptReferenceKind::Variable,
            .label        = label,
            .insertText   = label,
            .category     = translate(category),
            .description  = translate(description),
            .cursorOffset = 0};
}

Fooyin::ScriptReferenceEntry variableEntry(const char* name, const char* category, const char* description)
{
    return variableEntry(QString::fromLatin1(name), category, description);
}

Fooyin::ScriptReferenceEntry functionEntry(const char* name, QString signature, const char* description,
                                           const char* category = QT_TRANSLATE_NOOP("Fooyin", "Functions"))
{
    const QString functionName = QString::fromLatin1(name);

    return {.kind         = Fooyin::ScriptReferenceKind::Function,
            .label        = std::move(signature),
            .insertText   = u"$%1()"_s.arg(functionName),
            .category     = translate(category),
            .description  = translate(description),
            .cursorOffset = 1};
}

Fooyin::ScriptReferenceEntry formattingEntry(const char* name, const char* signature, const char* category,
                                             const char* description, int cursorOffset)
{
    const QString tagName = QString::fromLatin1(name);
    const QString sig     = QString::fromLatin1(signature);

    return {.kind         = Fooyin::ScriptReferenceKind::Formatting,
            .label        = sig,
            .insertText   = u"%1</%2>"_s.arg(sig, tagName),
            .category     = translate(category),
            .description  = translate(description),
            .cursorOffset = cursorOffset};
}

Fooyin::ScriptReferenceEntry commandAliasEntry(const Fooyin::ScriptCommandAlias& alias)
{
    return {
        .kind         = Fooyin::ScriptReferenceKind::CommandAlias,
        .label        = alias.alias.toString(),
        .insertText   = alias.alias.toString(),
        .category     = translate(alias.category),
        .description  = translate(alias.description),
        .cursorOffset = 0,
    };
}
} // namespace

namespace Fooyin {
const std::vector<ScriptReferenceEntry>& scriptReferenceEntries()
{
    using namespace Fooyin::Constants;

    static const std::vector Entries = {
        variableEntry(MetaData::Title, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Track title")),
        variableEntry(MetaData::Artist, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Primary artist")),
        variableEntry(MetaData::UniqueArtist, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Unique artists combined into one value")),
        variableEntry(MetaData::Album, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Album title")),
        variableEntry(MetaData::AlbumArtist, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Album artist")),
        variableEntry(MetaData::TrackNumber, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Track number")),
        variableEntry(MetaData::TrackTotal, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Total tracks on the release")),
        variableEntry(MetaData::Disc, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Disc number")),
        variableEntry(MetaData::DiscTotal, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Total discs on the release")),
        variableEntry(MetaData::Genre, QT_TRANSLATE_NOOP("Fooyin", "Metadata"), QT_TRANSLATE_NOOP("Fooyin", "Genres")),
        variableEntry(MetaData::Composer, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Composers")),
        variableEntry(MetaData::Performer, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Performers")),
        variableEntry(MetaData::Duration, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Track duration formatted as time")),
        variableEntry(MetaData::DurationSecs, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Track duration in seconds")),
        variableEntry(MetaData::DurationMSecs, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Track duration in milliseconds")),
        variableEntry(MetaData::Comment, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Comment tag")),
        variableEntry(MetaData::Date, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Release date")),
        variableEntry(MetaData::Year, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Release year")),
        variableEntry(MetaData::FileSize, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "File size in bytes")),
        variableEntry(MetaData::FileSizeNatural, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Human readable file size")),
        variableEntry(MetaData::Bitrate, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Track bitrate")),
        variableEntry(MetaData::SampleRate, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Sample rate")),
        variableEntry(MetaData::BitDepth, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Bit depth")),
        variableEntry(MetaData::FirstPlayed, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "First played timestamp")),
        variableEntry(MetaData::LastPlayed, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Last played timestamp")),
        variableEntry(MetaData::PlayCount, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Play count")),
        variableEntry(MetaData::Rating, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Numeric rating in stars")),
        variableEntry(MetaData::RatingNormalized, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Normalized rating")),
        variableEntry(MetaData::Stars, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Numeric rating in stars")),
        variableEntry(MetaData::RatingStars, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Rating shown as stars")),
        variableEntry(MetaData::RatingStarsPadded, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Rating shown as stars with trailing empty stars")),
        variableEntry(MetaData::RatingEditor, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Rating editor representation")),
        variableEntry(MetaData::Codec, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Codec name")),
        variableEntry(MetaData::CodecProfile, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Codec profile")),
        variableEntry(MetaData::Tool, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Encoding tool")),
        variableEntry(MetaData::TagType, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Tag type list")),
        variableEntry(MetaData::Encoding, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Encoding description")),
        variableEntry(MetaData::Channels, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Channel layout")),
        variableEntry(MetaData::CreatedTime, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "File created timestamp")),
        variableEntry(MetaData::AddedTime, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Library added timestamp")),
        variableEntry(MetaData::LastModified, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Last modified timestamp")),
        variableEntry(MetaData::FilePath, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Full file path")),
        variableEntry(MetaData::RelativePath, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Path relative to the library root")),
        variableEntry(MetaData::FileName, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Filename without extension")),
        variableEntry(MetaData::Extension, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "File extension")),
        variableEntry(MetaData::FileNameWithExt, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Filename including extension")),
        variableEntry(MetaData::Directory, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Containing directory name")),
        variableEntry(MetaData::Path, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Containing directory path")),
        variableEntry(MetaData::Subsong, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "Subsong index")),
        variableEntry(MetaData::RGTrackGain, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "ReplayGain track gain")),
        variableEntry(MetaData::RGTrackPeak, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "ReplayGain track peak")),
        variableEntry(MetaData::RGTrackPeakDB, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "ReplayGain track peak in dB")),
        variableEntry(MetaData::RGAlbumGain, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "ReplayGain album gain")),
        variableEntry(MetaData::RGAlbumPeak, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "ReplayGain album peak")),
        variableEntry(MetaData::RGAlbumPeakDB, QT_TRANSLATE_NOOP("Fooyin", "Metadata"),
                      QT_TRANSLATE_NOOP("Fooyin", "ReplayGain album peak in dB")),
        variableEntry("trackcount", QT_TRANSLATE_NOOP("Fooyin", "Playlist"),
                      QT_TRANSLATE_NOOP("Fooyin", "Number of tracks in the list")),
        variableEntry("playtime", QT_TRANSLATE_NOOP("Fooyin", "Playlist"),
                      QT_TRANSLATE_NOOP("Fooyin", "Combined duration of the track list")),
        variableEntry("playlist_size", QT_TRANSLATE_NOOP("Fooyin", "Playlist"),
                      QT_TRANSLATE_NOOP("Fooyin", "Combined file size of the track list")),
        variableEntry("playlist_duration", QT_TRANSLATE_NOOP("Fooyin", "Playlist"),
                      QT_TRANSLATE_NOOP("Fooyin", "Alias for total playlist duration")),
        variableEntry("playlist_elapsed", QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                      QT_TRANSLATE_NOOP("Fooyin", "Elapsed time within the active playlist")),
        variableEntry("genres", QT_TRANSLATE_NOOP("Fooyin", "Playlist"),
                      QT_TRANSLATE_NOOP("Fooyin", "Unique genres across the track list")),
        variableEntry("queue_index", QT_TRANSLATE_NOOP("Fooyin", "Queue"),
                      QT_TRANSLATE_NOOP("Fooyin", "First playback queue index for the specified item")),
        variableEntry("queue_indexes", QT_TRANSLATE_NOOP("Fooyin", "Queue"),
                      QT_TRANSLATE_NOOP("Fooyin", "Playback queue indexes for the specified item")),
        variableEntry("queue_total", QT_TRANSLATE_NOOP("Fooyin", "Queue"),
                      QT_TRANSLATE_NOOP("Fooyin", "Total amount of tracks in the playback queue for queued items")),
        variableEntry("playback_time", QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                      QT_TRANSLATE_NOOP("Fooyin", "Current playback position formatted as time")),
        variableEntry("playback_time_s", QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                      QT_TRANSLATE_NOOP("Fooyin", "Current playback position in seconds")),
        variableEntry("playback_time_remaining", QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                      QT_TRANSLATE_NOOP("Fooyin", "Remaining playback time formatted as time")),
        variableEntry("playback_time_remaining_s", QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                      QT_TRANSLATE_NOOP("Fooyin", "Remaining playback time in seconds")),
        variableEntry("isplaying", QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                      QT_TRANSLATE_NOOP("Fooyin", "Returns 1 while playback is active")),
        variableEntry("ispaused", QT_TRANSLATE_NOOP("Fooyin", "Playback"),
                      QT_TRANSLATE_NOOP("Fooyin", "Returns 1 while playback is paused")),
        variableEntry("libraryname", QT_TRANSLATE_NOOP("Fooyin", "Library"),
                      QT_TRANSLATE_NOOP("Fooyin", "Current library name")),
        variableEntry("librarypath", QT_TRANSLATE_NOOP("Fooyin", "Library"),
                      QT_TRANSLATE_NOOP("Fooyin", "Current library path")),
        formattingEntry("b", "<b>", QT_TRANSLATE_NOOP("Fooyin", "Style"),
                        QT_TRANSLATE_NOOP("Fooyin", "Makes the enclosed text bold"), 4),
        formattingEntry("i", "<i>", QT_TRANSLATE_NOOP("Fooyin", "Style"),
                        QT_TRANSLATE_NOOP("Fooyin", "Makes the enclosed text italic"), 4),
        formattingEntry("font", "<font=sans>", QT_TRANSLATE_NOOP("Fooyin", "Style"),
                        QT_TRANSLATE_NOOP("Fooyin", "Sets the font family for the enclosed text"), 7),
        formattingEntry("size", "<size=12>", QT_TRANSLATE_NOOP("Fooyin", "Style"),
                        QT_TRANSLATE_NOOP("Fooyin", "Sets the font size in points"), 7),
        formattingEntry("sized", "<sized=2>", QT_TRANSLATE_NOOP("Fooyin", "Style"),
                        QT_TRANSLATE_NOOP("Fooyin", "Adjusts the current font size by a positive or negative delta"),
                        8),
        formattingEntry("alpha", "<alpha=180>", QT_TRANSLATE_NOOP("Fooyin", "Colour"),
                        QT_TRANSLATE_NOOP("Fooyin", "Sets the text alpha channel from 0 to 255"), 8),
        formattingEntry("rgb", "<rgb=255,0,0>", QT_TRANSLATE_NOOP("Fooyin", "Colour"),
                        QT_TRANSLATE_NOOP("Fooyin", "Sets the text colour from red, green and blue components"), 6),
        formattingEntry("rgba", "<rgba=255,0,0,255>", QT_TRANSLATE_NOOP("Fooyin", "Colour"),
                        QT_TRANSLATE_NOOP("Fooyin", "Sets the text colour from red, green, blue, and alpha components"),
                        7),
        formattingEntry("color", "<color=red>", QT_TRANSLATE_NOOP("Fooyin", "Colour"),
                        QT_TRANSLATE_NOOP("Fooyin", "Sets the text colour from a named colour or hex code"), 8),
        functionEntry("add", u"$add(x,y,…)"_s, QT_TRANSLATE_NOOP("Fooyin", "Adds numeric arguments"),
                      QT_TRANSLATE_NOOP("Fooyin", "Numeric")),
        functionEntry("sub", u"$sub(x,y,…)"_s, QT_TRANSLATE_NOOP("Fooyin", "Subtracts later values from the first"),
                      QT_TRANSLATE_NOOP("Fooyin", "Numeric")),
        functionEntry("mul", u"$mul(x,y,…)"_s, QT_TRANSLATE_NOOP("Fooyin", "Multiplies numeric arguments"),
                      QT_TRANSLATE_NOOP("Fooyin", "Numeric")),
        functionEntry("div", u"$div(x,y)"_s, QT_TRANSLATE_NOOP("Fooyin", "Divides the first value by the second"),
                      QT_TRANSLATE_NOOP("Fooyin", "Numeric")),
        functionEntry("min", u"$min(x,y,…)"_s, QT_TRANSLATE_NOOP("Fooyin", "Returns the smallest numeric value"),
                      QT_TRANSLATE_NOOP("Fooyin", "Numeric")),
        functionEntry("max", u"$max(x,y,…)"_s, QT_TRANSLATE_NOOP("Fooyin", "Returns the largest numeric value"),
                      QT_TRANSLATE_NOOP("Fooyin", "Numeric")),
        functionEntry("mod", u"$mod(x,y)"_s, QT_TRANSLATE_NOOP("Fooyin", "Returns the remainder of a division"),
                      QT_TRANSLATE_NOOP("Fooyin", "Numeric")),
        functionEntry("rand", u"$rand(min,max)"_s, QT_TRANSLATE_NOOP("Fooyin", "Returns a random number in range"),
                      QT_TRANSLATE_NOOP("Fooyin", "Numeric")),
        functionEntry("round", u"$round(value)"_s, QT_TRANSLATE_NOOP("Fooyin", "Rounds a numeric value"),
                      QT_TRANSLATE_NOOP("Fooyin", "Numeric")),
        functionEntry("num", u"$num(value,length)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Formats a number with leading zeroes"),
                      QT_TRANSLATE_NOOP("Fooyin", "Numeric")),
        functionEntry("replace", u"$replace(text,from,to,…)"_s, QT_TRANSLATE_NOOP("Fooyin", "Replaces text fragments"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("ascii", u"$ascii(text)"_s, QT_TRANSLATE_NOOP("Fooyin", "Converts text to ASCII"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("slice", u"$slice(text,start,end)"_s, QT_TRANSLATE_NOOP("Fooyin", "Returns a slice of text"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("chop", u"$chop(text,count)"_s, QT_TRANSLATE_NOOP("Fooyin", "Removes characters from the end"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("left", u"$left(text,count)"_s, QT_TRANSLATE_NOOP("Fooyin", "Returns characters from the left"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("right", u"$right(text,count)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Returns characters from the right"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("insert", u"$insert(text,insert,pos)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Inserts text at a position"), QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("substr", u"$substr(text,start,end)"_s, QT_TRANSLATE_NOOP("Fooyin", "Returns a substring"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("strstr", u"$strstr(text,needle)"_s, QT_TRANSLATE_NOOP("Fooyin", "Finds a substring position"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("stristr", u"$stristr(text,needle)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Finds a substring position ignoring case"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("strstrlast", u"$strstrlast(text,needle)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Finds the last substring position"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("stristrlast", u"$stristrlast(text,needle)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Finds the last substring position ignoring case"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("split", u"$split(text,sep,index)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Returns one split segment (1-based index)"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("len", u"$len(text)"_s, QT_TRANSLATE_NOOP("Fooyin", "Returns the text length"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("longest", u"$longest(a,b,…)"_s, QT_TRANSLATE_NOOP("Fooyin", "Returns the longest string"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("strcmp", u"$strcmp(a,b)"_s, QT_TRANSLATE_NOOP("Fooyin", "Compares two strings"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("stricmp", u"$stricmp(a,b)"_s, QT_TRANSLATE_NOOP("Fooyin", "Compares two strings ignoring case"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("longer", u"$longer(a,b)"_s, QT_TRANSLATE_NOOP("Fooyin", "Tests whether one string is longer"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("sep", u"$sep()"_s, QT_TRANSLATE_NOOP("Fooyin", "Returns the unit separator character"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("crlf", u"$crlf()"_s, QT_TRANSLATE_NOOP("Fooyin", "Returns a newline"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("tab", u"$tab()"_s, QT_TRANSLATE_NOOP("Fooyin", "Returns a tab character"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("swapprefix", u"$swapprefix(text)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Moves leading articles to the end"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("stripprefix", u"$stripprefix(text)"_s, QT_TRANSLATE_NOOP("Fooyin", "Removes leading articles"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("pad", u"$pad(text,length,char)"_s, QT_TRANSLATE_NOOP("Fooyin", "Pads text on the left"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("padright", u"$padright(text,length,char)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Pads text on the right"), QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("repeat", u"$repeat(text,count)"_s, QT_TRANSLATE_NOOP("Fooyin", "Repeats text"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("trim", u"$trim(text)"_s, QT_TRANSLATE_NOOP("Fooyin", "Trims surrounding whitespace"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("lower", u"$lower(text)"_s, QT_TRANSLATE_NOOP("Fooyin", "Converts text to lowercase"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("upper", u"$upper(text)"_s, QT_TRANSLATE_NOOP("Fooyin", "Converts text to uppercase"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("abbr", u"$abbr(text)"_s, QT_TRANSLATE_NOOP("Fooyin", "Builds an abbreviation"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("caps", u"$caps(text)"_s, QT_TRANSLATE_NOOP("Fooyin", "Capitalises words"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("directory", u"$directory(path,level)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Returns a directory name from a path"),
                      QT_TRANSLATE_NOOP("Fooyin", "Path")),
        functionEntry("directory_path", u"$directory_path(path,level)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Returns a parent directory path"),
                      QT_TRANSLATE_NOOP("Fooyin", "Path")),
        functionEntry("elide_end", u"$elide_end(text,width)"_s, QT_TRANSLATE_NOOP("Fooyin", "Elides text at the end"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("elide_mid", u"$elide_mid(text,width)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Elides text in the middle"), QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("ext", u"$ext(path)"_s, QT_TRANSLATE_NOOP("Fooyin", "Returns a file extension"),
                      QT_TRANSLATE_NOOP("Fooyin", "Path")),
        functionEntry("filename", u"$filename(path)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Returns a filename without extension"),
                      QT_TRANSLATE_NOOP("Fooyin", "Path")),
        functionEntry("progress", u"$progress(pos,total,length)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Builds a text progress bar"),
                      QT_TRANSLATE_NOOP("Fooyin", "Utility")),
        functionEntry("progress2", u"$progress2(pos,total,length)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Builds an alternate text progress bar"),
                      QT_TRANSLATE_NOOP("Fooyin", "Utility")),
        functionEntry("doclink", u"$doclink(label,url)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Builds a clickable document or web link"),
                      QT_TRANSLATE_NOOP("Fooyin", "Utility")),
        functionEntry("cmdlink", u"$cmdlink(label,id)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Builds a clickable link to a fooyin command"),
                      QT_TRANSLATE_NOOP("Fooyin", "Utility")),
        functionEntry("urlencode", u"$urlencode(text)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Percent-encodes text for use in URLs"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("isalpha", u"$isalpha(text)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Checks if text contains only alphabetic characters"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("isalnum", u"$isalnum(text)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Checks if text contains only alphanumeric characters"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("isnum", u"$isnum(text)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Checks if text contains only numeric characters"),
                      QT_TRANSLATE_NOOP("Fooyin", "String")),
        functionEntry("timems", u"$timems(milliseconds)"_s, QT_TRANSLATE_NOOP("Fooyin", "Formats milliseconds as time"),
                      QT_TRANSLATE_NOOP("Fooyin", "Numeric")),
        functionEntry("year", u"$year(time)"_s, QT_TRANSLATE_NOOP("Fooyin", "Returns the four-digit year from a date"),
                      QT_TRANSLATE_NOOP("Fooyin", "Time")),
        functionEntry("month", u"$month(time)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Returns the two-digit month from a date"),
                      QT_TRANSLATE_NOOP("Fooyin", "Time")),
        functionEntry("day_of_month", u"$day_of_month(time)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Returns the two-digit day of month from a date"),
                      QT_TRANSLATE_NOOP("Fooyin", "Time")),
        functionEntry("date", u"$date(time)"_s, QT_TRANSLATE_NOOP("Fooyin", "Returns the date formatted as YYYY-MM-DD"),
                      QT_TRANSLATE_NOOP("Fooyin", "Time")),
        functionEntry("time", u"$time(time)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Returns the time formatted as HH:MM or HH:MM:SS"),
                      QT_TRANSLATE_NOOP("Fooyin", "Time")),
        functionEntry("get", u"$get(name)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Returns the value stored in a script variable"),
                      QT_TRANSLATE_NOOP("Fooyin", "Variable")),
        functionEntry("put", u"$put(name,value)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Stores a script variable and returns the value"),
                      QT_TRANSLATE_NOOP("Fooyin", "Variable")),
        functionEntry("puts", u"$puts(name,value)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Stores a script variable and returns nothing"),
                      QT_TRANSLATE_NOOP("Fooyin", "Variable")),
        functionEntry("and", u"$and(expr,…)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Returns true when all expressions are true"),
                      QT_TRANSLATE_NOOP("Fooyin", "Conditional")),
        functionEntry("not", u"$not(expr)"_s, QT_TRANSLATE_NOOP("Fooyin", "Returns the opposite truth value"),
                      QT_TRANSLATE_NOOP("Fooyin", "Conditional")),
        functionEntry("or", u"$or(expr,…)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Returns true when at least one expression is true"),
                      QT_TRANSLATE_NOOP("Fooyin", "Conditional")),
        functionEntry("xor", u"$xor(expr,…)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Returns true when an odd number of expressions are true"),
                      QT_TRANSLATE_NOOP("Fooyin", "Conditional")),
        functionEntry("if", u"$if(condition,then[,else])"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Returns then when condition is true"),
                      QT_TRANSLATE_NOOP("Fooyin", "Conditional")),
        functionEntry("if2", u"$if2(value,fallback)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Returns the first non-empty value"),
                      QT_TRANSLATE_NOOP("Fooyin", "Conditional")),
        functionEntry("if3", u"$if3(a1,a2,…,aN,else)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Returns the first true value from a list, or else when none match"),
                      QT_TRANSLATE_NOOP("Fooyin", "Conditional")),
        functionEntry("ifgreater", u"$ifgreater(x,y,then,else)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Compares numeric values"),
                      QT_TRANSLATE_NOOP("Fooyin", "Conditional")),
        functionEntry("iflonger", u"$iflonger(text,length,then,else)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Checks whether text is longer than a limit"),
                      QT_TRANSLATE_NOOP("Fooyin", "Conditional")),
        functionEntry("ifequal", u"$ifequal(x,y,then,else)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Checks whether two numeric values are equal"),
                      QT_TRANSLATE_NOOP("Fooyin", "Conditional")),
        functionEntry(
            "meta", u"$meta(field)"_s,
            QT_TRANSLATE_NOOP("Fooyin", "Looks up a raw tag field by name. Multiple values are joined with \", \"."),
            QT_TRANSLATE_NOOP("Fooyin", "Lookup")),
        functionEntry(
            "meta", u"$meta(field,index)"_s,
            QT_TRANSLATE_NOOP("Fooyin", "Looks up a raw tag field by name and returns the zero-based indexed value."),
            QT_TRANSLATE_NOOP("Fooyin", "Lookup")),
        functionEntry(
            "meta_sep", u"$meta_sep(field,sep)"_s,
            QT_TRANSLATE_NOOP("Fooyin", "Looks up a raw tag field by name. Multiple values are joined with sep."),
            QT_TRANSLATE_NOOP("Fooyin", "Lookup")),
        functionEntry(
            "meta_sep", u"$meta_sep(field,sep,lastsep)"_s,
            QT_TRANSLATE_NOOP("Fooyin",
                              "Looks up a raw tag field by name. Multiple values are joined with sep, using lastsep "
                              "between the final two values."),
            QT_TRANSLATE_NOOP("Fooyin", "Lookup")),
        functionEntry("meta_test", u"$meta_test(field,…)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Returns 1 when all named tag fields exist."),
                      QT_TRANSLATE_NOOP("Fooyin", "Lookup")),
        functionEntry("meta_num", u"$meta_num(field)"_s,
                      QT_TRANSLATE_NOOP("Fooyin", "Returns the number of values in a raw tag field."),
                      QT_TRANSLATE_NOOP("Fooyin", "Lookup")),
        functionEntry("info", u"$info(field)"_s, QT_TRANSLATE_NOOP("Fooyin", "Looks up technical track information"),
                      QT_TRANSLATE_NOOP("Fooyin", "Lookup")),
    };

    static const auto CommandEntries = [] {
        const auto& aliases = ScriptCommandHandler::scriptCommandAliases();

        std::vector<ScriptReferenceEntry> entries;
        entries.reserve(aliases.size());

        for(const auto& alias : aliases) {
            entries.emplace_back(commandAliasEntry(alias));
        }

        return entries;
    }();

    static const auto AllEntries = [] {
        std::vector<ScriptReferenceEntry> entries;
        entries.reserve(Entries.size() + CommandEntries.size());
        entries.insert(entries.end(), Entries.cbegin(), Entries.cend());
        entries.insert(entries.end(), CommandEntries.cbegin(), CommandEntries.cend());
        return entries;
    }();

    return AllEntries;
}
} // namespace Fooyin
