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
    using FuncRet = std::variant<int, uint64_t, QString, QStringList>;
    using VarFunc = std::function<FuncRet()>;

    Registry();

    bool varExists(const QString& var) const;
    bool funcExists(const QString& func) const;

    inline void setVar(const QString& var, VarFunc func = {})
    {
        m_vars[var] = std::move(func);
    }

    ScriptResult varValue(const QString& var) const;
    ScriptResult function(const QString& func, const ValueList& args) const;

    void changeCurrentTrack(const Core::Track& track);

    template <typename NewCntr, typename Cntr>
    NewCntr containerCast(Cntr&& from) const
    {
        return NewCntr(from.cbegin(), from.cend());
    }

private:
    using NativeFunc     = std::function<QString(const QStringList&)>;
    using NativeCondFunc = std::function<ScriptResult(const ValueList&)>;
    using Func           = std::variant<NativeFunc, NativeCondFunc>;

    using TrackFunc = std::function<FuncRet(const Track&)>;

    void addDefaultFunctions();
    void addDefaultMetadata();

    Track m_currentTrack;
    std::unordered_map<QString, TrackFunc> m_metadata;
    std::unordered_map<QString, VarFunc> m_vars;
    std::unordered_map<QString, Func> m_funcs;
};
} // namespace Fy::Core::Scripting
