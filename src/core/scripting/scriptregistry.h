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

#include <QObject>

namespace Fy::Core::Scripting {
struct Value
{
    operator QString() const
    {
        return value;
    }
    QString value;
    bool cond;
};
using ValueList  = std::vector<Value>;
using StringList = std::vector<QString>;

using NativeFunc = QString (*)(const StringList&);

using Func = std::variant<NativeFunc>;

class Registry
{
public:
    Registry();

    Value varValue(const QString& var);
    Value function(const QString& func, const ValueList& args);

    template <typename F, typename C>
    F containerCast(C&& from)
    {
        return F(from.cbegin(), from.cend());
    }

private:
    void addDefaultFunctions();

    std::unordered_map<QString, QString> m_vars;
    std::unordered_map<QString, StringList> m_metadata;
    std::unordered_map<QString, Func> m_funcs;
};
} // namespace Fy::Core::Scripting
