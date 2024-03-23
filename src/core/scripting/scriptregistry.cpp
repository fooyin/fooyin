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

#include <core/scripting/scriptregistry.h>

#include "functions/controlfuncs.h"
#include "functions/mathfuncs.h"
#include "functions/stringfuncs.h"
#include "functions/timefuncs.h"
#include "functions/tracklistfuncs.h"

#include <core/constants.h>
#include <core/track.h>

namespace {
using NativeFunc     = std::function<QString(const QStringList&)>;
using NativeVoidFunc = std::function<QString()>;
using NativeBoolFunc = std::function<Fooyin::ScriptResult(const QStringList&)>;
using NativeCondFunc = std::function<Fooyin::ScriptResult(const Fooyin::ScriptValueList&)>;
using Func           = std::variant<NativeFunc, NativeVoidFunc, NativeBoolFunc, NativeCondFunc>;

using TrackFunc     = std::function<Fooyin::ScriptRegistry::FuncRet(const Fooyin::Track&)>;
using TrackSetFunc  = std::function<void(Fooyin::Track&, const Fooyin::ScriptRegistry::FuncRet&)>;
using TrackListFunc = std::function<Fooyin::ScriptRegistry::FuncRet(const Fooyin::TrackList&)>;

template <typename FuncType>
auto generateSetFunc(FuncType func)
{
    return [func](Fooyin::Track& track, Fooyin::ScriptRegistry::FuncRet arg) {
        if constexpr(std::is_same_v<FuncType, void (Fooyin::Track::*)(int)>) {
            if(const auto* value = std::get_if<int>(&arg)) {
                (track.*func)(*value);
            }
        }
        else if constexpr(std::is_same_v<FuncType, void (Fooyin::Track::*)(uint64_t)>) {
            if(const auto* value = std::get_if<uint64_t>(&arg)) {
                (track.*func)(*value);
            }
        }
        else if constexpr(std::is_same_v<FuncType, void (Fooyin::Track::*)(const QString&)>) {
            if(const auto* value = std::get_if<QString>(&arg)) {
                (track.*func)(*value);
            }
        }
        else if constexpr(std::is_same_v<FuncType, void (Fooyin::Track::*)(const QStringList&)>) {
            if(const auto* value = std::get_if<QStringList>(&arg)) {
                (track.*func)(*value);
            }
        }
    };
}
} // namespace

namespace Fooyin {
struct ScriptRegistry::Private
{
    std::unordered_map<QString, TrackFunc> metadata;
    std::unordered_map<QString, TrackSetFunc> setMetadata;
    std::unordered_map<QString, TrackListFunc> listProperties;
    std::unordered_map<QString, Func> funcs;

    Private()
    {
        addDefaultFunctions();
        addDefaultListFuncs();
        addDefaultMetadata();
    }

    void addDefaultFunctions()
    {
        funcs.emplace(QStringLiteral("add"), Fooyin::Scripting::add);
        funcs.emplace(QStringLiteral("sub"), Fooyin::Scripting::sub);
        funcs.emplace(QStringLiteral("mul"), Fooyin::Scripting::mul);
        funcs.emplace(QStringLiteral("div"), Fooyin::Scripting::div);
        funcs.emplace(QStringLiteral("min"), Fooyin::Scripting::min);
        funcs.emplace(QStringLiteral("max"), Fooyin::Scripting::max);
        funcs.emplace(QStringLiteral("mod"), Fooyin::Scripting::mod);

        funcs.emplace(QStringLiteral("num"), Fooyin::Scripting::num);
        funcs.emplace(QStringLiteral("replace"), Fooyin::Scripting::replace);
        funcs.emplace(QStringLiteral("chop"), Fooyin::Scripting::chop);
        funcs.emplace(QStringLiteral("slice"), Fooyin::Scripting::slice);
        funcs.emplace(QStringLiteral("left"), Fooyin::Scripting::left);
        funcs.emplace(QStringLiteral("right"), Fooyin::Scripting::right);
        funcs.emplace(QStringLiteral("strcmp"), Fooyin::Scripting::strcmp);
        funcs.emplace(QStringLiteral("strcmpi"), Fooyin::Scripting::strcmpi);
        funcs.emplace(QStringLiteral("sep"), Fooyin::Scripting::sep);

        funcs.emplace(QStringLiteral("timems"), Fooyin::Scripting::msToString);

        funcs.emplace(QStringLiteral("if"), Fooyin::Scripting::cif);
        funcs.emplace(QStringLiteral("if2"), Fooyin::Scripting::cif2);
        funcs.emplace(QStringLiteral("ifgreater"), Fooyin::Scripting::ifgreater);
        funcs.emplace(QStringLiteral("iflonger"), Fooyin::Scripting::iflonger);
        funcs.emplace(QStringLiteral("ifequal"), Fooyin::Scripting::ifequal);
    }

