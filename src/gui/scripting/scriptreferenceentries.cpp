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

#include "scriptreferenceentries.h"

#include <core/constants.h>
#include <gui/scripting/scripteditor.h>

using namespace Qt::StringLiterals;

namespace {
QString tr(const char* text)
{
    return Fooyin::ScriptEditor::tr(text);
}

QString variableLabel(const QString& name)
{
    return u"%%1%"_s.arg(name.toLower());
}

Fooyin::ScriptReferenceEntry variableEntry(const QString& name, QString category, QString description)
{
    const QString label = variableLabel(name);

    return {.kind         = Fooyin::ScriptReferenceKind::Variable,
            .label        = label,
            .insertText   = label,
            .category     = std::move(category),
            .description  = std::move(description),
            .cursorOffset = 0};
}

Fooyin::ScriptReferenceEntry variableEntry(const char* name, QString category, QString description)
{
    return variableEntry(QString::fromLatin1(name), std::move(category), std::move(description));
}

Fooyin::ScriptReferenceEntry functionEntry(const char* name, QString signature, QString description)
{
    const QString functionName = QString::fromLatin1(name);

    return {.kind         = Fooyin::ScriptReferenceKind::Function,
            .label        = std::move(signature),
            .insertText   = u"$%1()"_s.arg(functionName),
            .category     = tr("Functions"),
            .description  = std::move(description),
            .cursorOffset = 1};
}
} // namespace

