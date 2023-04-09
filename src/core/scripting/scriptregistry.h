/*
 * Fooyin
 * Copyright 2022, Luke Taylor <LukeT1@proton.me>
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

#include "scriptvalue.h"

#include <core/models/track.h>

#include <QObject>

namespace Fy::Core::Scripting {
class Registry
{
public:
    Registry();

    bool varExists(const QString& var) const;
    bool funcExists(const QString& func) const;

    ScriptResult varValue(const QString& var) const;
    ScriptResult function(const QString& func, const ValueList& args) const;

    void changeCurrentTrack(const Core::Track& track);

    template <typename NewCntr, typename Cntr>
    NewCntr containerCast(Cntr&& from) const
    {
        return NewCntr(from.cbegin(), from.cend());
    }

private:
    using NativeFunc     = QString (*)(const QStringList&);
    using NativeCondFunc = ScriptResult (*)(const ValueList&);

    using Func = std::variant<NativeFunc, NativeCondFunc>;

    using StringFunc     = QString (Track::*)() const;
    using IntegerFunc    = int (Track::*)() const;
    using StringListFunc = QStringList (Track::*)() const;
    using U64Func        = uint64_t (Track::*)() const;

    using TrackFunc = std::variant<StringFunc, IntegerFunc, StringListFunc, U64Func>;

    void addDefaultFunctions();
    void addDefaultMetadata();

    Track m_currentTrack;
    std::unordered_map<QString, TrackFunc> m_metadata;
    std::unordered_map<QString, QVariant> m_vars;
    std::unordered_map<QString, Func> m_funcs;
};
} // namespace Scripting