    void addDefaultListFuncs()
    {
        listProperties.emplace(QStringLiteral("trackcount"), Fooyin::Scripting::trackCount);
        listProperties.emplace(QStringLiteral("playtime"), Fooyin::Scripting::playtime);
        listProperties.emplace(QStringLiteral("genres"), Fooyin::Scripting::genres);
    }

    void addDefaultMetadata()
    {
        using namespace Fooyin::Constants;
        using Fooyin::Track;

        metadata[QString::fromLatin1(MetaData::Title)]        = &Track::title;
        metadata[QString::fromLatin1(MetaData::Artist)]       = &Track::artists;
        metadata[QString::fromLatin1(MetaData::Album)]        = &Track::album;
        metadata[QString::fromLatin1(MetaData::AlbumArtist)]  = &Track::albumArtist;
        metadata[QString::fromLatin1(MetaData::Track)]        = &Track::trackNumber;
        metadata[QString::fromLatin1(MetaData::TrackTotal)]   = &Track::trackTotal;
        metadata[QString::fromLatin1(MetaData::Disc)]         = &Track::discNumber;
        metadata[QString::fromLatin1(MetaData::DiscTotal)]    = &Track::discTotal;
        metadata[QString::fromLatin1(MetaData::Genre)]        = &Track::genres;
        metadata[QString::fromLatin1(MetaData::Composer)]     = &Track::composer;
        metadata[QString::fromLatin1(MetaData::Performer)]    = &Track::performer;
        metadata[QString::fromLatin1(MetaData::Duration)]     = &Track::duration;
        metadata[QString::fromLatin1(MetaData::Comment)]      = &Track::comment;
        metadata[QString::fromLatin1(MetaData::Date)]         = &Track::date;
        metadata[QString::fromLatin1(MetaData::Year)]         = &Track::year;
        metadata[QString::fromLatin1(MetaData::Cover)]        = &Track::coverPath;
        metadata[QString::fromLatin1(MetaData::FileSize)]     = &Track::fileSize;
        metadata[QString::fromLatin1(MetaData::Bitrate)]      = &Track::bitrate;
        metadata[QString::fromLatin1(MetaData::SampleRate)]   = &Track::sampleRate;
        metadata[QString::fromLatin1(MetaData::PlayCount)]    = &Track::playCount;
        metadata[QString::fromLatin1(MetaData::FileType)]     = &Track::typeString;
        metadata[QString::fromLatin1(MetaData::AddedTime)]    = &Track::addedTime;
        metadata[QString::fromLatin1(MetaData::ModifiedTime)] = &Track::modifiedTime;
        metadata[QString::fromLatin1(MetaData::FilePath)]     = &Track::filepath;
        metadata[QString::fromLatin1(MetaData::RelativePath)] = &Track::relativePath;
        metadata[QString::fromLatin1(MetaData::FileName)]     = &Track::filename;

        setMetadata[QString::fromLatin1(MetaData::Title)]        = generateSetFunc(&Track::setTitle);
        setMetadata[QString::fromLatin1(MetaData::Artist)]       = generateSetFunc(&Track::setArtists);
        setMetadata[QString::fromLatin1(MetaData::Album)]        = generateSetFunc(&Track::setAlbum);
        setMetadata[QString::fromLatin1(MetaData::AlbumArtist)]  = generateSetFunc(&Track::setAlbumArtist);
        setMetadata[QString::fromLatin1(MetaData::Track)]        = generateSetFunc(&Track::setTrackNumber);
        setMetadata[QString::fromLatin1(MetaData::TrackTotal)]   = generateSetFunc(&Track::setTrackTotal);
        setMetadata[QString::fromLatin1(MetaData::Disc)]         = generateSetFunc(&Track::setDiscNumber);
        setMetadata[QString::fromLatin1(MetaData::DiscTotal)]    = generateSetFunc(&Track::setDiscTotal);
        setMetadata[QString::fromLatin1(MetaData::Genre)]        = generateSetFunc(&Track::setGenres);
        setMetadata[QString::fromLatin1(MetaData::Composer)]     = generateSetFunc(&Track::setComposer);
        setMetadata[QString::fromLatin1(MetaData::Performer)]    = generateSetFunc(&Track::setPerformer);
        setMetadata[QString::fromLatin1(MetaData::Duration)]     = generateSetFunc(&Track::setDuration);
        setMetadata[QString::fromLatin1(MetaData::Comment)]      = generateSetFunc(&Track::setComment);
        setMetadata[QString::fromLatin1(MetaData::Date)]         = generateSetFunc(&Track::setDate);
        setMetadata[QString::fromLatin1(MetaData::Year)]         = generateSetFunc(&Track::setYear);
        setMetadata[QString::fromLatin1(MetaData::Cover)]        = generateSetFunc(&Track::setCoverPath);
        setMetadata[QString::fromLatin1(MetaData::FileSize)]     = generateSetFunc(&Track::setFileSize);
        setMetadata[QString::fromLatin1(MetaData::Bitrate)]      = generateSetFunc(&Track::setBitrate);
        setMetadata[QString::fromLatin1(MetaData::SampleRate)]   = generateSetFunc(&Track::setSampleRate);
        setMetadata[QString::fromLatin1(MetaData::PlayCount)]    = generateSetFunc(&Track::setPlayCount);
        setMetadata[QString::fromLatin1(MetaData::AddedTime)]    = generateSetFunc(&Track::setAddedTime);
        setMetadata[QString::fromLatin1(MetaData::ModifiedTime)] = generateSetFunc(&Track::setModifiedTime);
    }
};

ScriptRegistry::ScriptRegistry()
    : p{std::make_unique<Private>()}
{ }

ScriptRegistry::~ScriptRegistry() = default;

bool ScriptRegistry::isVariable(const QString& var, const Track& track) const
{
    return p->metadata.contains(var) || track.hasExtraTag(var.toUpper());
}

bool ScriptRegistry::isVariable(const QString& var, const TrackList& tracks) const
{
    if(!isListVariable(var)) {
        return !tracks.empty() && isVariable(var, tracks.front());
    }

    return true;
}

bool ScriptRegistry::isFunction(const QString& func) const
{
    return p->funcs.contains(func);
}

ScriptResult ScriptRegistry::value(const QString& var, const Track& track) const
{
    if(var.isEmpty() || !isVariable(var, track)) {
        return {};
    }

    if(p->metadata.contains(var)) {
        return calculateResult(p->metadata.at(var)(track));
    }

    return calculateResult(track.extraTag(var.toUpper()));
}

ScriptResult ScriptRegistry::value(const QString& var, const TrackList& tracks) const
{
    if(var.isEmpty() || !isVariable(var, tracks)) {
        return {};
    }

    if(p->listProperties.contains(var)) {
        return calculateResult(p->listProperties.at(var)(tracks));
    }

    if(!tracks.empty()) {
        if(p->metadata.contains(var)) {
            return calculateResult(p->metadata.at(var)(tracks.front()));
        }
        return calculateResult(tracks.front().extraTag(var.toUpper()));
    }

    return {};
}

ScriptResult ScriptRegistry::function(const QString& func, const ScriptValueList& args) const
{
    if(func.isEmpty() || !p->funcs.contains(func)) {
        return {};
    }

    auto function = p->funcs.at(func);
    if(std::holds_alternative<NativeFunc>(function)) {
        const QString value = std::get<NativeFunc>(function)(containerCast<QStringList>(args));
        return {.value = value, .cond = !value.isEmpty()};
    }
    if(std::holds_alternative<NativeVoidFunc>(function)) {
        const QString value = std::get<NativeVoidFunc>(function)();
        return {.value = value, .cond = !value.isEmpty()};
    }
    if(std::holds_alternative<NativeBoolFunc>(function)) {
        return std::get<NativeBoolFunc>(function)(containerCast<QStringList>(args));
    }
    if(std::holds_alternative<NativeCondFunc>(function)) {
        return std::get<NativeCondFunc>(function)(args);
    }

    return {};
}

void ScriptRegistry::setValue(const QString& var, const FuncRet& value, Track& track)
{
    if(var.isEmpty()) {
        return;
    }

    if(isVariable(var, track)) {
        p->setMetadata.at(var)(track, value);
    }
}

bool ScriptRegistry::isListVariable(const QString& var) const
{
    return p->listProperties.contains(var);
}

ScriptResult ScriptRegistry::calculateResult(ScriptRegistry::FuncRet funcRet)
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
        result.value = strListVal->empty() ? QStringLiteral("") : strListVal->join(u"\037");
        result.cond  = !strListVal->isEmpty();
    }

    return result;
}
} // namespace Fooyin