namespace Fooyin {
const std::vector<ScriptReferenceEntry>& scriptReferenceEntries()
{
    using namespace Fooyin::Constants;

    static const std::vector<ScriptReferenceEntry> Entries = {
        variableEntry(MetaData::Title, tr("Metadata"), tr("Track title")),
        variableEntry(MetaData::Artist, tr("Metadata"), tr("Primary artist")),
        variableEntry(MetaData::UniqueArtist, tr("Metadata"), tr("Unique artists combined into one value")),
        variableEntry(MetaData::Album, tr("Metadata"), tr("Album title")),
        variableEntry(MetaData::AlbumArtist, tr("Metadata"), tr("Album artist")),
        variableEntry(MetaData::Track, tr("Metadata"), tr("Track number")),
        variableEntry(MetaData::TrackTotal, tr("Metadata"), tr("Total tracks on the release")),
        variableEntry(MetaData::Disc, tr("Metadata"), tr("Disc number")),
        variableEntry(MetaData::DiscTotal, tr("Metadata"), tr("Total discs on the release")),
        variableEntry(MetaData::Genre, tr("Metadata"), tr("Genres")),
        variableEntry(MetaData::Composer, tr("Metadata"), tr("Composers")),
        variableEntry(MetaData::Performer, tr("Metadata"), tr("Performers")),
        variableEntry(MetaData::Duration, tr("Metadata"), tr("Track duration formatted as time")),
        variableEntry(MetaData::DurationSecs, tr("Metadata"), tr("Track duration in seconds")),
        variableEntry(MetaData::DurationMSecs, tr("Metadata"), tr("Track duration in milliseconds")),
        variableEntry(MetaData::Comment, tr("Metadata"), tr("Comment tag")),
        variableEntry(MetaData::Date, tr("Metadata"), tr("Release date")),
        variableEntry(MetaData::Year, tr("Metadata"), tr("Release year")),
        variableEntry(MetaData::FileSize, tr("Metadata"), tr("File size in bytes")),
        variableEntry(MetaData::FileSizeNatural, tr("Metadata"), tr("Human readable file size")),
        variableEntry(MetaData::Bitrate, tr("Metadata"), tr("Track bitrate")),
        variableEntry(MetaData::SampleRate, tr("Metadata"), tr("Sample rate")),
        variableEntry(MetaData::BitDepth, tr("Metadata"), tr("Bit depth")),
        variableEntry(MetaData::FirstPlayed, tr("Metadata"), tr("First played timestamp")),
        variableEntry(MetaData::LastPlayed, tr("Metadata"), tr("Last played timestamp")),
        variableEntry(MetaData::PlayCount, tr("Metadata"), tr("Play count")),
        variableEntry(MetaData::Rating, tr("Metadata"), tr("Numeric rating")),
        variableEntry(MetaData::RatingStars, tr("Metadata"), tr("Rating shown as stars")),
        variableEntry(MetaData::RatingEditor, tr("Metadata"), tr("Rating editor representation")),
        variableEntry(MetaData::Codec, tr("Metadata"), tr("Codec name")),
        variableEntry(MetaData::CodecProfile, tr("Metadata"), tr("Codec profile")),
        variableEntry(MetaData::Tool, tr("Metadata"), tr("Encoding tool")),
        variableEntry(MetaData::TagType, tr("Metadata"), tr("Tag type list")),
        variableEntry(MetaData::Encoding, tr("Metadata"), tr("Encoding description")),
        variableEntry(MetaData::Channels, tr("Metadata"), tr("Channel layout")),
        variableEntry(MetaData::AddedTime, tr("Metadata"), tr("Library added timestamp")),
        variableEntry(MetaData::LastModified, tr("Metadata"), tr("Last modified timestamp")),
        variableEntry(MetaData::FilePath, tr("Metadata"), tr("Full file path")),
        variableEntry(MetaData::RelativePath, tr("Metadata"), tr("Path relative to the library root")),
        variableEntry(MetaData::FileName, tr("Metadata"), tr("Filename without extension")),
        variableEntry(MetaData::Extension, tr("Metadata"), tr("File extension")),
        variableEntry(MetaData::FileNameWithExt, tr("Metadata"), tr("Filename including extension")),
        variableEntry(MetaData::Directory, tr("Metadata"), tr("Containing directory name")),
        variableEntry(MetaData::Path, tr("Metadata"), tr("Containing directory path")),
        variableEntry(MetaData::Subsong, tr("Metadata"), tr("Subsong index")),
        variableEntry(MetaData::RGTrackGain, tr("Metadata"), tr("ReplayGain track gain")),
        variableEntry(MetaData::RGTrackPeak, tr("Metadata"), tr("ReplayGain track peak")),
        variableEntry(MetaData::RGTrackPeakDB, tr("Metadata"), tr("ReplayGain track peak in dB")),
        variableEntry(MetaData::RGAlbumGain, tr("Metadata"), tr("ReplayGain album gain")),
        variableEntry(MetaData::RGAlbumPeak, tr("Metadata"), tr("ReplayGain album peak")),
        variableEntry(MetaData::RGAlbumPeakDB, tr("Metadata"), tr("ReplayGain album peak in dB")),
        variableEntry("trackcount", tr("Playlist"), tr("Number of tracks in the list")),
        variableEntry("playtime", tr("Playlist"), tr("Combined duration of the track list")),
        variableEntry("playlist_duration", tr("Playlist"), tr("Alias for total playlist duration")),
        variableEntry("playlist_elapsed", tr("Playback"), tr("Elapsed time within the active playlist")),
        variableEntry("genres", tr("Playlist"), tr("Unique genres across the track list")),
        variableEntry("playback_time", tr("Playback"), tr("Current playback position formatted as time")),
        variableEntry("playback_time_s", tr("Playback"), tr("Current playback position in seconds")),
        variableEntry("playback_time_remaining", tr("Playback"), tr("Remaining playback time formatted as time")),
        variableEntry("playback_time_remaining_s", tr("Playback"), tr("Remaining playback time in seconds")),
        variableEntry("isplaying", tr("Playback"), tr("Returns 1 while playback is active")),
        variableEntry("ispaused", tr("Playback"), tr("Returns 1 while playback is paused")),
        variableEntry("libraryname", tr("Library"), tr("Current library name")),
        variableEntry("librarypath", tr("Library"), tr("Current library path")),
        functionEntry("add", u"$add(a,b,...)"_s, tr("Adds numeric arguments")),
        functionEntry("sub", u"$sub(a,b,...)"_s, tr("Subtracts later values from the first")),
        functionEntry("mul", u"$mul(a,b,...)"_s, tr("Multiplies numeric arguments")),
        functionEntry("div", u"$div(a,b)"_s, tr("Divides the first value by the second")),
        functionEntry("min", u"$min(a,b,...)"_s, tr("Returns the smallest numeric value")),
        functionEntry("max", u"$max(a,b,...)"_s, tr("Returns the largest numeric value")),
        functionEntry("mod", u"$mod(a,b)"_s, tr("Returns the remainder of a division")),
        functionEntry("rand", u"$rand(min,max)"_s, tr("Returns a random number in range")),
        functionEntry("round", u"$round(value)"_s, tr("Rounds a numeric value")),
        functionEntry("num", u"$num(value,length)"_s, tr("Formats a number with leading zeroes")),
        functionEntry("replace", u"$replace(text,from,to,...)"_s, tr("Replaces text fragments")),
        functionEntry("ascii", u"$ascii(text)"_s, tr("Converts text to ASCII")),
        functionEntry("slice", u"$slice(text,start,end)"_s, tr("Returns a slice of text")),
        functionEntry("chop", u"$chop(text,count)"_s, tr("Removes characters from the end")),
        functionEntry("left", u"$left(text,count)"_s, tr("Returns characters from the left")),
        functionEntry("right", u"$right(text,count)"_s, tr("Returns characters from the right")),
        functionEntry("insert", u"$insert(text,insert,pos)"_s, tr("Inserts text at a position")),
        functionEntry("substr", u"$substr(text,start,length)"_s, tr("Returns a substring")),
        functionEntry("strstr", u"$strstr(text,needle)"_s, tr("Finds a substring position")),
        functionEntry("stristr", u"$stristr(text,needle)"_s, tr("Finds a substring position ignoring case")),
        functionEntry("strstrlast", u"$strstrlast(text,needle)"_s, tr("Finds the last substring position")),
        functionEntry("stristrlast", u"$stristrlast(text,needle)"_s,
                      tr("Finds the last substring position ignoring case")),
        functionEntry("split", u"$split(text,sep,index)"_s, tr("Returns one split segment")),
        functionEntry("len", u"$len(text)"_s, tr("Returns the text length")),
        functionEntry("longest", u"$longest(a,b,...)"_s, tr("Returns the longest string")),
        functionEntry("strcmp", u"$strcmp(a,b)"_s, tr("Compares two strings")),
        functionEntry("stricmp", u"$stricmp(a,b)"_s, tr("Compares two strings ignoring case")),
        functionEntry("longer", u"$longer(a,b)"_s, tr("Tests whether one string is longer")),
        functionEntry("sep", u"$sep()"_s, tr("Returns the unit separator character")),
        functionEntry("crlf", u"$crlf()"_s, tr("Returns a newline")),
        functionEntry("tab", u"$tab()"_s, tr("Returns a tab character")),
        functionEntry("swapprefix", u"$swapprefix(text)"_s, tr("Moves leading articles to the end")),
        functionEntry("stripprefix", u"$stripprefix(text)"_s, tr("Removes leading articles")),
        functionEntry("pad", u"$pad(text,length,char)"_s, tr("Pads text on the left")),
        functionEntry("padright", u"$padright(text,length,char)"_s, tr("Pads text on the right")),
        functionEntry("repeat", u"$repeat(text,count)"_s, tr("Repeats text")),
        functionEntry("trim", u"$trim(text)"_s, tr("Trims surrounding whitespace")),
        functionEntry("lower", u"$lower(text)"_s, tr("Converts text to lowercase")),
        functionEntry("upper", u"$upper(text)"_s, tr("Converts text to uppercase")),
        functionEntry("abbr", u"$abbr(text)"_s, tr("Builds an abbreviation")),
        functionEntry("caps", u"$caps(text)"_s, tr("Capitalises words")),
        functionEntry("directory", u"$directory(path,level)"_s, tr("Returns a directory name from a path")),
        functionEntry("directory_path", u"$directory_path(path,level)"_s, tr("Returns a parent directory path")),
        functionEntry("elide_end", u"$elide_end(text,width)"_s, tr("Elides text at the end")),
        functionEntry("elide_mid", u"$elide_mid(text,width)"_s, tr("Elides text in the middle")),
        functionEntry("ext", u"$ext(path)"_s, tr("Returns a file extension")),
        functionEntry("filename", u"$filename(path)"_s, tr("Returns a filename without extension")),
        functionEntry("progress", u"$progress(pos,total,length)"_s, tr("Builds a text progress bar")),
        functionEntry("progress2", u"$progress2(pos,total,length)"_s, tr("Builds an alternate text progress bar")),
        functionEntry("timems", u"$timems(milliseconds)"_s, tr("Formats milliseconds as time")),
        functionEntry("if", u"$if(condition,then[,else])"_s, tr("Returns then when condition is true")),
        functionEntry("if2", u"$if2(value,fallback)"_s, tr("Returns the first non-empty value")),
        functionEntry("ifgreater", u"$ifgreater(a,b,then,else)"_s, tr("Compares numeric values")),
        functionEntry("iflonger", u"$iflonger(text,length,then,else)"_s,
                      tr("Checks whether text is longer than a limit")),
        functionEntry("ifequal", u"$ifequal(a,b,then,else)"_s, tr("Checks whether two values are equal")),
        functionEntry("meta", u"$meta(field)"_s, tr("Looks up a raw tag field by name")),
        functionEntry("info", u"$info(field)"_s, tr("Looks up technical track information")),
    };

    return Entries;
}
} // namespace Fooyin
