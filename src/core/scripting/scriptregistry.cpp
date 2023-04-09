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
#include "models/track.h"

#include <QVariant>

namespace Fy::Core::Scripting {
Registry::Registry()
    : m_currentTrack{nullptr}
{
    addDefaultFunctions();
    addDefaultMetadata();
}

bool Registry::varExists(const QString& var) const
{
    return m_vars.count(var) || m_metadata.count(var);
}

bool Registry::funcExists(const QString& func) const
{
    return m_funcs.count(func);
}

ScriptResult Registry::varValue(const QString& var) const
{
    if(var.isEmpty()) {
        return {};
    }
    if(m_metadata.count(var)) {
        auto function = m_metadata.at(var);
        if(std::holds_alternative<StringFunc>(function)) {
            const auto f        = std::get<0>(function);
            const QString value = (m_currentTrack.*f)();

            ScriptResult result;
            result.value = value;
            result.cond  = !value.isEmpty();
            return result;
        }
        if(std::holds_alternative<IntegerFunc>(function)) {
            auto f    = std::get<1>(function);
            int value = (m_currentTrack.*f)();

            ScriptResult result;
            result.value = QString::number(value);
            result.cond  = value >= 0;
            return result;
        }
        if(std::holds_alternative<StringListFunc>(function)) {
            const auto f            = std::get<2>(function);
            const QStringList value = (m_currentTrack.*f)();

            ScriptResult result;
            result.value = value.empty() ? "" : value.join(Constants::Separator);
            result.cond  = !value.isEmpty();
            return result;
        }
        if(std::holds_alternative<U64Func>(function)) {
            const auto f         = std::get<3>(function);
            const uint64_t value = (m_currentTrack.*f)();

            ScriptResult result;
            result.value = QString::number(value);
            result.cond  = true;
            return result;
        }
    }

    if(!m_vars.count(var)) {
        return {};
    }

    const QString value = m_vars.at(var).toString();
    return {value, !value.isEmpty()};
}

ScriptResult Registry::function(const QString& func, const ValueList& args) const
{
    if(func.isEmpty() || !m_funcs.count(func)) {
        return {};
    }
    auto function = m_funcs.at(func);
    if(std::holds_alternative<NativeFunc>(function)) {
        const auto f        = std::get<NativeFunc>(function);
        const QString value = f(containerCast<QStringList>(args));

        ScriptResult result;
        result.value = value;
        result.cond  = !value.isEmpty();
        return result;
    }
    if(std::holds_alternative<NativeCondFunc>(function)) {
        const auto f = std::get<NativeCondFunc>(function);
        return f((args));
    }
    return {};
}

void Registry::changeCurrentTrack(const Core::Track& track)
{
    m_currentTrack = track;
}

void Registry::addDefaultFunctions()
{
    m_funcs.emplace("add", NativeFunc(add));
    m_funcs.emplace("sub", NativeFunc(sub));
    m_funcs.emplace("mul", NativeFunc(mul));
    m_funcs.emplace("div", NativeFunc(div));
    m_funcs.emplace("min", NativeFunc(min));
    m_funcs.emplace("max", NativeFunc(max));
    m_funcs.emplace("mod", NativeFunc(mod));

    m_funcs.emplace("if", NativeCondFunc(cif));
    m_funcs.emplace("ifgreater", NativeCondFunc(ifgreater));
    m_funcs.emplace("iflonger", NativeCondFunc(iflonger));
    m_funcs.emplace("ifequal", NativeCondFunc(ifequal));
}

void Registry::addDefaultMetadata()
{
    m_metadata[Constants::MetaData::Title]        = StringFunc(&Track::title);
    m_metadata[Constants::MetaData::Artist]       = StringListFunc(&Track::artists);
    m_metadata[Constants::MetaData::Album]        = StringFunc(&Track::album);
    m_metadata[Constants::MetaData::AlbumArtist]  = StringFunc(&Track::albumArtist);
    m_metadata[Constants::MetaData::Track]        = IntegerFunc(&Track::trackNumber);
    m_metadata[Constants::MetaData::TrackTotal]   = IntegerFunc(&Track::trackTotal);
    m_metadata[Constants::MetaData::Disc]         = IntegerFunc(&Track::discNumber);
    m_metadata[Constants::MetaData::DiscTotal]    = IntegerFunc(&Track::discTotal);
    m_metadata[Constants::MetaData::Genre]        = StringListFunc(&Track::genres);
    m_metadata[Constants::MetaData::Composer]     = StringFunc(&Track::composer);
    m_metadata[Constants::MetaData::Performer]    = StringFunc(&Track::performer);
    m_metadata[Constants::MetaData::Duration]     = U64Func(&Track::duration);
    m_metadata[Constants::MetaData::Lyrics]       = StringFunc(&Track::lyrics);
    m_metadata[Constants::MetaData::Comment]      = StringFunc(&Track::comment);
    m_metadata[Constants::MetaData::Date]         = StringFunc(&Track::date);
    m_metadata[Constants::MetaData::Year]         = IntegerFunc(&Track::year);
    m_metadata[Constants::MetaData::Cover]        = StringFunc(&Track::coverPath);
    m_metadata[Constants::MetaData::FileSize]     = U64Func(&Track::fileSize);
    m_metadata[Constants::MetaData::Bitrate]      = IntegerFunc(&Track::bitrate);
    m_metadata[Constants::MetaData::SampleRate]   = IntegerFunc(&Track::sampleRate);
    m_metadata[Constants::MetaData::PlayCount]    = IntegerFunc(&Track::playCount);
    m_metadata[Constants::MetaData::AddedTime]    = U64Func(&Track::addedTime);
    m_metadata[Constants::MetaData::ModifiedTime] = U64Func(&Track::modifiedTime);
}
} // namespace Fy::Core::Scripting
