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

    Registry();
    virtual ~Registry() = default;

    virtual bool varExists(const QString& var) const;
    virtual bool funcExists(const QString& func) const;

    virtual ScriptResult varValue(const QString& var) const;
    virtual ScriptResult function(const QString& func, const ValueList& args) const;

    void changeCurrentTrack(const Core::Track& track);

protected:
    template <typename NewCntr, typename Cntr>
    NewCntr containerCast(Cntr&& from) const
    {
        return NewCntr(from.cbegin(), from.cend());
    }

    static ScriptResult calculateResult(FuncRet funcRet);

private:
    using NativeFunc     = std::function<QString(const QStringList&)>;
    using NativeCondFunc = std::function<ScriptResult(const ValueList&)>;
    using Func           = std::variant<NativeFunc, NativeCondFunc>;

    using TrackFunc = std::function<FuncRet(const Track&)>;

    void addDefaultFunctions();
    void addDefaultMetadata();

    Track m_currentTrack;
    std::unordered_map<QString, TrackFunc> m_metadata;
    std::unordered_map<QString, Func> m_funcs;
};
} // namespace Fy::Core::Scripting