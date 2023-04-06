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

#pragma once

#include <QString>

namespace Fy::Core::Scripting {
enum ExprType : int
{
    Literal     = 0,
    Variable    = 1,
    Function    = 2,
    FunctionArg = 3,
    Conditional = 4,
    Null        = 5
};

struct Expression;

struct FuncValue
{
    QString name;
    std::vector<Expression> args;
};

using ExpressionList = std::vector<Expression>;

using ExpressionValue = std::variant<QString, FuncValue, ExpressionList>;

struct Expression
{
    ExprType type{Null};
    ExpressionValue value{""};
};
} // namespace Fy::Core::Scripting
