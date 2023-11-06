/*
 * Fooyin
 * Copyright 2022-2023, Luke Taylor <LukeT1@proton.me>
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

#include <core/scripting/scriptregistry.h>

#include "functions/controlfuncs.h"
#include "functions/mathfuncs.h"
#include "functions/stringfuncs.h"
#include "functions/timefuncs.h"

#include <core/constants.h>
#include <core/track.h>

namespace {
using namespace Fy::Core::Scripting;

using NativeFunc     = std::function<QString(const QStringList&)>;
using NativeCondFunc = std::function<ScriptResult(const ValueList&)>;
using Func           = std::variant<NativeFunc, NativeCondFunc>;

using TrackFunc    = std::function<Registry::FuncRet(const Fy::Core::Track&)>;
using TrackSetFunc = std::function<void(Fy::Core::Track&, const Registry::FuncRet&)>;

template <typename FuncType>
auto generateSetFunc(FuncType func)
{
    return [func](Fy::Core::Track& track, Registry::FuncRet arg) {
        if constexpr(std::is_same_v<FuncType, void (Fy::Core::Track::*)(int)>) {
            if(const auto* value = std::get_if<int>(&arg)) {
                (track.*func)(*value);
            }
        }
        else if constexpr(std::is_same_v<FuncType, void (Fy::Core::Track::*)(uint64_t)>) {
            if(const auto* value = std::get_if<uint64_t>(&arg)) {
                (track.*func)(*value);
            }
        }
        else if constexpr(std::is_same_v<FuncType, void (Fy::Core::Track::*)(const QString&)>) {
            if(const auto* value = std::get_if<QString>(&arg)) {
                (track.*func)(*value);
            }
        }
        else if constexpr(std::is_same_v<FuncType, void (Fy::Core::Track::*)(const QStringList&)>) {
            if(const auto* value = std::get_if<QStringList>(&arg)) {
                (track.*func)(*value);
            }
        }
    };
}

void addDefaultFunctions(std::unordered_map<QString, Func>& funcs)
{
    funcs.emplace("add", add);
    funcs.emplace("sub", sub);
    funcs.emplace("mul", mul);
    // funcs.emplace("div", div);
    funcs.emplace("min", min);
    funcs.emplace("max", max);
    funcs.emplace("mod", mod);

    funcs.emplace("num", num);
    funcs.emplace("replace", replace);

    funcs.emplace("timems", msToString);

    funcs.emplace("if", cif);
    funcs.emplace("if2", cif2);
    funcs.emplace("ifgreater", ifgreater);
    funcs.emplace("iflonger", iflonger);
    funcs.emplace("ifequal", ifequal);
}

void addDefaultMetadata(std::unordered_map<QString, TrackFunc>& metadata,
                        std::unordered_map<QString, TrackSetFunc>& setMetadata)
{
    using namespace Fy::Core::Constants;
    using Fy::Core::Track;

    metadata[MetaData::Title]        = &Track::title;
    metadata[MetaData::Artist]       = &Track::artists;
    metadata[MetaData::Album]        = &Track::album;
    metadata[MetaData::AlbumArtist]  = &Track::albumArtist;
    metadata[MetaData::Track]        = &Track::trackNumber;
    metadata[MetaData::TrackTotal]   = &Track::trackTotal;
    metadata[MetaData::Disc]         = &Track::discNumber;
    metadata[MetaData::DiscTotal]    = &Track::discTotal;
    metadata[MetaData::Genre]        = &Track::genres;
    metadata[MetaData::Composer]     = &Track::composer;
    metadata[MetaData::Performer]    = &Track::performer;
    metadata[MetaData::Duration]     = &Track::duration;
    metadata[MetaData::Lyrics]       = &Track::lyrics;
    metadata[MetaData::Comment]      = &Track::comment;
    metadata[MetaData::Date]         = &Track::date;
    metadata[MetaData::Year]         = &Track::year;
    metadata[MetaData::Cover]        = &Track::coverPath;
    metadata[MetaData::FileSize]     = &Track::fileSize;
    metadata[MetaData::Bitrate]      = &Track::bitrate;
    metadata[MetaData::SampleRate]   = &Track::sampleRate;
    metadata[MetaData::PlayCount]    = &Track::playCount;
    metadata[MetaData::FileType]     = &Track::typeString;
    metadata[MetaData::AddedTime]    = &Track::addedTime;
    metadata[MetaData::ModifiedTime] = &Track::modifiedTime;
    metadata[MetaData::FilePath]     = &Track::filepath;
    metadata[MetaData::RelativePath] = &Track::relativePath;

    setMetadata[MetaData::Title]        = generateSetFunc(&Track::setTitle);
    setMetadata[MetaData::Artist]       = generateSetFunc(&Track::setArtists);
    setMetadata[MetaData::Album]        = generateSetFunc(&Track::setAlbum);
    setMetadata[MetaData::AlbumArtist]  = generateSetFunc(&Track::setAlbumArtist);
    setMetadata[MetaData::Track]        = generateSetFunc(&Track::setTrackNumber);
    setMetadata[MetaData::TrackTotal]   = generateSetFunc(&Track::setTrackTotal);
    setMetadata[MetaData::Disc]         = generateSetFunc(&Track::setDiscNumber);
    setMetadata[MetaData::DiscTotal]    = generateSetFunc(&Track::setDiscTotal);
    setMetadata[MetaData::Genre]        = generateSetFunc(&Track::setGenres);
    setMetadata[MetaData::Composer]     = generateSetFunc(&Track::setComposer);
    setMetadata[MetaData::Performer]    = generateSetFunc(&Track::setPerformer);
    setMetadata[MetaData::Duration]     = generateSetFunc(&Track::setDuration);
    setMetadata[MetaData::Lyrics]       = generateSetFunc(&Track::setLyrics);
    setMetadata[MetaData::Comment]      = generateSetFunc(&Track::setComment);
    setMetadata[MetaData::Date]         = generateSetFunc(&Track::setDate);
    setMetadata[MetaData::Year]         = generateSetFunc(&Track::setYear);
    setMetadata[MetaData::Cover]        = generateSetFunc(&Track::setCoverPath);
    setMetadata[MetaData::FileSize]     = generateSetFunc(&Track::setFileSize);
    setMetadata[MetaData::Bitrate]      = generateSetFunc(&Track::setBitrate);
    setMetadata[MetaData::SampleRate]   = generateSetFunc(&Track::setSampleRate);
    setMetadata[MetaData::PlayCount]    = generateSetFunc(&Track::setPlayCount);
    setMetadata[MetaData::AddedTime]    = generateSetFunc(&Track::setAddedTime);
    setMetadata[MetaData::ModifiedTime] = generateSetFunc(&Track::setModifiedTime);
}
} // namespace

namespace Fy::Core::Scripting {
struct Registry::Private
{
    Track currentTrack;

    std::unordered_map<QString, TrackFunc> metadata;
    std::unordered_map<QString, TrackSetFunc> setMetadata;
    std::unordered_map<QString, Func> funcs;

    Private()
    {
        addDefaultFunctions(funcs);
        addDefaultMetadata(metadata, setMetadata);
    }
};

Registry::Registry()
    : p{std::make_unique<Private>()}
{ }

Registry::~Registry() = default;

bool Registry::varExists(const QString& var) const
{
    return p->metadata.contains(var);
}

bool Registry::funcExists(const QString& func) const
{
    return p->funcs.contains(func);
}

ScriptResult Registry::varValue(const QString& var) const
{
    if(var.isEmpty() || !varExists(var)) {
        return {};
    }

    auto funcResult = p->metadata.at(var)(p->currentTrack);
    return calculateResult(funcResult);
}

void Registry::setVar(const QString& var, const FuncRet& value, Track& track)
{
    if(var.isEmpty() || !varExists(var)) {
        return;
    }

    p->setMetadata.at(var)(track, value);
}

ScriptResult Registry::function(const QString& func, const ValueList& args) const
{
    if(func.isEmpty() || !p->funcs.contains(func)) {
        return {};
    }

    auto function = p->funcs.at(func);
    if(std::holds_alternative<NativeFunc>(function)) {
        const QString value = std::get<NativeFunc>(function)(containerCast<QStringList>(args));

        ScriptResult result;
        result.value = value;
        result.cond  = !value.isEmpty();
        return result;
    }
    if(std::holds_alternative<NativeCondFunc>(function)) {
        return std::get<NativeCondFunc>(function)(args);
    }
    return {};
}

void Registry::changeCurrentTrack(const Core::Track& track)
{
    p->currentTrack = track;
}

ScriptResult Registry::calculateResult(Registry::FuncRet funcRet)
{
    ScriptResult result;

    if(auto* intVal = std::get_if<int>(&funcRet)) {
        result.value = QString::number(*intVal);
        result.cond  = (*intVal) >= 0;
    }
    else if(auto* uintVal = std::get_if<uint64_t>(&funcRet)) {
        result.value = QString::number(*uintVal);
        result.cond  = true;
    }
    else if(auto* strVal = std::get_if<QString>(&funcRet)) {
        result.value = *strVal;
        result.cond  = !strVal->isEmpty();
    }
    else if(auto* strListVal = std::get_if<QStringList>(&funcRet)) {
        result.value = strListVal->empty() ? QStringLiteral("") : strListVal->join(Constants::Separator);
        result.cond  = !strListVal->isEmpty();
    }

    return result;
}
} // namespace Fy::Core::Scripting
