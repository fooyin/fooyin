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

#include "scriptregistry.h"

#include "core/constants.h"
#include "functions/controlfuncs.h"
#include "functions/mathfuncs.h"
#include "functions/stringfuncs.h"
#include "functions/timefuncs.h"
#include "models/track.h"

namespace Fy::Core::Scripting {
Registry::Registry()
    : m_currentTrack{nullptr}
{
    addDefaultFunctions();
    addDefaultMetadata();
}

bool Registry::varExists(const QString& var) const
{
    return m_metadata.contains(var);
}

bool Registry::funcExists(const QString& func) const
{
    return m_funcs.contains(func);
}

ScriptResult Registry::varValue(const QString& var) const
{
    if(var.isEmpty() || !varExists(var)) {
        return {};
    }

    auto funcResult = m_metadata.at(var)(m_currentTrack);
    return calculateResult(funcResult);
}

void Registry::setVar(const QString& var, const FuncRet& value, Track& track)
{
    if(var.isEmpty() || !varExists(var)) {
        return;
    }

    m_setMetadata.at(var)(track, value);
}

ScriptResult Registry::function(const QString& func, const ValueList& args) const
{
    if(func.isEmpty() || !m_funcs.contains(func)) {
        return {};
    }

    auto function = m_funcs.at(func);
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
    m_currentTrack = track;
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
        result.value = strListVal->empty() ? "" : strListVal->join(Constants::Separator);
        result.cond  = !strListVal->isEmpty();
    }

    return result;
}

void Registry::addDefaultFunctions()
{
    m_funcs.emplace("add", add);
    m_funcs.emplace("sub", sub);
    m_funcs.emplace("mul", mul);
    m_funcs.emplace("div", div);
    m_funcs.emplace("min", min);
    m_funcs.emplace("max", max);
    m_funcs.emplace("mod", mod);

    m_funcs.emplace("num", num);
    m_funcs.emplace("replace", replace);

    m_funcs.emplace("timems", msToString);

    m_funcs.emplace("if", cif);
    m_funcs.emplace("if2", cif2);
    m_funcs.emplace("ifgreater", ifgreater);
    m_funcs.emplace("iflonger", iflonger);
    m_funcs.emplace("ifequal", ifequal);
}

void Registry::addDefaultMetadata()
{
    m_metadata[Constants::MetaData::Title]        = &Track::title;
    m_metadata[Constants::MetaData::Artist]       = &Track::artists;
    m_metadata[Constants::MetaData::Album]        = &Track::album;
    m_metadata[Constants::MetaData::AlbumArtist]  = &Track::albumArtist;
    m_metadata[Constants::MetaData::Track]        = &Track::trackNumber;
    m_metadata[Constants::MetaData::TrackTotal]   = &Track::trackTotal;
    m_metadata[Constants::MetaData::Disc]         = &Track::discNumber;
    m_metadata[Constants::MetaData::DiscTotal]    = &Track::discTotal;
    m_metadata[Constants::MetaData::Genre]        = &Track::genres;
    m_metadata[Constants::MetaData::Composer]     = &Track::composer;
    m_metadata[Constants::MetaData::Performer]    = &Track::performer;
    m_metadata[Constants::MetaData::Duration]     = &Track::duration;
    m_metadata[Constants::MetaData::Lyrics]       = &Track::lyrics;
    m_metadata[Constants::MetaData::Comment]      = &Track::comment;
    m_metadata[Constants::MetaData::Date]         = &Track::date;
    m_metadata[Constants::MetaData::Year]         = &Track::year;
    m_metadata[Constants::MetaData::Cover]        = &Track::coverPath;
    m_metadata[Constants::MetaData::FileSize]     = &Track::fileSize;
    m_metadata[Constants::MetaData::Bitrate]      = &Track::bitrate;
    m_metadata[Constants::MetaData::SampleRate]   = &Track::sampleRate;
    m_metadata[Constants::MetaData::PlayCount]    = &Track::playCount;
    m_metadata[Constants::MetaData::AddedTime]    = &Track::addedTime;
    m_metadata[Constants::MetaData::ModifiedTime] = &Track::modifiedTime;
    m_metadata[Constants::MetaData::FilePath]     = &Track::filepath;
    m_metadata[Constants::MetaData::RelativePath] = &Track::relativePath;

    m_setMetadata[Constants::MetaData::Title]        = generateSetFunc(&Track::setTitle);
    m_setMetadata[Constants::MetaData::Artist]       = generateSetFunc(&Track::setArtists);
    m_setMetadata[Constants::MetaData::Album]        = generateSetFunc(&Track::setAlbum);
    m_setMetadata[Constants::MetaData::AlbumArtist]  = generateSetFunc(&Track::setAlbumArtist);
    m_setMetadata[Constants::MetaData::Track]        = generateSetFunc(&Track::setTrackNumber);
    m_setMetadata[Constants::MetaData::TrackTotal]   = generateSetFunc(&Track::setTrackTotal);
    m_setMetadata[Constants::MetaData::Disc]         = generateSetFunc(&Track::setDiscNumber);
    m_setMetadata[Constants::MetaData::DiscTotal]    = generateSetFunc(&Track::setDiscTotal);
    m_setMetadata[Constants::MetaData::Genre]        = generateSetFunc(&Track::setGenres);
    m_setMetadata[Constants::MetaData::Composer]     = generateSetFunc(&Track::setComposer);
    m_setMetadata[Constants::MetaData::Performer]    = generateSetFunc(&Track::setPerformer);
    m_setMetadata[Constants::MetaData::Duration]     = generateSetFunc(&Track::setDuration);
    m_setMetadata[Constants::MetaData::Lyrics]       = generateSetFunc(&Track::setLyrics);
    m_setMetadata[Constants::MetaData::Comment]      = generateSetFunc(&Track::setComment);
    m_setMetadata[Constants::MetaData::Date]         = generateSetFunc(&Track::setDate);
    m_setMetadata[Constants::MetaData::Year]         = generateSetFunc(&Track::setYear);
    m_setMetadata[Constants::MetaData::Cover]        = generateSetFunc(&Track::setCoverPath);
    m_setMetadata[Constants::MetaData::FileSize]     = generateSetFunc(&Track::setFileSize);
    m_setMetadata[Constants::MetaData::Bitrate]      = generateSetFunc(&Track::setBitrate);
    m_setMetadata[Constants::MetaData::SampleRate]   = generateSetFunc(&Track::setSampleRate);
    m_setMetadata[Constants::MetaData::PlayCount]    = generateSetFunc(&Track::setPlayCount);
    m_setMetadata[Constants::MetaData::AddedTime]    = generateSetFunc(&Track::setAddedTime);
    m_setMetadata[Constants::MetaData::ModifiedTime] = generateSetFunc(&Track::setModifiedTime);
}
} // namespace Fy::Core::Scripting
