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
enum ExprType : int
{
    // Do not change
    // These values correspond to the order in ExpressionValue
    Literal  = 0,
    Variable = 1,
    Function = 2,
    Null     = 3
};

struct Expression;

struct LiteralValue
{
    QString value;
};
struct VarValue
{
    QString value;
};
struct FuncValue
{
    QString name;
    std::vector<Expression> args;
};

using ExpressionValue = std::variant<LiteralValue, VarValue, FuncValue>;

struct Expression
{
    Expression() = default;
    Expression(ExprType type)
        : type{type} {};
    ExprType type{Null};
    ExpressionValue value;
};
} // namespace Fy::Core::Scripting
