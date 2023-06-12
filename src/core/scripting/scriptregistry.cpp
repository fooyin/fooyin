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
    return m_vars.contains(var) || m_metadata.contains(var);
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

    auto funcResult = m_vars.contains(var) ? m_vars.at(var)() : m_metadata.at(var)(m_currentTrack);
    if(std::holds_alternative<int>(funcResult)) {
        const int value = std::get<0>(funcResult);

        ScriptResult result;
        result.value = QString::number(value);
        result.cond  = value >= 0;
        return result;
    }
    if(std::holds_alternative<uint64_t>(funcResult)) {
        const uint64_t value = std::get<1>(funcResult);

        ScriptResult result;
        result.value = QString::number(value);
        result.cond  = true;
        return result;
    }
    if(std::holds_alternative<QString>(funcResult)) {
        const QString value = std::get<2>(funcResult);

        ScriptResult result;
        result.value = value;
        result.cond  = !value.isEmpty();
        return result;
    }
    if(std::holds_alternative<QStringList>(funcResult)) {
        const QStringList value = std::get<3>(funcResult);

        ScriptResult result;
        result.value = value.empty() ? "" : value.join(Constants::Separator);
        result.cond  = !value.isEmpty();
        return result;
    }
    return {};
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

    m_funcs.emplace("timems", msToString);

    m_funcs.emplace("if", cif);
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
}
} // namespace Fy::Core::Scripting
